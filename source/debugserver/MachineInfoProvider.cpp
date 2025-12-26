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

#include "MachineInfoProvider.h"
#include "JsonBuilder.h"
#include "SimpleTemplate.h"

// AppleWin includes - access to emulator state
#include "Common.h"
#include "Core.h"
#include "CPU.h"
#include "Memory.h"
#include "Interface.h"

namespace debugserver {

void MachineInfoProvider::HandleRequest(const HttpRequest& request, HttpResponse& response) {
    std::string path = request.GetPath();
    std::string format = request.GetQueryParam("format", "json");

    // API routes
    if (path == "/api/status" || path == "/status") {
        HandleApiStatus(request, response);
    }
    else if (path == "/api/info" || path == "/info") {
        HandleApiInfo(request, response);
    }
    else if (path == "/" || path == "/index.html") {
        HandleHtmlDashboard(request, response);
    }
    else {
        SendErrorResponse(response, 404, "Endpoint not found: " + path);
    }
}

void MachineInfoProvider::HandleApiStatus(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .Add("server", "AppleWin Debug Server")
        .Add("provider", GetName())
        .Add("port", static_cast<int>(GetPort()))
        .Add("available", IsAvailable())
        .Add("mode", GetAppModeName())
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MachineInfoProvider::HandleApiInfo(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .Add("apple2Type", GetApple2TypeName())
        .Add("cpuType", GetCpuTypeName())
        .Add("mode", GetAppModeName())
        .Add("videoMode", GetVideoModeName())
        .Key("memory").BeginObject()
            .Add("memMode", static_cast<unsigned int>(GetMemMode()))
            .Add("80store", (GetMemMode() & MF_80STORE) != 0)
            .Add("auxRead", (GetMemMode() & MF_AUXREAD) != 0)
            .Add("auxWrite", (GetMemMode() & MF_AUXWRITE) != 0)
            .Add("altZP", (GetMemMode() & MF_ALTZP) != 0)
            .Add("highRam", (GetMemMode() & MF_HIGHRAM) != 0)
            .Add("bank2", (GetMemMode() & MF_BANK2) != 0)
            .Add("writeRam", (GetMemMode() & MF_WRITERAM) != 0)
            .Add("page2", (GetMemMode() & MF_PAGE2) != 0)
            .Add("hires", (GetMemMode() & MF_HIRES) != 0)
        .EndObject()
        .Key("cycles").BeginObject()
            .Add("cumulative", static_cast<unsigned long long>(g_nCumulativeCycles))
        .EndObject()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MachineInfoProvider::HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response) {
    SimpleTemplate tpl;

    std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>AppleWin Debug - Machine Info</title>
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
        .container { max-width: 900px; margin: 0 auto; }
        .info-box {
            background: #313244;
            padding: 15px;
            margin: 10px 0;
            border-radius: 8px;
            border-left: 4px solid #89b4fa;
        }
        .info-row { display: flex; margin: 5px 0; }
        .info-label { color: #94a3b8; width: 150px; }
        .info-value { color: #f9e2af; font-weight: bold; }
        .status-running { color: #a6e3a1; }
        .status-paused { color: #f38ba8; }
        .status-debug { color: #fab387; }
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
        .flag-on { color: #a6e3a1; }
        .flag-off { color: #6c7086; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AppleWin Debug Server - Machine Info</h1>
        <div class="nav">
            <a href="/">Dashboard</a>
            <a href="/api/info">API: Info</a>
            <a href="/api/status">API: Status</a>
            <a href="http://localhost:65502/">I/O Info</a>
            <a href="http://localhost:65503/">CPU Info</a>
            <a href="http://localhost:65504/">Memory Info</a>
        </div>

        <div class="info-box">
            <h2>System</h2>
            <div class="info-row">
                <span class="info-label">Apple II Type:</span>
                <span class="info-value">{{apple2Type}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">CPU Type:</span>
                <span class="info-value">{{cpuType}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">Mode:</span>
                <span class="info-value {{modeClass}}">{{mode}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">Video Mode:</span>
                <span class="info-value">{{videoMode}}</span>
            </div>
        </div>

        <div class="info-box">
            <h2>Memory State</h2>
            <div class="info-row">
                <span class="info-label">80STORE:</span>
                <span class="info-value {{80storeClass}}">{{80store}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">AUXREAD:</span>
                <span class="info-value {{auxReadClass}}">{{auxRead}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">AUXWRITE:</span>
                <span class="info-value {{auxWriteClass}}">{{auxWrite}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">ALTZP:</span>
                <span class="info-value {{altZPClass}}">{{altZP}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">HIGHRAM:</span>
                <span class="info-value {{highRamClass}}">{{highRam}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">BANK2:</span>
                <span class="info-value {{bank2Class}}">{{bank2}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">PAGE2:</span>
                <span class="info-value {{page2Class}}">{{page2}}</span>
            </div>
            <div class="info-row">
                <span class="info-label">HIRES:</span>
                <span class="info-value {{hiresClass}}">{{hires}}</span>
            </div>
        </div>

        <div class="info-box">
            <h2>Timing</h2>
            <div class="info-row">
                <span class="info-label">Cumulative Cycles:</span>
                <span class="info-value">{{cycles}}</span>
            </div>
        </div>
    </div>
</body>
</html>)HTML";

    tpl.LoadFromString(html);

    // Set values
    tpl.SetVariable("apple2Type", GetApple2TypeName());
    tpl.SetVariable("cpuType", GetCpuTypeName());

    std::string mode = GetAppModeName();
    tpl.SetVariable("mode", mode);
    if (mode == "Running") {
        tpl.SetVariable("modeClass", "status-running");
    } else if (mode == "Debug" || mode == "Stepping") {
        tpl.SetVariable("modeClass", "status-debug");
    } else {
        tpl.SetVariable("modeClass", "status-paused");
    }

    tpl.SetVariable("videoMode", GetVideoModeName());

    // Memory flags
    uint32_t memMode = GetMemMode();

    auto setFlag = [&tpl](const std::string& name, bool value) {
        tpl.SetVariable(name, value ? "ON" : "OFF");
        tpl.SetVariable(name + "Class", value ? "flag-on" : "flag-off");
    };

    setFlag("80store", (memMode & MF_80STORE) != 0);
    setFlag("auxRead", (memMode & MF_AUXREAD) != 0);
    setFlag("auxWrite", (memMode & MF_AUXWRITE) != 0);
    setFlag("altZP", (memMode & MF_ALTZP) != 0);
    setFlag("highRam", (memMode & MF_HIGHRAM) != 0);
    setFlag("bank2", (memMode & MF_BANK2) != 0);
    setFlag("page2", (memMode & MF_PAGE2) != 0);
    setFlag("hires", (memMode & MF_HIRES) != 0);

    tpl.SetVariable("cycles", std::to_string(g_nCumulativeCycles));

    SendHtmlResponse(response, tpl.Render());
}

std::string MachineInfoProvider::GetApple2TypeName() const {
    switch (g_Apple2Type) {
        case A2TYPE_APPLE2:         return "Apple ][";
        case A2TYPE_APPLE2PLUS:     return "Apple ][+";
        case A2TYPE_APPLE2JPLUS:    return "Apple ][ J-Plus";
        case A2TYPE_APPLE2E:        return "Apple //e";
        case A2TYPE_APPLE2EENHANCED: return "Enhanced Apple //e";
        case A2TYPE_APPLE2C:        return "Apple //c";
        case A2TYPE_PRAVETS82:      return "Pravets 82";
        case A2TYPE_PRAVETS8M:      return "Pravets 8M";
        case A2TYPE_PRAVETS8A:      return "Pravets 8A";
        case A2TYPE_TK30002E:       return "TK3000 //e";
        case A2TYPE_BASE64A:        return "Base64A";
        default:                    return "Unknown";
    }
}

std::string MachineInfoProvider::GetCpuTypeName() const {
    switch (GetMainCpu()) {
        case CPU_6502:  return "6502 (NMOS)";
        case CPU_65C02: return "65C02 (CMOS)";
        case CPU_Z80:   return "Z80";
        default:        return "Unknown";
    }
}

std::string MachineInfoProvider::GetAppModeName() const {
    switch (g_nAppMode) {
        case MODE_LOGO:      return "Logo";
        case MODE_PAUSED:    return "Paused";
        case MODE_RUNNING:   return "Running";
        case MODE_DEBUG:     return "Debug";
        case MODE_STEPPING:  return "Stepping";
        case MODE_BENCHMARK: return "Benchmark";
        case MODE_UNDEFINED: return "Undefined";
    }
    return "Unknown";
}

std::string MachineInfoProvider::GetVideoModeName() const {
    // Simplified - full implementation would check video soft switches
    uint32_t memMode = GetMemMode();

    if (memMode & MF_HIRES) {
        if (memMode & MF_80STORE) {
            return "Double Hi-Res";
        }
        return "Hi-Res";
    } else {
        if (memMode & MF_80STORE) {
            return "80-Column Text";
        }
        return "Text/Lo-Res";
    }
}

} // namespace debugserver
