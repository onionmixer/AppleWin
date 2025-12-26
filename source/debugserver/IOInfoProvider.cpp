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

#include "StdAfx.h"

#include "IOInfoProvider.h"
#include "JsonBuilder.h"
#include "SimpleTemplate.h"

// AppleWin includes
#include "Memory.h"
#include "CardManager.h"

namespace debugserver {

void IOInfoProvider::HandleRequest(const HttpRequest& request, HttpResponse& response) {
    std::string path = request.GetPath();

    if (path == "/api/softswitches" || path == "/softswitches") {
        HandleApiSoftSwitches(request, response);
    }
    else if (path == "/api/slots" || path == "/slots") {
        HandleApiSlots(request, response);
    }
    else if (path == "/api/annunciators" || path == "/annunciators") {
        HandleApiAnnunciators(request, response);
    }
    else if (path == "/" || path == "/index.html") {
        HandleHtmlDashboard(request, response);
    }
    else {
        SendErrorResponse(response, 404, "Endpoint not found: " + path);
    }
}

void IOInfoProvider::HandleApiSoftSwitches(const HttpRequest& request, HttpResponse& response) {
    auto switches = GetSoftSwitchStates();

    JsonBuilder json;
    json.BeginObject()
        .Add("memMode", static_cast<unsigned int>(GetMemMode()))
        .Key("switches").BeginArray();

    for (const auto& sw : switches) {
        json.BeginObject()
            .AddHex16("address", sw.address)
            .Add("name", sw.name)
            .Add("description", sw.description)
            .Add("state", sw.state)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void IOInfoProvider::HandleApiSlots(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .Key("slots").BeginArray();

    // Get card manager and enumerate slots
    CardManager& cardMgr = ::GetCardMgr();

    for (int slot = 0; slot <= 7; slot++) {
        SS_CARDTYPE cardType = cardMgr.QuerySlot(slot);
        json.BeginObject()
            .Add("slot", slot);

        if (cardType != CT_Empty) {
            json.Add("type", Card::GetCardName(cardType))
                .Add("active", true);
        } else {
            json.Add("type", "Empty")
                .Add("active", false);
        }

        json.EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void IOInfoProvider::HandleApiAnnunciators(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .Key("annunciators").BeginArray();

    for (int i = 0; i < 4; i++) {
        json.BeginObject()
            .Add("index", i)
            .Add("state", MemGetAnnunciator(i))
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void IOInfoProvider::HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response) {
    SimpleTemplate tpl;

    std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>AppleWin Debug - I/O Info</title>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="1">
    <style>
        body {
            font-family: 'Courier New', monospace;
            background: #1e1e2e;
            color: #cdd6f4;
            padding: 20px;
            margin: 0;
        }
        h1 { color: #89b4fa; border-bottom: 2px solid #45475a; padding-bottom: 10px; }
        h2 { color: #a6e3a1; margin-top: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(400px, 1fr)); gap: 20px; }
        .info-box {
            background: #313244;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #89b4fa;
        }
        .switch-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 5px; }
        .switch-item {
            display: flex;
            justify-content: space-between;
            padding: 5px 10px;
            background: #45475a;
            border-radius: 4px;
        }
        .switch-name { color: #94a3b8; }
        .switch-on { color: #a6e3a1; font-weight: bold; }
        .switch-off { color: #6c7086; }
        .nav { margin-bottom: 20px; }
        .nav a {
            color: #89b4fa;
            text-decoration: none;
            margin-right: 15px;
            padding: 5px 10px;
            background: #45475a;
            border-radius: 4px;
        }
        .nav a:hover { background: #585b70; }
        table { width: 100%; border-collapse: collapse; }
        th, td { padding: 8px 10px; text-align: left; }
        th { color: #94a3b8; border-bottom: 1px solid #45475a; }
        .slot-empty { color: #6c7086; }
        .slot-active { color: #a6e3a1; }
        .ann-on { color: #a6e3a1; font-weight: bold; }
        .ann-off { color: #6c7086; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AppleWin Debug Server - I/O Info</h1>
        <div class="nav">
            <a href="http://localhost:65501/">Machine Info</a>
            <a href="/">I/O Info</a>
            <a href="http://localhost:65503/">CPU Info</a>
            <a href="http://localhost:65504/">Memory Info</a>
            <a href="/api/softswitches">API: Soft Switches</a>
            <a href="/api/slots">API: Slots</a>
            <a href="/api/annunciators">API: Annunciators</a>
        </div>

        <div class="grid">
            <div class="info-box">
                <h2>Soft Switches</h2>
                <div class="switch-grid">
{{#switches}}
                    <div class="switch-item">
                        <span class="switch-name">{{name}}</span>
                        <span class="{{stateClass}}">{{stateText}}</span>
                    </div>
{{/switches}}
                </div>
            </div>

            <div class="info-box">
                <h2>Expansion Slots</h2>
                <table>
                    <tr>
                        <th>Slot</th>
                        <th>Card Type</th>
                        <th>Status</th>
                    </tr>
{{#slots}}
                    <tr>
                        <td>{{slot}}</td>
                        <td>{{type}}</td>
                        <td class="{{statusClass}}">{{status}}</td>
                    </tr>
{{/slots}}
                </table>
            </div>
        </div>

        <div class="info-box" style="margin-top: 20px;">
            <h2>Annunciators</h2>
            <div style="display: flex; gap: 20px;">
{{#annunciators}}
                <div style="text-align: center;">
                    <div style="color: #94a3b8;">ANN{{index}}</div>
                    <div class="{{stateClass}}" style="font-size: 1.2em;">{{stateText}}</div>
                </div>
{{/annunciators}}
            </div>
        </div>
    </div>
</body>
</html>)HTML";

    tpl.LoadFromString(html);

    // Soft switches
    auto switches = GetSoftSwitchStates();
    SimpleTemplate::ArrayData switchArray;
    for (const auto& sw : switches) {
        SimpleTemplate::VariableMap item;
        item["name"] = sw.name;
        item["stateText"] = sw.state ? "ON" : "OFF";
        item["stateClass"] = sw.state ? "switch-on" : "switch-off";
        switchArray.push_back(item);
    }
    tpl.SetArray("switches", switchArray);

    // Slots
    CardManager& cardMgr = ::GetCardMgr();

    SimpleTemplate::ArrayData slotArray;
    for (int slot = 0; slot <= 7; slot++) {
        SimpleTemplate::VariableMap item;
        item["slot"] = std::to_string(slot);

        SS_CARDTYPE cardType = cardMgr.QuerySlot(slot);
        if (cardType != CT_Empty) {
            item["type"] = Card::GetCardName(cardType);
            item["status"] = "Active";
            item["statusClass"] = "slot-active";
        } else {
            item["type"] = "Empty";
            item["status"] = "-";
            item["statusClass"] = "slot-empty";
        }
        slotArray.push_back(item);
    }
    tpl.SetArray("slots", slotArray);

    // Annunciators
    SimpleTemplate::ArrayData annArray;
    for (int i = 0; i < 4; i++) {
        SimpleTemplate::VariableMap item;
        item["index"] = std::to_string(i);
        bool state = MemGetAnnunciator(i);
        item["stateText"] = state ? "ON" : "OFF";
        item["stateClass"] = state ? "ann-on" : "ann-off";
        annArray.push_back(item);
    }
    tpl.SetArray("annunciators", annArray);

    SendHtmlResponse(response, tpl.Render());
}

std::vector<IOInfoProvider::SoftSwitch> IOInfoProvider::GetSoftSwitchStates() const {
    uint32_t memMode = GetMemMode();

    std::vector<SoftSwitch> switches = {
        { 0xC000, "80STORE", "80-column store", (memMode & MF_80STORE) != 0 },
        { 0xC002, "RAMRD", "Aux RAM read", (memMode & MF_AUXREAD) != 0 },
        { 0xC004, "RAMWRT", "Aux RAM write", (memMode & MF_AUXWRITE) != 0 },
        { 0xC008, "ALTZP", "Alt zero page", (memMode & MF_ALTZP) != 0 },
        { 0xC00C, "80COL", "80-column mode", false }, // Would need video state
        { 0xC00E, "ALTCHARSET", "Alt character set", false }, // Would need video state
        { 0xC050, "TEXT", "Text mode", false }, // Would need video state
        { 0xC052, "MIXED", "Mixed mode", false }, // Would need video state
        { 0xC054, "PAGE2", "Page 2 display", (memMode & MF_PAGE2) != 0 },
        { 0xC056, "HIRES", "Hi-res mode", (memMode & MF_HIRES) != 0 },
        { 0xC080, "LCRAM", "Language card RAM", (memMode & MF_HIGHRAM) != 0 },
        { 0xC081, "LCBANK2", "LC Bank 2", (memMode & MF_BANK2) != 0 },
        { 0xC083, "LCWRITE", "LC write enable", (memMode & MF_WRITERAM) != 0 },
        { 0xC00A, "INTCXROM", "Internal CX ROM", MemCheckINTCXROM() },
        { 0xC017, "SLOTC3ROM", "Slot C3 ROM", MemCheckSLOTC3ROM() },
    };

    return switches;
}

} // namespace debugserver
