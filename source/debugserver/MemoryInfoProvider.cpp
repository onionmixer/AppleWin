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

#include "MemoryInfoProvider.h"
#include "JsonBuilder.h"
#include "SimpleTemplate.h"

// AppleWin includes
#include "Memory.h"
#include "CPU.h"

namespace debugserver {

void MemoryInfoProvider::HandleRequest(const HttpRequest& request, HttpResponse& response) {
    std::string path = request.GetPath();

    if (path == "/api/dump" || path == "/dump") {
        HandleApiDump(request, response);
    }
    else if (path == "/api/read" || path == "/read") {
        HandleApiRead(request, response);
    }
    else if (path == "/api/zeropage" || path == "/zeropage") {
        HandleApiZeroPage(request, response);
    }
    else if (path == "/api/stack" || path == "/stack") {
        HandleApiStack(request, response);
    }
    else if (path == "/api/textscreen" || path == "/textscreen") {
        HandleApiTextScreen(request, response);
    }
    else if (path == "/" || path == "/index.html") {
        HandleHtmlDashboard(request, response);
    }
    else {
        SendErrorResponse(response, 404, "Endpoint not found: " + path);
    }
}

void MemoryInfoProvider::HandleApiDump(const HttpRequest& request, HttpResponse& response) {
    // Get parameters
    std::string addrStr = request.GetQueryParam("addr", "0");
    std::string linesStr = request.GetQueryParam("lines", "16");
    std::string widthStr = request.GetQueryParam("width", "16");

    uint16_t startAddr = 0;
    if (!addrStr.empty()) {
        if (addrStr[0] == '$') addrStr = addrStr.substr(1);
        startAddr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
    }

    int lines = std::stoi(linesStr);
    int width = std::stoi(widthStr);

    if (lines < 1) lines = 1;
    if (lines > 256) lines = 256;
    if (width < 1) width = 1;
    if (width > 32) width = 32;

    auto dump = GetHexDump(startAddr, lines, width);

    JsonBuilder json;
    json.BeginObject()
        .AddHex16("startAddress", startAddr)
        .Add("lines", static_cast<int>(dump.size()))
        .Add("bytesPerLine", width)
        .Key("data").BeginArray();

    for (const auto& line : dump) {
        json.BeginObject()
            .Add("address", line.addressHex)
            .Add("hex", line.bytesHex)
            .Add("ascii", line.ascii)
            .Key("bytes").BeginArray();
        for (uint8_t b : line.bytes) {
            json.Value(static_cast<int>(b));
        }
        json.EndArray()
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MemoryInfoProvider::HandleApiRead(const HttpRequest& request, HttpResponse& response) {
    // Read single byte or range
    std::string addrStr = request.GetQueryParam("addr", "0");
    std::string lenStr = request.GetQueryParam("len", "1");

    uint16_t addr = 0;
    if (!addrStr.empty()) {
        if (addrStr[0] == '$') addrStr = addrStr.substr(1);
        addr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
    }

    int len = std::stoi(lenStr);
    if (len < 1) len = 1;
    if (len > 256) len = 256;

    JsonBuilder json;
    json.BeginObject()
        .AddHex16("address", addr)
        .Add("length", len)
        .Key("bytes").BeginArray();

    for (int i = 0; i < len && (addr + i) <= 0xFFFF; i++) {
        json.Value(static_cast<int>(mem[addr + i]));
    }

    json.EndArray()
        .Key("hex").BeginArray();

    for (int i = 0; i < len && (addr + i) <= 0xFFFF; i++) {
        json.Value(ToHex8(mem[addr + i]));
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MemoryInfoProvider::HandleApiZeroPage(const HttpRequest& request, HttpResponse& response) {
    auto dump = GetHexDump(0x0000, 16, 16);

    JsonBuilder json;
    json.BeginObject()
        .Add("description", "Zero Page ($0000-$00FF)")
        .Key("data").BeginArray();

    for (const auto& line : dump) {
        json.BeginObject()
            .Add("address", line.addressHex)
            .Add("hex", line.bytesHex)
            .Add("ascii", line.ascii)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MemoryInfoProvider::HandleApiStack(const HttpRequest& request, HttpResponse& response) {
    uint8_t sp = regs.sp & 0xFF;

    // Stack page is $0100-$01FF
    auto dump = GetHexDump(0x0100, 16, 16);

    JsonBuilder json;
    json.BeginObject()
        .Add("description", "Stack Page ($0100-$01FF)")
        .AddHex8("SP", sp)
        .AddHex16("stackPointer", 0x0100 + sp)
        .Key("data").BeginArray();

    for (const auto& line : dump) {
        json.BeginObject()
            .Add("address", line.addressHex)
            .Add("hex", line.bytesHex)
            .Add("ascii", line.ascii)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MemoryInfoProvider::HandleApiTextScreen(const HttpRequest& request, HttpResponse& response) {
    // Apple II text screen is at $0400-$07FF (Page 1) or $0800-$0BFF (Page 2)
    bool page2 = (GetMemMode() & MF_PAGE2) != 0;
    uint16_t baseAddr = page2 ? 0x0800 : 0x0400;

    // Text screen is 40 columns x 24 rows, but memory is not contiguous
    // Each row has 40 bytes with gaps for screen holes

    JsonBuilder json;
    json.BeginObject()
        .Add("page", page2 ? 2 : 1)
        .AddHex16("baseAddress", baseAddr)
        .Key("lines").BeginArray();

    // Apple II text screen memory map offsets for each row
    static const uint16_t rowOffsets[24] = {
        0x000, 0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380,
        0x028, 0x0A8, 0x128, 0x1A8, 0x228, 0x2A8, 0x328, 0x3A8,
        0x050, 0x0D0, 0x150, 0x1D0, 0x250, 0x2D0, 0x350, 0x3D0
    };

    for (int row = 0; row < 24; row++) {
        uint16_t rowAddr = baseAddr + rowOffsets[row];
        std::string text;
        text.reserve(40);

        for (int col = 0; col < 40; col++) {
            uint8_t ch = mem[rowAddr + col];
            // Convert Apple II character to ASCII
            // Normal ASCII: $A0-$DF maps to $20-$5F
            // Inverse: $00-$3F maps to $40-$7F (letters) or $20-$5F (symbols)
            // Flash: $40-$7F maps to $00-$3F
            if (ch >= 0xA0 && ch <= 0xFF) {
                text += static_cast<char>(ch & 0x7F);
            } else if (ch >= 0x80 && ch <= 0x9F) {
                text += static_cast<char>(ch & 0x7F);
            } else {
                // Inverse/flash - show as uppercase
                text += ToPrintable(ch | 0x40);
            }
        }

        json.BeginObject()
            .Add("row", row)
            .AddHex16("address", rowAddr)
            .Add("text", text)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void MemoryInfoProvider::HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response) {
    // Get optional address parameter
    std::string addrStr = request.GetQueryParam("addr", "0");
    uint16_t viewAddr = 0;
    if (!addrStr.empty()) {
        if (addrStr[0] == '$') addrStr = addrStr.substr(1);
        try {
            viewAddr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
        } catch (...) {
            viewAddr = 0;
        }
    }

    SimpleTemplate tpl;

    std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>AppleWin Debug - Memory Info</title>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="2">
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
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(500px, 1fr)); gap: 20px; }
        .info-box {
            background: #313244;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #89b4fa;
        }
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
        .hex-dump {
            font-family: 'Courier New', monospace;
            font-size: 12px;
            line-height: 1.4;
        }
        .hex-line {
            display: flex;
            padding: 2px 0;
        }
        .hex-line:hover { background: #45475a; }
        .hex-addr { color: #89b4fa; width: 60px; }
        .hex-bytes { color: #f9e2af; flex: 1; }
        .hex-ascii { color: #a6e3a1; width: 140px; margin-left: 20px; }
        .address-form {
            display: flex;
            gap: 10px;
            margin-bottom: 15px;
        }
        .address-form input {
            background: #45475a;
            border: 1px solid #585b70;
            color: #cdd6f4;
            padding: 5px 10px;
            border-radius: 4px;
            font-family: 'Courier New', monospace;
        }
        .address-form button {
            background: #89b4fa;
            color: #1e1e2e;
            border: none;
            padding: 5px 15px;
            border-radius: 4px;
            cursor: pointer;
        }
        .address-form button:hover { background: #b4befe; }
        .quick-links {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            margin-bottom: 15px;
        }
        .quick-links a {
            color: #89b4fa;
            text-decoration: none;
            padding: 3px 8px;
            background: #45475a;
            border-radius: 4px;
            font-size: 0.9em;
        }
        .quick-links a:hover { background: #585b70; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AppleWin Debug Server - Memory Info</h1>
        <div class="nav">
            <a href="http://localhost:65501/">Machine Info</a>
            <a href="http://localhost:65502/">I/O Info</a>
            <a href="http://localhost:65503/">CPU Info</a>
            <a href="/">Memory Info</a>
            <a href="/api/dump?addr={{viewAddr}}">API: Dump</a>
            <a href="/api/zeropage">API: Zero Page</a>
            <a href="/api/stack">API: Stack</a>
            <a href="/api/textscreen">API: Text Screen</a>
        </div>

        <div class="info-box">
            <h2>Memory Viewer</h2>
            <form class="address-form" method="get">
                <input type="text" name="addr" placeholder="Address (hex)" value="{{viewAddr}}">
                <button type="submit">Go</button>
            </form>
            <div class="quick-links">
                <a href="/?addr=$0000">Zero Page</a>
                <a href="/?addr=$0100">Stack</a>
                <a href="/?addr=$0400">Text Page 1</a>
                <a href="/?addr=$0800">Text Page 2</a>
                <a href="/?addr=$2000">HGR Page 1</a>
                <a href="/?addr=$4000">HGR Page 2</a>
                <a href="/?addr=$C000">I/O</a>
                <a href="/?addr=$D000">ROM</a>
            </div>
            <div class="hex-dump">
{{#dump}}
                <div class="hex-line">
                    <span class="hex-addr">{{address}}</span>
                    <span class="hex-bytes">{{hex}}</span>
                    <span class="hex-ascii">{{ascii}}</span>
                </div>
{{/dump}}
            </div>
        </div>

        <div class="grid" style="margin-top: 20px;">
            <div class="info-box">
                <h2>Zero Page ($0000-$00FF)</h2>
                <div class="hex-dump">
{{#zeropage}}
                    <div class="hex-line">
                        <span class="hex-addr">{{address}}</span>
                        <span class="hex-bytes">{{hex}}</span>
                        <span class="hex-ascii">{{ascii}}</span>
                    </div>
{{/zeropage}}
                </div>
            </div>

            <div class="info-box">
                <h2>Stack ($0100-$01FF) SP={{sp}}</h2>
                <div class="hex-dump">
{{#stack}}
                    <div class="hex-line">
                        <span class="hex-addr">{{address}}</span>
                        <span class="hex-bytes">{{hex}}</span>
                        <span class="hex-ascii">{{ascii}}</span>
                    </div>
{{/stack}}
                </div>
            </div>
        </div>
    </div>
</body>
</html>)HTML";

    tpl.LoadFromString(html);
    tpl.SetVariable("viewAddr", ToHex16Prefixed(viewAddr));
    tpl.SetVariable("sp", ToHex8Prefixed(static_cast<uint8_t>(regs.sp & 0xFF)));

    // Main memory dump
    auto dump = GetHexDump(viewAddr, 16, 16);
    SimpleTemplate::ArrayData dumpArray;
    for (const auto& line : dump) {
        SimpleTemplate::VariableMap item;
        item["address"] = line.addressHex;
        item["hex"] = line.bytesHex;
        item["ascii"] = line.ascii;
        dumpArray.push_back(item);
    }
    tpl.SetArray("dump", dumpArray);

    // Zero page (first 8 lines = 128 bytes)
    auto zp = GetHexDump(0x0000, 8, 16);
    SimpleTemplate::ArrayData zpArray;
    for (const auto& line : zp) {
        SimpleTemplate::VariableMap item;
        item["address"] = line.addressHex;
        item["hex"] = line.bytesHex;
        item["ascii"] = line.ascii;
        zpArray.push_back(item);
    }
    tpl.SetArray("zeropage", zpArray);

    // Stack (first 8 lines = 128 bytes)
    auto stack = GetHexDump(0x0100, 8, 16);
    SimpleTemplate::ArrayData stackArray;
    for (const auto& line : stack) {
        SimpleTemplate::VariableMap item;
        item["address"] = line.addressHex;
        item["hex"] = line.bytesHex;
        item["ascii"] = line.ascii;
        stackArray.push_back(item);
    }
    tpl.SetArray("stack", stackArray);

    SendHtmlResponse(response, tpl.Render());
}

std::vector<MemoryInfoProvider::HexDumpLine> MemoryInfoProvider::GetHexDump(
    uint16_t startAddr, int lines, int bytesPerLine) const {

    std::vector<HexDumpLine> result;

    uint32_t addr = startAddr;
    for (int i = 0; i < lines && addr <= 0xFFFF; i++) {
        HexDumpLine line;
        line.address = static_cast<uint16_t>(addr);
        line.addressHex = ToHex16Prefixed(static_cast<uint16_t>(addr));

        std::string hexStr;
        std::string asciiStr;

        for (int j = 0; j < bytesPerLine && addr <= 0xFFFF; j++) {
            uint8_t byte = mem[addr];
            line.bytes.push_back(byte);

            if (j > 0) hexStr += " ";
            hexStr += ToHex8(byte);
            asciiStr += ToPrintable(byte);

            addr++;
        }

        line.bytesHex = hexStr;
        line.ascii = asciiStr;
        result.push_back(line);
    }

    return result;
}

char MemoryInfoProvider::ToPrintable(uint8_t byte) {
    if (byte >= 0x20 && byte <= 0x7E) {
        return static_cast<char>(byte);
    }
    return '.';
}

} // namespace debugserver
