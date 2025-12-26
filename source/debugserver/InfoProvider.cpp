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

#include "InfoProvider.h"
#include <cstdio>

namespace debugserver {

void InfoProvider::SendJsonResponse(HttpResponse& response, const std::string& json) {
    response.SendJSON(json);
}

void InfoProvider::SendHtmlResponse(HttpResponse& response, const std::string& html) {
    response.SendHTML(html);
}

void InfoProvider::SendErrorResponse(HttpResponse& response, int code, const std::string& message) {
    response.SendError(code, message);
}

std::string InfoProvider::ToHex8(uint8_t value) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02X", value);
    return buf;
}

std::string InfoProvider::ToHex16(uint16_t value) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%04X", value);
    return buf;
}

std::string InfoProvider::ToHex8Prefixed(uint8_t value) {
    return "$" + ToHex8(value);
}

std::string InfoProvider::ToHex16Prefixed(uint16_t value) {
    return "$" + ToHex16(value);
}

} // namespace debugserver
