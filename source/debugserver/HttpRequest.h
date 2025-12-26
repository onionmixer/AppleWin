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
 * HTTP Request Parser
 * Internal implementation - no external library dependencies
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>

namespace debugserver {

class HttpRequest {
public:
    HttpRequest() = default;
    ~HttpRequest() = default;

    // Parse raw HTTP request data
    bool Parse(const char* data, size_t length);
    bool Parse(const std::string& data);

    // Getters
    const std::string& GetMethod() const { return m_method; }
    const std::string& GetPath() const { return m_path; }
    const std::string& GetQuery() const { return m_query; }
    const std::string& GetVersion() const { return m_version; }
    const std::string& GetBody() const { return m_body; }

    // Query parameters
    std::string GetQueryParam(const std::string& key,
                               const std::string& defaultValue = "") const;
    bool HasQueryParam(const std::string& key) const;
    const std::map<std::string, std::string>& GetQueryParams() const { return m_queryParams; }

    // Headers
    std::string GetHeader(const std::string& key,
                          const std::string& defaultValue = "") const;
    bool HasHeader(const std::string& key) const;
    const std::map<std::string, std::string>& GetHeaders() const { return m_headers; }

    // URL decoding utility
    static std::string UrlDecode(const std::string& str);

    // Reset for reuse
    void Clear();

private:
    bool ParseRequestLine(const std::string& line);
    void ParseHeaders(const std::string& headerSection);
    void ParseQueryString(const std::string& queryString);

    static std::string Trim(const std::string& str);
    static std::string ToLower(const std::string& str);

    std::string m_method;
    std::string m_path;
    std::string m_query;
    std::string m_version;
    std::string m_body;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_queryParams;
};

} // namespace debugserver
