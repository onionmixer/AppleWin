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
 * I/O Information Provider
 * Provides soft switch states and slot information via HTTP
 * Port: 65502
 */

#pragma once

#include "InfoProvider.h"

namespace debugserver {

class IOInfoProvider : public InfoProvider {
public:
    IOInfoProvider() = default;
    ~IOInfoProvider() override = default;

    const char* GetName() const override { return "IOInfo"; }
    uint16_t GetPort() const override { return static_cast<uint16_t>(DebugServerPort::IO); }

    void HandleRequest(const HttpRequest& request, HttpResponse& response) override;

private:
    // API endpoints
    void HandleApiSoftSwitches(const HttpRequest& request, HttpResponse& response);
    void HandleApiSlots(const HttpRequest& request, HttpResponse& response);
    void HandleApiAnnunciators(const HttpRequest& request, HttpResponse& response);
    void HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response);

    // Soft switch state descriptions
    struct SoftSwitch {
        uint16_t address;
        const char* name;
        const char* description;
        bool state;
    };

    std::vector<SoftSwitch> GetSoftSwitchStates() const;
};

} // namespace debugserver
