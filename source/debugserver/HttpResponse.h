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
 * HTTP Response Builder
 * Internal implementation - no external library dependencies
 */

#pragma once

#include <string>
#include <map>
#include <sstream>

namespace debugserver {

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse() = default;

    // Status code
    void SetStatus(int code, const std::string& reason = "");
    int GetStatus() const { return m_statusCode; }

    // Headers
    void SetHeader(const std::string& key, const std::string& value);
    void SetContentType(const std::string& contentType);
    void SetContentLength(size_t length);

    // Common headers
    void EnableCORS(const std::string& origin = "*");
    void SetCacheControl(const std::string& directive);
    void DisableCache();

    // Body
    void SetBody(const std::string& body);
    void AppendBody(const std::string& data);
    const std::string& GetBody() const { return m_body; }

    // Convenience methods for common content types
    void SendHTML(const std::string& html);
    void SendJSON(const std::string& json);
    void SendText(const std::string& text);
    void SendError(int code, const std::string& message);

    // Redirect
    void Redirect(const std::string& url, int code = 302);

    // Build the complete HTTP response
    std::string Build() const;

    // Reset for reuse
    void Clear();

private:
    static std::string GetStatusText(int code);
    static std::string GetCurrentTimestamp();

    int m_statusCode;
    std::string m_statusReason;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};

} // namespace debugserver
