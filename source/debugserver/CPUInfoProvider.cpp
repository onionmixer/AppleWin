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

#include "CPUInfoProvider.h"
#include "JsonBuilder.h"
#include "SimpleTemplate.h"

// AppleWin includes
#include "CPU.h"
#include "Memory.h"
#include "Debugger/Debug.h"

namespace debugserver {

void CPUInfoProvider::HandleRequest(const HttpRequest& request, HttpResponse& response) {
    std::string path = request.GetPath();

    if (path == "/api/registers" || path == "/registers") {
        HandleApiRegisters(request, response);
    }
    else if (path == "/api/flags" || path == "/flags") {
        HandleApiFlags(request, response);
    }
    else if (path == "/api/breakpoints" || path == "/breakpoints") {
        HandleApiBreakpoints(request, response);
    }
    else if (path == "/api/disasm" || path == "/disasm") {
        HandleApiDisassembly(request, response);
    }
    else if (path == "/api/stack" || path == "/stack") {
        HandleApiStack(request, response);
    }
    else if (path == "/" || path == "/index.html") {
        HandleHtmlDashboard(request, response);
    }
    else {
        SendErrorResponse(response, 404, "Endpoint not found: " + path);
    }
}

void CPUInfoProvider::HandleApiRegisters(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .AddHex8("A", regs.a)
        .AddHex8("X", regs.x)
        .AddHex8("Y", regs.y)
        .AddHex16("PC", regs.pc)
        .AddHex8("SP", static_cast<uint8_t>(regs.sp & 0xFF))
        .AddHex8("P", regs.ps)
        .Add("jammed", regs.bJammed != 0)
        .Key("decimal").BeginObject()
            .Add("A", static_cast<int>(regs.a))
            .Add("X", static_cast<int>(regs.x))
            .Add("Y", static_cast<int>(regs.y))
            .Add("PC", static_cast<int>(regs.pc))
            .Add("SP", static_cast<int>(regs.sp & 0xFF))
        .EndObject()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void CPUInfoProvider::HandleApiFlags(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    uint8_t ps = regs.ps;

    json.BeginObject()
        .AddHex8("value", ps)
        .Add("N", (ps & AF_SIGN) != 0)
        .Add("V", (ps & AF_OVERFLOW) != 0)
        .Add("R", (ps & AF_RESERVED) != 0)
        .Add("B", (ps & AF_BREAK) != 0)
        .Add("D", (ps & AF_DECIMAL) != 0)
        .Add("I", (ps & AF_INTERRUPT) != 0)
        .Add("Z", (ps & AF_ZERO) != 0)
        .Add("C", (ps & AF_CARRY) != 0)
        .Add("string", std::string() +
            ((ps & AF_SIGN)      ? 'N' : '-') +
            ((ps & AF_OVERFLOW)  ? 'V' : '-') +
            ((ps & AF_RESERVED)  ? 'R' : '-') +
            ((ps & AF_BREAK)     ? 'B' : '-') +
            ((ps & AF_DECIMAL)   ? 'D' : '-') +
            ((ps & AF_INTERRUPT) ? 'I' : '-') +
            ((ps & AF_ZERO)      ? 'Z' : '-') +
            ((ps & AF_CARRY)     ? 'C' : '-'))
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void CPUInfoProvider::HandleApiBreakpoints(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    json.BeginObject()
        .Add("count", g_nBreakpoints)
        .Key("breakpoints").BeginArray();

    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        const Breakpoint_t& bp = g_aBreakpoints[i];
        if (bp.bSet) {
            json.BeginObject()
                .Add("index", i)
                .AddHex16("address", bp.nAddress)
                .Add("length", static_cast<int>(bp.nLength))
                .Add("source", g_aBreakpointSource[bp.eSource])
                .Add("operator", g_aBreakpointSymbols[bp.eOperator])
                .Add("enabled", bp.bEnabled)
                .Add("temp", bp.bTemp)
                .Add("hit", bp.bHit)
                .Add("hitCount", static_cast<unsigned int>(bp.nHitCount))
            .EndObject();
        }
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void CPUInfoProvider::HandleApiDisassembly(const HttpRequest& request, HttpResponse& response) {
    // Get parameters
    std::string addrStr = request.GetQueryParam("addr", "");
    std::string linesStr = request.GetQueryParam("lines", "16");

    uint16_t startAddr = regs.pc;
    if (!addrStr.empty()) {
        // Parse hex address (skip $ prefix if present)
        if (addrStr[0] == '$') addrStr = addrStr.substr(1);
        startAddr = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
    }

    int lines = std::stoi(linesStr);
    if (lines < 1) lines = 1;
    if (lines > 64) lines = 64;

    auto disasm = GetDisassembly(startAddr, lines);

    JsonBuilder json;
    json.BeginObject()
        .AddHex16("startAddress", startAddr)
        .Add("lines", static_cast<int>(disasm.size()))
        .Key("disassembly").BeginArray();

    for (const auto& line : disasm) {
        json.BeginObject()
            .Add("address", line.addressHex)
            .Add("bytes", line.bytes)
            .Add("mnemonic", line.mnemonic)
            .Add("operand", line.operand)
            .Add("isPC", line.isCurrentPC)
            .Add("hasBreakpoint", line.hasBreakpoint)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void CPUInfoProvider::HandleApiStack(const HttpRequest& request, HttpResponse& response) {
    JsonBuilder json;

    uint8_t sp = regs.sp & 0xFF;
    uint16_t stackBase = 0x0100;

    json.BeginObject()
        .AddHex8("SP", sp)
        .Add("depth", static_cast<int>(0xFF - sp))
        .Key("contents").BeginArray();

    // Show stack from current SP+1 to 0x1FF
    for (int i = sp + 1; i <= 0xFF && i <= sp + 32; i++) {
        uint16_t addr = stackBase + i;
        uint8_t value = mem[addr];
        json.BeginObject()
            .Add("offset", i - sp - 1)
            .AddHex16("address", addr)
            .AddHex8("value", value)
        .EndObject();
    }

    json.EndArray()
    .EndObject();

    SendJsonResponse(response, json.ToPrettyString());
}

void CPUInfoProvider::HandleHtmlDashboard(const HttpRequest& request, HttpResponse& response) {
    SimpleTemplate tpl;

    std::string html = R"HTML(<!DOCTYPE html>
<html>
<head>
    <title>AppleWin Debug - CPU Info</title>
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
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 20px; }
        .info-box {
            background: #313244;
            padding: 15px;
            border-radius: 8px;
            border-left: 4px solid #89b4fa;
        }
        .register-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
        .register {
            background: #45475a;
            padding: 10px;
            border-radius: 4px;
            text-align: center;
        }
        .register-name { color: #94a3b8; font-size: 0.9em; }
        .register-value { color: #f9e2af; font-size: 1.4em; font-weight: bold; }
        .flags-display {
            display: flex;
            gap: 8px;
            justify-content: center;
            flex-wrap: wrap;
        }
        .flag {
            width: 30px;
            height: 30px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 4px;
            font-weight: bold;
        }
        .flag-on { background: #a6e3a1; color: #1e1e2e; }
        .flag-off { background: #45475a; color: #6c7086; }
        .disasm-line {
            font-family: 'Courier New', monospace;
            padding: 2px 8px;
            border-radius: 2px;
        }
        .disasm-line:hover { background: #45475a; }
        .disasm-pc { background: #f9e2af22; border-left: 3px solid #f9e2af; }
        .disasm-bp { border-left: 3px solid #f38ba8; }
        .disasm-addr { color: #89b4fa; }
        .disasm-bytes { color: #6c7086; }
        .disasm-mnemonic { color: #a6e3a1; font-weight: bold; }
        .disasm-operand { color: #cdd6f4; }
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
        th, td { padding: 5px 10px; text-align: left; }
        th { color: #94a3b8; border-bottom: 1px solid #45475a; }
        .bp-enabled { color: #a6e3a1; }
        .bp-disabled { color: #6c7086; }
    </style>
</head>
<body>
    <div class="container">
        <h1>AppleWin Debug Server - CPU Info</h1>
        <div class="nav">
            <a href="http://localhost:65501/">Machine Info</a>
            <a href="http://localhost:65502/">I/O Info</a>
            <a href="/">CPU Info</a>
            <a href="http://localhost:65504/">Memory Info</a>
            <a href="/api/registers">API: Registers</a>
            <a href="/api/flags">API: Flags</a>
            <a href="/api/breakpoints">API: Breakpoints</a>
            <a href="/api/disasm">API: Disasm</a>
        </div>

        <div class="grid">
            <div class="info-box">
                <h2>Registers</h2>
                <div class="register-grid">
                    <div class="register">
                        <div class="register-name">A</div>
                        <div class="register-value">{{regA}}</div>
                    </div>
                    <div class="register">
                        <div class="register-name">X</div>
                        <div class="register-value">{{regX}}</div>
                    </div>
                    <div class="register">
                        <div class="register-name">Y</div>
                        <div class="register-value">{{regY}}</div>
                    </div>
                    <div class="register">
                        <div class="register-name">PC</div>
                        <div class="register-value">{{regPC}}</div>
                    </div>
                    <div class="register">
                        <div class="register-name">SP</div>
                        <div class="register-value">{{regSP}}</div>
                    </div>
                    <div class="register">
                        <div class="register-name">P</div>
                        <div class="register-value">{{regP}}</div>
                    </div>
                </div>

                <h2>Flags</h2>
                <div class="flags-display">
                    <div class="flag {{flagN}}">N</div>
                    <div class="flag {{flagV}}">V</div>
                    <div class="flag {{flagR}}">-</div>
                    <div class="flag {{flagB}}">B</div>
                    <div class="flag {{flagD}}">D</div>
                    <div class="flag {{flagI}}">I</div>
                    <div class="flag {{flagZ}}">Z</div>
                    <div class="flag {{flagC}}">C</div>
                </div>
            </div>

            <div class="info-box">
                <h2>Disassembly</h2>
                <div id="disasm">
{{#disasm}}
                    <div class="disasm-line {{lineClass}}">
                        <span class="disasm-addr">{{address}}</span>
                        <span class="disasm-bytes">{{bytes}}</span>
                        <span class="disasm-mnemonic">{{mnemonic}}</span>
                        <span class="disasm-operand">{{operand}}</span>
                    </div>
{{/disasm}}
                </div>
            </div>
        </div>

        <div class="info-box" style="margin-top: 20px;">
            <h2>Breakpoints ({{bpCount}})</h2>
            <table>
                <tr>
                    <th>#</th>
                    <th>Address</th>
                    <th>Type</th>
                    <th>Status</th>
                    <th>Hits</th>
                </tr>
{{#breakpoints}}
                <tr>
                    <td>{{index}}</td>
                    <td>{{address}}</td>
                    <td>{{type}}</td>
                    <td class="{{statusClass}}">{{status}}</td>
                    <td>{{hits}}</td>
                </tr>
{{/breakpoints}}
            </table>
        </div>
    </div>
</body>
</html>)HTML";

    tpl.LoadFromString(html);

    // Set register values
    tpl.SetVariable("regA", ToHex8Prefixed(regs.a));
    tpl.SetVariable("regX", ToHex8Prefixed(regs.x));
    tpl.SetVariable("regY", ToHex8Prefixed(regs.y));
    tpl.SetVariable("regPC", ToHex16Prefixed(regs.pc));
    tpl.SetVariable("regSP", ToHex8Prefixed(static_cast<uint8_t>(regs.sp & 0xFF)));
    tpl.SetVariable("regP", ToHex8Prefixed(regs.ps));

    // Set flag classes
    uint8_t ps = regs.ps;
    tpl.SetVariable("flagN", (ps & AF_SIGN)      ? "flag-on" : "flag-off");
    tpl.SetVariable("flagV", (ps & AF_OVERFLOW)  ? "flag-on" : "flag-off");
    tpl.SetVariable("flagR", (ps & AF_RESERVED)  ? "flag-on" : "flag-off");
    tpl.SetVariable("flagB", (ps & AF_BREAK)     ? "flag-on" : "flag-off");
    tpl.SetVariable("flagD", (ps & AF_DECIMAL)   ? "flag-on" : "flag-off");
    tpl.SetVariable("flagI", (ps & AF_INTERRUPT) ? "flag-on" : "flag-off");
    tpl.SetVariable("flagZ", (ps & AF_ZERO)      ? "flag-on" : "flag-off");
    tpl.SetVariable("flagC", (ps & AF_CARRY)     ? "flag-on" : "flag-off");

    // Get disassembly
    auto disasm = GetDisassembly(regs.pc, 12);
    SimpleTemplate::ArrayData disasmArray;
    for (const auto& line : disasm) {
        SimpleTemplate::VariableMap item;
        item["address"] = line.addressHex;
        item["bytes"] = line.bytes;
        item["mnemonic"] = line.mnemonic;
        item["operand"] = line.operand;
        if (line.isCurrentPC && line.hasBreakpoint) {
            item["lineClass"] = "disasm-pc disasm-bp";
        } else if (line.isCurrentPC) {
            item["lineClass"] = "disasm-pc";
        } else if (line.hasBreakpoint) {
            item["lineClass"] = "disasm-bp";
        } else {
            item["lineClass"] = "";
        }
        disasmArray.push_back(item);
    }
    tpl.SetArray("disasm", disasmArray);

    // Breakpoints
    tpl.SetVariable("bpCount", g_nBreakpoints);
    SimpleTemplate::ArrayData bpArray;
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        const Breakpoint_t& bp = g_aBreakpoints[i];
        if (bp.bSet) {
            SimpleTemplate::VariableMap item;
            item["index"] = std::to_string(i);
            item["address"] = ToHex16Prefixed(bp.nAddress);
            item["type"] = g_aBreakpointSource[bp.eSource];
            item["status"] = bp.bEnabled ? "Enabled" : "Disabled";
            item["statusClass"] = bp.bEnabled ? "bp-enabled" : "bp-disabled";
            item["hits"] = std::to_string(bp.nHitCount);
            bpArray.push_back(item);
        }
    }
    tpl.SetArray("breakpoints", bpArray);

    SendHtmlResponse(response, tpl.Render());
}

std::vector<CPUInfoProvider::DisasmLine> CPUInfoProvider::GetDisassembly(uint16_t startAddr, int lines) const {
    std::vector<DisasmLine> result;

    uint16_t addr = startAddr;
    for (int i = 0; i < lines && addr <= 0xFFFF; i++) {
        DisasmLine line;
        line.address = addr;
        line.addressHex = ToHex16Prefixed(addr);
        line.isCurrentPC = (addr == regs.pc);

        // Check for breakpoint at this address
        line.hasBreakpoint = false;
        for (int bp = 0; bp < MAX_BREAKPOINTS; bp++) {
            if (g_aBreakpoints[bp].bSet && g_aBreakpoints[bp].bEnabled &&
                g_aBreakpoints[bp].eSource == BP_SRC_REG_PC &&
                g_aBreakpoints[bp].nAddress == addr) {
                line.hasBreakpoint = true;
                break;
            }
        }

        // Read opcode and operands
        uint8_t opcode = mem[addr];

        // Simple disassembly - for full implementation, use the Debugger's disassembler
        // This is a simplified version that just shows bytes
        line.bytes = ToHex8(opcode);

        // Very simplified mnemonic lookup
        // In real implementation, this would use the full opcode tables from the debugger
        static const char* mnemonics[256] = {
            "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 00-07
            "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???", // 08-0F
            "BPL", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 10-17
            "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???", // 18-1F
            "JSR", "AND", "???", "???", "BIT", "AND", "ROL", "???", // 20-27
            "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???", // 28-2F
            "BMI", "AND", "???", "???", "???", "AND", "ROL", "???", // 30-37
            "SEC", "AND", "???", "???", "???", "AND", "ROL", "???", // 38-3F
            "RTI", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 40-47
            "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???", // 48-4F
            "BVC", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 50-57
            "CLI", "EOR", "???", "???", "???", "EOR", "LSR", "???", // 58-5F
            "RTS", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 60-67
            "PLA", "ADC", "ROR", "???", "JMP", "ADC", "ROR", "???", // 68-6F
            "BVS", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 70-77
            "SEI", "ADC", "???", "???", "???", "ADC", "ROR", "???", // 78-7F
            "???", "STA", "???", "???", "STY", "STA", "STX", "???", // 80-87
            "DEY", "???", "TXA", "???", "STY", "STA", "STX", "???", // 88-8F
            "BCC", "STA", "???", "???", "STY", "STA", "STX", "???", // 90-97
            "TYA", "STA", "TXS", "???", "???", "STA", "???", "???", // 98-9F
            "LDY", "LDA", "LDX", "???", "LDY", "LDA", "LDX", "???", // A0-A7
            "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "???", // A8-AF
            "BCS", "LDA", "???", "???", "LDY", "LDA", "LDX", "???", // B0-B7
            "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "???", // B8-BF
            "CPY", "CMP", "???", "???", "CPY", "CMP", "DEC", "???", // C0-C7
            "INY", "CMP", "DEX", "???", "CPY", "CMP", "DEC", "???", // C8-CF
            "BNE", "CMP", "???", "???", "???", "CMP", "DEC", "???", // D0-D7
            "CLD", "CMP", "???", "???", "???", "CMP", "DEC", "???", // D8-DF
            "CPX", "SBC", "???", "???", "CPX", "SBC", "INC", "???", // E0-E7
            "INX", "SBC", "NOP", "???", "CPX", "SBC", "INC", "???", // E8-EF
            "BEQ", "SBC", "???", "???", "???", "SBC", "INC", "???", // F0-F7
            "SED", "SBC", "???", "???", "???", "SBC", "INC", "???"  // F8-FF
        };

        // Instruction lengths (simplified)
        static const uint8_t lengths[256] = {
            1,2,1,1,1,2,2,1, 1,2,1,1,1,3,3,1, // 00-0F
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1, // 10-1F
            3,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1, // 20-2F
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1, // 30-3F
            1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1, // 40-4F
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1, // 50-5F
            1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1, // 60-6F
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1, // 70-7F
            1,2,1,1,2,2,2,1, 1,1,1,1,3,3,3,1, // 80-8F
            2,2,1,1,2,2,2,1, 1,3,1,1,1,3,1,1, // 90-9F
            2,2,2,1,2,2,2,1, 1,2,1,1,3,3,3,1, // A0-AF
            2,2,1,1,2,2,2,1, 1,3,1,1,3,3,3,1, // B0-BF
            2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1, // C0-CF
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1, // D0-DF
            2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1, // E0-EF
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1  // F0-FF
        };

        line.mnemonic = mnemonics[opcode];
        uint8_t len = lengths[opcode];

        // Format operand bytes
        if (len >= 2 && (addr + 1) <= 0xFFFF) {
            uint8_t op1 = mem[addr + 1];
            line.bytes += " " + ToHex8(op1);
            if (len == 2) {
                line.operand = ToHex8Prefixed(op1);
            }
        }
        if (len >= 3 && (addr + 2) <= 0xFFFF) {
            uint8_t op2 = mem[addr + 2];
            line.bytes += " " + ToHex8(op2);
            uint8_t op1 = mem[addr + 1];
            uint16_t word = op1 | (op2 << 8);
            line.operand = ToHex16Prefixed(word);
        }

        // Handle relative branches
        if (line.mnemonic[0] == 'B' && line.mnemonic != "BIT" && line.mnemonic != "BRK" && len == 2) {
            int8_t offset = static_cast<int8_t>(mem[addr + 1]);
            uint16_t target = addr + 2 + offset;
            line.operand = ToHex16Prefixed(target);
        }

        result.push_back(line);
        addr += len;
    }

    return result;
}

const char* CPUInfoProvider::GetFlagName(int bit) {
    static const char* names[] = { "Carry", "Zero", "Interrupt", "Decimal",
                                    "Break", "Reserved", "Overflow", "Sign" };
    if (bit >= 0 && bit < 8) return names[bit];
    return "Unknown";
}

char CPUInfoProvider::GetFlagChar(int bit) {
    static const char chars[] = "CZIDBRVN";
    if (bit >= 0 && bit < 8) return chars[bit];
    return '?';
}

} // namespace debugserver
