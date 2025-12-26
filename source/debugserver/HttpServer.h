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

/*
 * HTTP Server
 * Internal implementation using POSIX sockets (Linux/macOS) or Winsock (Windows)
 * No external library dependencies
 */

#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

// Platform-specific socket includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
    #pragma comment(lib, "ws2_32.lib")
    #endif
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <poll.h>
#endif

namespace debugserver {

// Socket type abstraction
#ifdef _WIN32
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCKET_VALUE = INVALID_SOCKET;
    constexpr int SOCKET_ERROR_VALUE = SOCKET_ERROR;
    inline int CloseSocket(socket_t s) { return closesocket(s); }
    inline int GetLastSocketError() { return WSAGetLastError(); }
#else
    using socket_t = int;
    constexpr socket_t INVALID_SOCKET_VALUE = -1;
    constexpr int SOCKET_ERROR_VALUE = -1;
    inline int CloseSocket(socket_t s) { return close(s); }
    inline int GetLastSocketError() { return errno; }
#endif

class HttpServer {
public:
    // Request handler callback type
    using RequestHandler = std::function<void(const HttpRequest& request, HttpResponse& response)>;

    // Constructor with port and optional bind address
    explicit HttpServer(uint16_t port, const std::string& bindAddress = "127.0.0.1");
    ~HttpServer();

    // Non-copyable
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    // Set the request handler
    void SetHandler(RequestHandler handler);

    // Start the server
    bool Start();

    // Stop the server
    void Stop();

    // Check if server is running
    bool IsRunning() const { return m_running.load(); }

    // Get the port
    uint16_t GetPort() const { return m_port; }

    // Get the bind address
    const std::string& GetBindAddress() const { return m_bindAddress; }

    // Get last error message
    const std::string& GetLastError() const { return m_lastError; }

    // Configuration
    void SetMaxRequestSize(size_t size) { m_maxRequestSize = size; }
    void SetReadTimeout(int milliseconds) { m_readTimeoutMs = milliseconds; }

private:
    // Main accept loop (runs in separate thread)
    void AcceptLoop();

    // Handle a single client connection
    void HandleClient(socket_t clientSocket);

    // Socket operations
    bool InitSocket();
    void CleanupSocket();
    bool SetSocketNonBlocking(socket_t sock);
    bool SetSocketReuseAddr(socket_t sock);

    // Read data from socket with timeout
    std::string ReadRequest(socket_t clientSocket);

    // Send response to socket
    bool SendResponse(socket_t clientSocket, const std::string& response);

#ifdef _WIN32
    // Windows socket initialization
    static bool InitWinsock();
    static void CleanupWinsock();
    static bool s_wsaInitialized;
    static int s_wsaRefCount;
    static std::mutex s_wsaMutex;
#endif

    uint16_t m_port;
    std::string m_bindAddress;
    socket_t m_serverSocket;
    RequestHandler m_handler;
    std::thread m_acceptThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shouldStop;
    std::mutex m_mutex;
    std::string m_lastError;

    // Configuration
    size_t m_maxRequestSize;
    int m_readTimeoutMs;

    // Constants
    static constexpr size_t DEFAULT_MAX_REQUEST_SIZE = 64 * 1024;  // 64KB
    static constexpr int DEFAULT_READ_TIMEOUT_MS = 5000;           // 5 seconds
    static constexpr int LISTEN_BACKLOG = 10;
    static constexpr size_t READ_BUFFER_SIZE = 4096;
};

} // namespace debugserver
