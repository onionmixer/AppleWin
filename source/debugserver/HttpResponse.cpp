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

#include "HttpResponse.h"
#include <ctime>
#include <iomanip>

namespace debugserver {

HttpResponse::HttpResponse()
    : m_statusCode(200)
    , m_statusReason("OK")
{
    // Set default headers
    SetHeader("Server", "AppleWin-DebugServer/1.0");
    SetHeader("Connection", "close");
}

void HttpResponse::SetStatus(int code, const std::string& reason) {
    m_statusCode = code;
    m_statusReason = reason.empty() ? GetStatusText(code) : reason;
}

void HttpResponse::SetHeader(const std::string& key, const std::string& value) {
    m_headers[key] = value;
}

void HttpResponse::SetContentType(const std::string& contentType) {
    SetHeader("Content-Type", contentType);
}

void HttpResponse::SetContentLength(size_t length) {
    SetHeader("Content-Length", std::to_string(length));
}

void HttpResponse::EnableCORS(const std::string& origin) {
    SetHeader("Access-Control-Allow-Origin", origin);
    SetHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    SetHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
}

void HttpResponse::SetCacheControl(const std::string& directive) {
    SetHeader("Cache-Control", directive);
}

void HttpResponse::DisableCache() {
    SetCacheControl("no-cache, no-store, must-revalidate");
    SetHeader("Pragma", "no-cache");
    SetHeader("Expires", "0");
}

void HttpResponse::SetBody(const std::string& body) {
    m_body = body;
}

void HttpResponse::AppendBody(const std::string& data) {
    m_body += data;
}

void HttpResponse::SendHTML(const std::string& html) {
    SetContentType("text/html; charset=utf-8");
    SetBody(html);
}

void HttpResponse::SendJSON(const std::string& json) {
    SetContentType("application/json; charset=utf-8");
    SetBody(json);
}

void HttpResponse::SendText(const std::string& text) {
    SetContentType("text/plain; charset=utf-8");
    SetBody(text);
}

void HttpResponse::SendError(int code, const std::string& message) {
    SetStatus(code);
    SetContentType("text/html; charset=utf-8");

    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>" << code << " " << GetStatusText(code) << "</title></head>\n"
         << "<body>\n"
         << "<h1>" << code << " " << GetStatusText(code) << "</h1>\n"
         << "<p>" << message << "</p>\n"
         << "<hr>\n"
         << "<p><small>AppleWin Debug Server</small></p>\n"
         << "</body>\n"
         << "</html>\n";

    SetBody(html.str());
}

void HttpResponse::Redirect(const std::string& url, int code) {
    SetStatus(code);
    SetHeader("Location", url);
    SetContentType("text/html; charset=utf-8");

    std::ostringstream html;
    html << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>Redirect</title></head>\n"
         << "<body>\n"
         << "<p>Redirecting to <a href=\"" << url << "\">" << url << "</a></p>\n"
         << "</body>\n"
         << "</html>\n";

    SetBody(html.str());
}

std::string HttpResponse::Build() const {
    std::ostringstream response;

    // Status line
    response << "HTTP/1.1 " << m_statusCode << " " << m_statusReason << "\r\n";

    // Date header
    response << "Date: " << GetCurrentTimestamp() << "\r\n";

    // Content-Length (if body is present and not already set)
    if (!m_body.empty() && m_headers.find("Content-Length") == m_headers.end()) {
        response << "Content-Length: " << m_body.length() << "\r\n";
    }

    // Other headers
    for (const auto& header : m_headers) {
        response << header.first << ": " << header.second << "\r\n";
    }

    // End of headers
    response << "\r\n";

    // Body
    response << m_body;

    return response.str();
}

void HttpResponse::Clear() {
    m_statusCode = 200;
    m_statusReason = "OK";
    m_headers.clear();
    m_body.clear();

    // Reset default headers
    SetHeader("Server", "AppleWin-DebugServer/1.0");
    SetHeader("Connection", "close");
}

std::string HttpResponse::GetStatusText(int code) {
    switch (code) {
        // 1xx Informational
        case 100: return "Continue";
        case 101: return "Switching Protocols";

        // 2xx Success
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";

        // 3xx Redirection
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";

        // 4xx Client Error
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 429: return "Too Many Requests";

        // 5xx Server Error
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";

        default: return "Unknown";
    }
}

std::string HttpResponse::GetCurrentTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);

    char buffer[64];
    // RFC 7231 date format: Sun, 06 Nov 1994 08:49:37 GMT
    std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);

    return buffer;
}

} // namespace debugserver
