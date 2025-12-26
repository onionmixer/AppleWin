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
 * CPU Information Provider
 * Provides CPU state, registers, flags, and breakpoints via HTTP
 * Port: 65503
 */

#pragma once

#include "InfoProvider.h"
#include <vector>

namespace debugserver {

class CPUInfoProvider : public InfoProvider {
public:
    CPUInfoProvider() = default;
    ~CPUInfoProvider() override = default;

    const char* GetName() const override { return "CPUInfo"; }
    uint16_t GetPort() const override { return static_cast<uint16_t>(DebugServerPort::CPU); }

    void HandleRequest(const HttpRequest& request, HttpResponse& response) override;

private:
    // API endpoints
    void HandleApiRegisters(const HttpRequest& request, HttpResponse& response);
    void HandleApiFlags(const HttpRequest& request, HttpResponse& response);
    void HandleApiBreakpoints(const HttpRequest& request, HttpResponse& response);
    void HandleApiDisassembly(const HttpRequest& request, HttpResponse& response);
    void HandleApiStack(const HttpRequest& request, HttpResponse& response);
    void HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response);

    // Disassembly helper
    struct DisasmLine {
        uint16_t address;
        std::string addressHex;
        std::string bytes;
        std::string mnemonic;
        std::string operand;
        bool isCurrentPC;
        bool hasBreakpoint;
    };

    std::vector<DisasmLine> GetDisassembly(uint16_t startAddr, int lines) const;

    // Flag name helpers
    static const char* GetFlagName(int bit);
    static char GetFlagChar(int bit);
};

} // namespace debugserver
