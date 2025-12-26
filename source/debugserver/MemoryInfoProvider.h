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
 * Memory Information Provider
 * Provides memory dumps and memory state information via HTTP
 * Port: 65504
 */

#pragma once

#include "InfoProvider.h"
#include <vector>

namespace debugserver {

class MemoryInfoProvider : public InfoProvider {
public:
    MemoryInfoProvider() = default;
    ~MemoryInfoProvider() override = default;

    const char* GetName() const override { return "MemoryInfo"; }
    uint16_t GetPort() const override { return static_cast<uint16_t>(DebugServerPort::Memory); }

    void HandleRequest(const HttpRequest& request, HttpResponse& response) override;

private:
    // API endpoints
    void HandleApiDump(const HttpRequest& request, HttpResponse& response);
    void HandleApiRead(const HttpRequest& request, HttpResponse& response);
    void HandleApiZeroPage(const HttpRequest& request, HttpResponse& response);
    void HandleApiStack(const HttpRequest& request, HttpResponse& response);
    void HandleApiTextScreen(const HttpRequest& request, HttpResponse& response);
    void HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response);

    // Helper structure for hex dump
    struct HexDumpLine {
        uint16_t address;
        std::string addressHex;
        std::vector<uint8_t> bytes;
        std::string bytesHex;
        std::string ascii;
    };

    std::vector<HexDumpLine> GetHexDump(uint16_t startAddr, int lines, int bytesPerLine = 16) const;

    // Convert byte to printable ASCII character
    static char ToPrintable(uint8_t byte);
};

} // namespace debugserver
