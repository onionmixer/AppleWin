/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2024, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "HttpServer.h"
#include <cstring>
#include <chrono>

namespace debugserver {

#ifdef _WIN32
bool HttpServer::s_wsaInitialized = false;
int HttpServer::s_wsaRefCount = 0;
std::mutex HttpServer::s_wsaMutex;

bool HttpServer::InitWinsock() {
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    if (!s_wsaInitialized) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            return false;
        }
        s_wsaInitialized = true;
    }
    s_wsaRefCount++;
    return true;
}

void HttpServer::CleanupWinsock() {
    std::lock_guard<std::mutex> lock(s_wsaMutex);
    if (s_wsaRefCount > 0) {
        s_wsaRefCount--;
        if (s_wsaRefCount == 0 && s_wsaInitialized) {
            WSACleanup();
            s_wsaInitialized = false;
        }
    }
}
#endif

HttpServer::HttpServer(uint16_t port, const std::string& bindAddress)
    : m_port(port)
    , m_bindAddress(bindAddress)
    , m_serverSocket(INVALID_SOCKET_VALUE)
    , m_running(false)
    , m_shouldStop(false)
    , m_maxRequestSize(DEFAULT_MAX_REQUEST_SIZE)
    , m_readTimeoutMs(DEFAULT_READ_TIMEOUT_MS)
{
#ifdef _WIN32
    InitWinsock();
#endif
}

HttpServer::~HttpServer() {
    Stop();
#ifdef _WIN32
    CleanupWinsock();
#endif
}

void HttpServer::SetHandler(RequestHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = std::move(handler);
}

bool HttpServer::Start() {
    if (m_running.load()) {
        m_lastError = "Server is already running";
        return false;
    }

    if (!InitSocket()) {
        return false;
    }

    m_shouldStop = false;
    m_running = true;

    // Start accept thread
    m_acceptThread = std::thread(&HttpServer::AcceptLoop, this);

    return true;
}

void HttpServer::Stop() {
    if (!m_running.load()) {
        return;
    }

    m_shouldStop = true;

    // Close the server socket to interrupt accept()
    CleanupSocket();

    // Wait for accept thread to finish
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }

    m_running = false;
}

bool HttpServer::InitSocket() {
    // Create socket
    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET_VALUE) {
        m_lastError = "Failed to create socket: " + std::to_string(GetLastSocketError());
        return false;
    }

    // Set socket options
    if (!SetSocketReuseAddr(m_serverSocket)) {
        CleanupSocket();
        return false;
    }

    // Bind to address
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);

    if (m_bindAddress == "0.0.0.0" || m_bindAddress.empty()) {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, m_bindAddress.c_str(), &serverAddr.sin_addr) != 1) {
            m_lastError = "Invalid bind address: " + m_bindAddress;
            CleanupSocket();
            return false;
        }
    }

    if (bind(m_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR_VALUE) {
        m_lastError = "Failed to bind socket: " + std::to_string(GetLastSocketError());
        CleanupSocket();
        return false;
    }

    // Start listening
    if (listen(m_serverSocket, LISTEN_BACKLOG) == SOCKET_ERROR_VALUE) {
        m_lastError = "Failed to listen on socket: " + std::to_string(GetLastSocketError());
        CleanupSocket();
        return false;
    }

    return true;
}

void HttpServer::CleanupSocket() {
    if (m_serverSocket != INVALID_SOCKET_VALUE) {
        CloseSocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET_VALUE;
    }
}

bool HttpServer::SetSocketNonBlocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool HttpServer::SetSocketReuseAddr(socket_t sock) {
    int optval = 1;
#ifdef _WIN32
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                      (const char*)&optval, sizeof(optval)) == 0;
#else
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                      &optval, sizeof(optval)) == 0;
#endif
}

void HttpServer::AcceptLoop() {
    while (!m_shouldStop.load()) {
        // Use poll/select with timeout to allow checking m_shouldStop
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_serverSocket, &readfds);

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout

        int selectResult = select(0, &readfds, nullptr, nullptr, &tv);
        if (selectResult == SOCKET_ERROR_VALUE) {
            if (!m_shouldStop.load()) {
                m_lastError = "Select error: " + std::to_string(GetLastSocketError());
            }
            break;
        }
        if (selectResult == 0) {
            // Timeout, check m_shouldStop and continue
            continue;
        }
#else
        struct pollfd pfd;
        pfd.fd = m_serverSocket;
        pfd.events = POLLIN;

        int pollResult = poll(&pfd, 1, 100);  // 100ms timeout
        if (pollResult == -1) {
            if (!m_shouldStop.load() && errno != EINTR) {
                m_lastError = "Poll error: " + std::to_string(errno);
            }
            break;
        }
        if (pollResult == 0) {
            // Timeout, check m_shouldStop and continue
            continue;
        }
#endif

        // Accept incoming connection
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        socket_t clientSocket = accept(m_serverSocket,
                                        (struct sockaddr*)&clientAddr,
                                        &clientAddrLen);

        if (clientSocket == INVALID_SOCKET_VALUE) {
            if (!m_shouldStop.load()) {
                int err = GetLastSocketError();
#ifdef _WIN32
                if (err != WSAEINTR && err != WSAEWOULDBLOCK) {
#else
                if (err != EINTR && err != EAGAIN && err != EWOULDBLOCK) {
#endif
                    m_lastError = "Accept error: " + std::to_string(err);
                }
            }
            continue;
        }

        // Handle client in the same thread (simple synchronous handling)
        // For production use, this should be handled in a thread pool
        HandleClient(clientSocket);
    }
}

void HttpServer::HandleClient(socket_t clientSocket) {
    // Read the request
    std::string requestData = ReadRequest(clientSocket);

    if (requestData.empty()) {
        CloseSocket(clientSocket);
        return;
    }

    // Parse the request
    HttpRequest request;
    if (!request.Parse(requestData)) {
        // Send 400 Bad Request
        HttpResponse response;
        response.SendError(400, "Invalid HTTP request");
        SendResponse(clientSocket, response.Build());
        CloseSocket(clientSocket);
        return;
    }

    // Handle OPTIONS request for CORS preflight
    if (request.GetMethod() == "OPTIONS") {
        HttpResponse response;
        response.SetStatus(204);
        response.EnableCORS();
        response.SetHeader("Access-Control-Max-Age", "86400");
        SendResponse(clientSocket, response.Build());
        CloseSocket(clientSocket);
        return;
    }

    // Create response and call handler
    HttpResponse response;
    response.EnableCORS();
    response.DisableCache();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_handler) {
            try {
                m_handler(request, response);
            } catch (const std::exception& e) {
                response.SendError(500, std::string("Internal error: ") + e.what());
            } catch (...) {
                response.SendError(500, "Unknown internal error");
            }
        } else {
            response.SendError(503, "No handler configured");
        }
    }

    // Send response
    SendResponse(clientSocket, response.Build());
    CloseSocket(clientSocket);
}

std::string HttpServer::ReadRequest(socket_t clientSocket) {
    std::string data;
    char buffer[READ_BUFFER_SIZE];

    auto startTime = std::chrono::steady_clock::now();

    while (data.size() < m_maxRequestSize) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= m_readTimeoutMs) {
            break;
        }

        // Use poll/select for timeout
#ifdef _WIN32
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);

        struct timeval tv;
        int remainingMs = m_readTimeoutMs - static_cast<int>(elapsed);
        tv.tv_sec = remainingMs / 1000;
        tv.tv_usec = (remainingMs % 1000) * 1000;

        int selectResult = select(0, &readfds, nullptr, nullptr, &tv);
        if (selectResult <= 0) {
            break;
        }
#else
        struct pollfd pfd;
        pfd.fd = clientSocket;
        pfd.events = POLLIN;

        int remainingMs = m_readTimeoutMs - static_cast<int>(elapsed);
        int pollResult = poll(&pfd, 1, remainingMs);
        if (pollResult <= 0) {
            break;
        }
#endif

        // Read data
#ifdef _WIN32
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
#else
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
#endif

        if (bytesRead > 0) {
            data.append(buffer, bytesRead);

            // Check if we have a complete HTTP request
            // Look for double CRLF which marks end of headers
            size_t headerEnd = data.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                // Check if there's a Content-Length header
                size_t clPos = data.find("Content-Length:");
                if (clPos == std::string::npos) {
                    clPos = data.find("content-length:");
                }

                if (clPos != std::string::npos && clPos < headerEnd) {
                    // Parse Content-Length
                    size_t valueStart = clPos + 15;  // Length of "Content-Length:"
                    while (valueStart < data.size() &&
                           (data[valueStart] == ' ' || data[valueStart] == '\t')) {
                        valueStart++;
                    }
                    size_t valueEnd = data.find("\r\n", valueStart);
                    if (valueEnd != std::string::npos) {
                        size_t contentLength = std::stoul(data.substr(valueStart, valueEnd - valueStart));
                        size_t expectedLength = headerEnd + 4 + contentLength;
                        if (data.size() >= expectedLength) {
                            break;  // Complete request received
                        }
                    }
                } else {
                    // No Content-Length, assume headers only (GET request)
                    break;
                }
            }
        } else if (bytesRead == 0) {
            // Connection closed by client
            break;
        } else {
            // Error
            int err = GetLastSocketError();
#ifdef _WIN32
            if (err != WSAEWOULDBLOCK) {
#else
            if (err != EAGAIN && err != EWOULDBLOCK) {
#endif
                break;
            }
        }
    }

    return data;
}

bool HttpServer::SendResponse(socket_t clientSocket, const std::string& response) {
    size_t totalSent = 0;
    const char* data = response.c_str();
    size_t remaining = response.size();

    while (remaining > 0) {
#ifdef _WIN32
        int sent = send(clientSocket, data + totalSent, static_cast<int>(remaining), 0);
#else
        ssize_t sent = send(clientSocket, data + totalSent, remaining, 0);
#endif

        if (sent > 0) {
            totalSent += sent;
            remaining -= sent;
        } else if (sent == 0) {
            return false;
        } else {
            int err = GetLastSocketError();
#ifdef _WIN32
            if (err == WSAEWOULDBLOCK) {
#else
            if (err == EAGAIN || err == EWOULDBLOCK) {
#endif
                // Would block, retry
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            return false;
        }
    }

    return true;
}

} // namespace debugserver
