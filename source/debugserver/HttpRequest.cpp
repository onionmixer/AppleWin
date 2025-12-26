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

#include "HttpRequest.h"
#include <sstream>

namespace debugserver {

bool HttpRequest::Parse(const char* data, size_t length) {
    return Parse(std::string(data, length));
}

bool HttpRequest::Parse(const std::string& data) {
    Clear();

    if (data.empty()) {
        return false;
    }

    // Find the end of headers (double CRLF)
    size_t headerEnd = data.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        // Try with just LF (some clients might send this)
        headerEnd = data.find("\n\n");
        if (headerEnd == std::string::npos) {
            // No complete header section, try to parse what we have
            headerEnd = data.length();
        }
    }

    std::string headerSection = data.substr(0, headerEnd);

    // Find the first line (request line)
    size_t firstLineEnd = headerSection.find("\r\n");
    if (firstLineEnd == std::string::npos) {
        firstLineEnd = headerSection.find("\n");
        if (firstLineEnd == std::string::npos) {
            firstLineEnd = headerSection.length();
        }
    }

    std::string requestLine = headerSection.substr(0, firstLineEnd);
    if (!ParseRequestLine(requestLine)) {
        return false;
    }

    // Parse headers
    if (firstLineEnd < headerSection.length()) {
        size_t headersStart = firstLineEnd;
        // Skip CRLF or LF
        if (headerSection[headersStart] == '\r') headersStart++;
        if (headersStart < headerSection.length() && headerSection[headersStart] == '\n') headersStart++;

        ParseHeaders(headerSection.substr(headersStart));
    }

    // Extract body if present
    if (headerEnd != std::string::npos && headerEnd + 4 <= data.length()) {
        // Skip \r\n\r\n
        size_t bodyStart = headerEnd + 4;
        if (data[headerEnd] == '\n') {
            // Was \n\n format
            bodyStart = headerEnd + 2;
        }
        if (bodyStart < data.length()) {
            m_body = data.substr(bodyStart);
        }
    }

    return true;
}

bool HttpRequest::ParseRequestLine(const std::string& line) {
    // Format: METHOD PATH HTTP/VERSION
    // Example: GET /path?query=value HTTP/1.1

    std::istringstream iss(line);
    std::string pathWithQuery;

    if (!(iss >> m_method >> pathWithQuery >> m_version)) {
        // Try without version (HTTP/0.9)
        std::istringstream iss2(line);
        if (!(iss2 >> m_method >> pathWithQuery)) {
            return false;
        }
        m_version = "HTTP/0.9";
    }

    // Separate path and query string
    size_t queryPos = pathWithQuery.find('?');
    if (queryPos != std::string::npos) {
        m_path = pathWithQuery.substr(0, queryPos);
        m_query = pathWithQuery.substr(queryPos + 1);
        ParseQueryString(m_query);
    } else {
        m_path = pathWithQuery;
    }

    // URL decode the path
    m_path = UrlDecode(m_path);

    return !m_method.empty() && !m_path.empty();
}

void HttpRequest::ParseHeaders(const std::string& headerSection) {
    std::istringstream iss(headerSection);
    std::string line;

    while (std::getline(iss, line)) {
        // Remove trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        // Find the colon separator
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = Trim(line.substr(0, colonPos));
            std::string value = Trim(line.substr(colonPos + 1));

            // Store header with lowercase key for case-insensitive lookup
            m_headers[ToLower(key)] = value;
        }
    }
}

void HttpRequest::ParseQueryString(const std::string& queryString) {
    if (queryString.empty()) {
        return;
    }

    std::string query = queryString;
    size_t pos = 0;

    while (pos < query.length()) {
        // Find the next '&' or end of string
        size_t ampPos = query.find('&', pos);
        if (ampPos == std::string::npos) {
            ampPos = query.length();
        }

        std::string pair = query.substr(pos, ampPos - pos);
        if (!pair.empty()) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = UrlDecode(pair.substr(0, eqPos));
                std::string value = UrlDecode(pair.substr(eqPos + 1));
                m_queryParams[key] = value;
            } else {
                // Key without value
                m_queryParams[UrlDecode(pair)] = "";
            }
        }

        pos = ampPos + 1;
    }
}

std::string HttpRequest::GetQueryParam(const std::string& key,
                                         const std::string& defaultValue) const {
    auto it = m_queryParams.find(key);
    if (it != m_queryParams.end()) {
        return it->second;
    }
    return defaultValue;
}

bool HttpRequest::HasQueryParam(const std::string& key) const {
    return m_queryParams.find(key) != m_queryParams.end();
}

std::string HttpRequest::GetHeader(const std::string& key,
                                    const std::string& defaultValue) const {
    auto it = m_headers.find(ToLower(key));
    if (it != m_headers.end()) {
        return it->second;
    }
    return defaultValue;
}

bool HttpRequest::HasHeader(const std::string& key) const {
    return m_headers.find(ToLower(key)) != m_headers.end();
}

std::string HttpRequest::UrlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            // Decode percent-encoded character
            char hex[3] = { str[i + 1], str[i + 2], '\0' };
            char* endPtr;
            long value = strtol(hex, &endPtr, 16);
            if (endPtr == hex + 2) {
                result += static_cast<char>(value);
                i += 2;
                continue;
            }
        } else if (str[i] == '+') {
            // Plus sign represents space in query strings
            result += ' ';
            continue;
        }
        result += str[i];
    }

    return result;
}

void HttpRequest::Clear() {
    m_method.clear();
    m_path.clear();
    m_query.clear();
    m_version.clear();
    m_body.clear();
    m_headers.clear();
    m_queryParams.clear();
}

std::string HttpRequest::Trim(const std::string& str) {
    size_t start = 0;
    size_t end = str.length();

    while (start < end && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }

    return str.substr(start, end - start);
}

std::string HttpRequest::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

} // namespace debugserver
