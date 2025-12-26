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
 * Information Provider Interface
 * Base class for all debug information providers
 */

#pragma once

#include <string>
#include <functional>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace debugserver {

// Forward declarations
class JsonBuilder;
class SimpleTemplate;

/**
 * Base interface for information providers
 * Each provider handles a specific category of debug information
 */
class InfoProvider {
public:
    virtual ~InfoProvider() = default;

    // Get the provider name (for logging/identification)
    virtual const char* GetName() const = 0;

    // Get the port number this provider listens on
    virtual uint16_t GetPort() const = 0;

    // Handle an HTTP request and generate response
    virtual void HandleRequest(const HttpRequest& request, HttpResponse& response) = 0;

    // Check if the emulator is in a valid state to provide information
    virtual bool IsAvailable() const { return true; }

protected:
    // Helper to send JSON response
    void SendJsonResponse(HttpResponse& response, const std::string& json);

    // Helper to send HTML response
    void SendHtmlResponse(HttpResponse& response, const std::string& html);

    // Helper to send error response
    void SendErrorResponse(HttpResponse& response, int code, const std::string& message);

    // Format a byte as hex string (e.g., "FF")
    static std::string ToHex8(uint8_t value);

    // Format a word as hex string (e.g., "C600")
    static std::string ToHex16(uint16_t value);

    // Format a byte as hex string with prefix (e.g., "$FF")
    static std::string ToHex8Prefixed(uint8_t value);

    // Format a word as hex string with prefix (e.g., "$C600")
    static std::string ToHex16Prefixed(uint16_t value);
};

// Port assignments for debug servers
enum class DebugServerPort : uint16_t {
    Machine = 65501,    // Machine info (Apple2 type, mode, speed, etc.)
    IO      = 65502,    // I/O info (soft switches, slot cards)
    CPU     = 65503,    // CPU info (registers, flags, breakpoints)
    Memory  = 65504     // Memory info (dumps, memory flags)
};

} // namespace debugserver
