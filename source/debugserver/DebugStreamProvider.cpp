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

#include "DebugStreamProvider.h"

// AppleWin includes
#include "Common.h"
#include "Core.h"
#include "CPU.h"
#include "Memory.h"
#include "CardManager.h"
#include "Debugger/Debug.h"

#include <sstream>
#include <iomanip>
#include <chrono>

namespace debugserver {

DebugStreamProvider::DebugStreamProvider() {
}

//-----------------------------------------------------------------------------
// System Messages
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetHelloMessage() {
    std::map<std::string, std::string> extra;
    extra["ver"] = VERSION;
    extra["ts"] = std::to_string(GetTimestamp());
    return FormatLine("sys", "conn", "hello", "AppleWin Debug Stream", extra);
}

std::string DebugStreamProvider::GetGoodbyeMessage() {
    std::map<std::string, std::string> extra;
    extra["ts"] = std::to_string(GetTimestamp());
    return FormatLine("sys", "conn", "goodbye", "", extra);
}

std::string DebugStreamProvider::GetErrorMessage(const std::string& error) {
    return FormatLine("sys", "error", "msg", error);
}

//-----------------------------------------------------------------------------
// CPU Information
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetCPURegisters() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;

    result += FormatLine("cpu", "reg", "a", ToHex8(regs.a)) + "\r\n";
    result += FormatLine("cpu", "reg", "x", ToHex8(regs.x)) + "\r\n";
    result += FormatLine("cpu", "reg", "y", ToHex8(regs.y)) + "\r\n";
    result += FormatLine("cpu", "reg", "pc", ToHex16(regs.pc)) + "\r\n";
    result += FormatLine("cpu", "reg", "sp", ToHex8(static_cast<uint8_t>(regs.sp & 0xFF))) + "\r\n";
    result += FormatLine("cpu", "reg", "p", ToHex8(regs.ps));

    return result;
}

std::string DebugStreamProvider::GetCPURegister(const std::string& regName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (regName == "a") {
        return FormatLine("cpu", "reg", "a", ToHex8(regs.a));
    } else if (regName == "x") {
        return FormatLine("cpu", "reg", "x", ToHex8(regs.x));
    } else if (regName == "y") {
        return FormatLine("cpu", "reg", "y", ToHex8(regs.y));
    } else if (regName == "pc") {
        return FormatLine("cpu", "reg", "pc", ToHex16(regs.pc));
    } else if (regName == "sp") {
        return FormatLine("cpu", "reg", "sp", ToHex8(static_cast<uint8_t>(regs.sp & 0xFF)));
    } else if (regName == "p") {
        return FormatLine("cpu", "reg", "p", ToHex8(regs.ps));
    }

    return "";
}

std::string DebugStreamProvider::GetCPUFlags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;

    uint8_t ps = regs.ps;

    result += FormatLine("cpu", "flag", "n", (ps & AF_SIGN) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "v", (ps & AF_OVERFLOW) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "b", (ps & AF_BREAK) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "d", (ps & AF_DECIMAL) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "i", (ps & AF_INTERRUPT) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "z", (ps & AF_ZERO) ? "1" : "0") + "\r\n";
    result += FormatLine("cpu", "flag", "c", (ps & AF_CARRY) ? "1" : "0");

    return result;
}

std::string DebugStreamProvider::GetCPUState() {
    std::lock_guard<std::mutex> lock(m_mutex);

    return FormatLine("cpu", "state", "jammed", regs.bJammed ? "1" : "0");
}

//-----------------------------------------------------------------------------
// Memory Information
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetMemoryRead(uint16_t addr, uint8_t value) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    return FormatLine("mem", "read", "byte", ToHex8(value), extra);
}

std::string DebugStreamProvider::GetMemoryWrite(uint16_t addr, uint8_t value) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    return FormatLine("mem", "write", "byte", ToHex8(value), extra);
}

std::string DebugStreamProvider::GetMemoryDump(uint16_t startAddr, const std::vector<uint8_t>& data) {
    std::string result;
    for (size_t i = 0; i < data.size(); ++i) {
        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(static_cast<uint16_t>(startAddr + i));
        result += FormatLine("mem", "dump", "byte", ToHex8(data[i]), extra);
        if (i < data.size() - 1) {
            result += "\r\n";
        }
    }
    return result;
}

std::string DebugStreamProvider::GetMemoryBankStatus() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;

    UINT memMode = GetMemMode();

    // Memory mode flags
    result += FormatLine("mem", "bank", "mode", ToHex8(static_cast<uint8_t>(memMode & 0xFF)));

    return result;
}

std::string DebugStreamProvider::GetMemoryFlags() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;

    UINT memMode = GetMemMode();

    // All memory flags as individual lines
    result += FormatLine("mem", "flag", "80store", (memMode & MF_80STORE) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "auxRead", (memMode & MF_AUXREAD) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "auxWrite", (memMode & MF_AUXWRITE) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "altZP", (memMode & MF_ALTZP) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "highRam", (memMode & MF_HIGHRAM) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "bank2", (memMode & MF_BANK2) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "writeRam", (memMode & MF_WRITERAM) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "page2", (memMode & MF_PAGE2) ? "1" : "0") + "\r\n";
    result += FormatLine("mem", "flag", "hires", (memMode & MF_HIRES) ? "1" : "0");

    return result;
}

//-----------------------------------------------------------------------------
// I/O Information
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetSoftSwitchRead(uint16_t addr, uint8_t value) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    return FormatLine("io", "sw_read", "val", ToHex8(value), extra);
}

std::string DebugStreamProvider::GetSoftSwitchWrite(uint16_t addr, uint8_t value) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    return FormatLine("io", "sw_write", "val", ToHex8(value), extra);
}

//-----------------------------------------------------------------------------
// Machine Information
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetMachineInfo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result;

    // Machine type
    const char* machineType = "Unknown";
    switch (GetApple2Type()) {
        case A2TYPE_APPLE2:      machineType = "Apple2"; break;
        case A2TYPE_APPLE2PLUS:  machineType = "Apple2Plus"; break;
        case A2TYPE_APPLE2JPLUS: machineType = "Apple2JPlus"; break;
        case A2TYPE_APPLE2E:     machineType = "Apple2e"; break;
        case A2TYPE_APPLE2EENHANCED: machineType = "Apple2eEnhanced"; break;
        default: break;
    }

    result += FormatLine("mach", "info", "type", machineType);

    return result;
}

std::string DebugStreamProvider::GetMachineStatus(const std::string& mode) {
    return FormatLine("mach", "status", "mode", mode);
}

//-----------------------------------------------------------------------------
// Debug Information
//-----------------------------------------------------------------------------

std::string DebugStreamProvider::GetBreakpointHit(int index, uint16_t addr) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    extra["idx"] = std::to_string(index);
    return FormatLine("dbg", "bp", "hit", "1", extra);
}

std::string DebugStreamProvider::GetTraceExec(uint16_t addr, const std::string& disasm) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    return FormatLine("dbg", "trace", "exec", EscapeJson(disasm), extra);
}

std::string DebugStreamProvider::GetTraceMemory(uint16_t addr, uint8_t value, bool isWrite) {
    std::map<std::string, std::string> extra;
    extra["addr"] = ToHex16(addr);
    extra["rw"] = isWrite ? "w" : "r";
    return FormatLine("dbg", "trace", "mem", ToHex8(value), extra);
}

//-----------------------------------------------------------------------------
// I/O Information (65502 compatibility)
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamProvider::GetSoftSwitches() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    uint32_t memMode = GetMemMode();

    // Soft switch states matching IOInfoProvider
    struct SwitchInfo {
        const char* name;
        bool state;
    };

    SwitchInfo switches[] = {
        { "80store",    (memMode & MF_80STORE) != 0 },
        { "ramrd",      (memMode & MF_AUXREAD) != 0 },
        { "ramwrt",     (memMode & MF_AUXWRITE) != 0 },
        { "altzp",      (memMode & MF_ALTZP) != 0 },
        { "page2",      (memMode & MF_PAGE2) != 0 },
        { "hires",      (memMode & MF_HIRES) != 0 },
        { "lcram",      (memMode & MF_HIGHRAM) != 0 },
        { "lcbank2",    (memMode & MF_BANK2) != 0 },
        { "lcwrite",    (memMode & MF_WRITERAM) != 0 },
        { "intcxrom",   MemCheckINTCXROM() },
        { "slotc3rom",  MemCheckSLOTC3ROM() },
    };

    for (const auto& sw : switches) {
        lines.push_back(FormatLine("io", "switch", sw.name, sw.state ? "1" : "0"));
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetSlotInfo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    CardManager& cardMgr = GetCardMgr();

    for (int slot = 0; slot <= 7; slot++) {
        SS_CARDTYPE cardType = cardMgr.QuerySlot(slot);
        std::string typeName = (cardType != CT_Empty) ? Card::GetCardName(cardType) : "Empty";

        std::map<std::string, std::string> extra;
        extra["idx"] = std::to_string(slot);
        lines.push_back(FormatLine("io", "slot", "type", typeName, extra));

        extra.clear();
        extra["idx"] = std::to_string(slot);
        lines.push_back(FormatLine("io", "slot", "active", (cardType != CT_Empty) ? "1" : "0", extra));
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetAnnunciators() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    for (int i = 0; i < 4; i++) {
        std::map<std::string, std::string> extra;
        extra["idx"] = std::to_string(i);
        lines.push_back(FormatLine("io", "ann", "state", MemGetAnnunciator(i) ? "1" : "0", extra));
    }

    return lines;
}

//-----------------------------------------------------------------------------
// Extended CPU Information (65503 compatibility)
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamProvider::GetBreakpointList() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Breakpoint count
    lines.push_back(FormatLine("dbg", "bp", "count", std::to_string(g_nBreakpoints)));

    // Each breakpoint
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        const Breakpoint_t& bp = g_aBreakpoints[i];
        if (bp.bSet) {
            std::map<std::string, std::string> extra;
            extra["idx"] = std::to_string(i);
            extra["addr"] = ToHex16(bp.nAddress);

            lines.push_back(FormatLine("dbg", "bp", "enabled", bp.bEnabled ? "1" : "0", extra));

            extra["idx"] = std::to_string(i);
            lines.push_back(FormatLine("dbg", "bp", "source", g_aBreakpointSource[bp.eSource], extra));

            extra["idx"] = std::to_string(i);
            lines.push_back(FormatLine("dbg", "bp", "hits", std::to_string(bp.nHitCount), extra));
        }
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetDisassembly(uint16_t addr, int numLines) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Simplified mnemonic lookup (same as CPUInfoProvider)
    static const char* mnemonics[256] = {
        "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???",
        "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???",
        "BPL", "ORA", "???", "???", "???", "ORA", "ASL", "???",
        "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???",
        "JSR", "AND", "???", "???", "BIT", "AND", "ROL", "???",
        "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???",
        "BMI", "AND", "???", "???", "???", "AND", "ROL", "???",
        "SEC", "AND", "???", "???", "???", "AND", "ROL", "???",
        "RTI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
        "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???",
        "BVC", "EOR", "???", "???", "???", "EOR", "LSR", "???",
        "CLI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
        "RTS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
        "PLA", "ADC", "ROR", "???", "JMP", "ADC", "ROR", "???",
        "BVS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
        "SEI", "ADC", "???", "???", "???", "ADC", "ROR", "???",
        "???", "STA", "???", "???", "STY", "STA", "STX", "???",
        "DEY", "???", "TXA", "???", "STY", "STA", "STX", "???",
        "BCC", "STA", "???", "???", "STY", "STA", "STX", "???",
        "TYA", "STA", "TXS", "???", "???", "STA", "???", "???",
        "LDY", "LDA", "LDX", "???", "LDY", "LDA", "LDX", "???",
        "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "???",
        "BCS", "LDA", "???", "???", "LDY", "LDA", "LDX", "???",
        "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "???",
        "CPY", "CMP", "???", "???", "CPY", "CMP", "DEC", "???",
        "INY", "CMP", "DEX", "???", "CPY", "CMP", "DEC", "???",
        "BNE", "CMP", "???", "???", "???", "CMP", "DEC", "???",
        "CLD", "CMP", "???", "???", "???", "CMP", "DEC", "???",
        "CPX", "SBC", "???", "???", "CPX", "SBC", "INC", "???",
        "INX", "SBC", "NOP", "???", "CPX", "SBC", "INC", "???",
        "BEQ", "SBC", "???", "???", "???", "SBC", "INC", "???",
        "SED", "SBC", "???", "???", "???", "SBC", "INC", "???"
    };

    static const uint8_t lengths[256] = {
        1,2,1,1,1,2,2,1, 1,2,1,1,1,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
        3,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
        1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
        1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
        1,2,1,1,2,2,2,1, 1,1,1,1,3,3,3,1,
        2,2,1,1,2,2,2,1, 1,3,1,1,1,3,1,1,
        2,2,2,1,2,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,2,2,2,1, 1,3,1,1,3,3,3,1,
        2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
        2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
        2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1
    };

    uint16_t currentAddr = addr;
    for (int i = 0; i < numLines && currentAddr <= 0xFFFF; i++) {
        uint8_t opcode = mem[currentAddr];
        uint8_t len = lengths[opcode];

        std::string bytes = ToHex8(opcode);
        std::string operand;

        if (len >= 2 && (currentAddr + 1) <= 0xFFFF) {
            uint8_t op1 = mem[currentAddr + 1];
            bytes += " " + ToHex8(op1);
            if (len == 2) {
                operand = ToHex8(op1);
            }
        }
        if (len >= 3 && (currentAddr + 2) <= 0xFFFF) {
            uint8_t op1 = mem[currentAddr + 1];
            uint8_t op2 = mem[currentAddr + 2];
            bytes += " " + ToHex8(op2);
            uint16_t word = op1 | (op2 << 8);
            operand = ToHex16(word);
        }

        // Build disassembly string: "MNEMONIC OPERAND"
        std::string disasm = mnemonics[opcode];
        if (!operand.empty()) {
            disasm += " " + operand;
        }

        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(currentAddr);
        extra["idx"] = std::to_string(i);
        lines.push_back(FormatLine("dbg", "disasm", "line", disasm, extra));

        currentAddr += len;
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetCPUStack() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    uint8_t sp = regs.sp & 0xFF;
    uint16_t stackBase = 0x0100;

    // Stack pointer
    lines.push_back(FormatLine("cpu", "stack", "sp", ToHex8(sp)));
    lines.push_back(FormatLine("cpu", "stack", "depth", std::to_string(0xFF - sp)));

    // Stack contents (up to 16 entries from current SP)
    for (int i = sp + 1; i <= 0xFF && i <= sp + 16; i++) {
        uint16_t addr = stackBase + i;
        uint8_t value = mem[addr];

        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(addr);
        extra["idx"] = std::to_string(i - sp - 1);
        lines.push_back(FormatLine("cpu", "stack", "val", ToHex8(value), extra));
    }

    return lines;
}

//-----------------------------------------------------------------------------
// Extended Memory Information (65504 compatibility)
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamProvider::GetZeroPageDump() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Zero page: $0000-$00FF (256 bytes)
    // Output as 16 lines of 16 bytes each
    for (uint16_t addr = 0x0000; addr < 0x0100; addr += 16) {
        std::string hexData;
        for (int i = 0; i < 16; i++) {
            if (i > 0) hexData += " ";
            hexData += ToHex8(mem[addr + i]);
        }

        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(addr);
        extra["len"] = "16";
        lines.push_back(FormatLine("mem", "zp", "data", hexData, extra));
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetStackPageDump() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Stack page: $0100-$01FF (256 bytes)
    // Output as 16 lines of 16 bytes each
    for (uint16_t addr = 0x0100; addr < 0x0200; addr += 16) {
        std::string hexData;
        for (int i = 0; i < 16; i++) {
            if (i > 0) hexData += " ";
            hexData += ToHex8(mem[addr + i]);
        }

        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(addr);
        extra["len"] = "16";
        lines.push_back(FormatLine("mem", "stack", "data", hexData, extra));
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetTextScreen() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Text screen page determination
    bool page2 = (GetMemMode() & MF_PAGE2) != 0;
    uint16_t baseAddr = page2 ? 0x0800 : 0x0400;

    lines.push_back(FormatLine("mem", "text", "page", page2 ? "2" : "1"));

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
            if (ch >= 0xA0 && ch <= 0xFF) {
                text += static_cast<char>(ch & 0x7F);
            } else if (ch >= 0x80 && ch <= 0x9F) {
                text += static_cast<char>(ch & 0x7F);
            } else if (ch >= 0x20 && ch <= 0x7E) {
                text += static_cast<char>(ch);
            } else {
                text += '.';
            }
        }

        std::map<std::string, std::string> extra;
        extra["idx"] = std::to_string(row);
        extra["addr"] = ToHex16(rowAddr);
        lines.push_back(FormatLine("mem", "text", "row", EscapeJson(text), extra));
    }

    return lines;
}

std::vector<std::string> DebugStreamProvider::GetMemoryDumpAt(uint16_t startAddr, int bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // Limit to reasonable size
    if (bytes > 256) bytes = 256;
    if (bytes < 1) bytes = 16;

    // Round to 16-byte lines
    int numLines = (bytes + 15) / 16;

    uint16_t addr = startAddr;
    for (int line = 0; line < numLines && addr <= 0xFFFF; line++) {
        std::string hexData;
        int bytesInLine = std::min(16, static_cast<int>(0x10000 - addr));

        for (int i = 0; i < bytesInLine; i++) {
            if (i > 0) hexData += " ";
            hexData += ToHex8(mem[addr + i]);
        }

        std::map<std::string, std::string> extra;
        extra["addr"] = ToHex16(addr);
        extra["len"] = std::to_string(bytesInLine);
        lines.push_back(FormatLine("mem", "dump", "data", hexData, extra));

        addr += 16;
    }

    return lines;
}

//-----------------------------------------------------------------------------
// Full State Snapshot
//-----------------------------------------------------------------------------

std::vector<std::string> DebugStreamProvider::GetFullSnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> lines;

    // ===== Machine info =====
    const char* machineType = "Unknown";
    switch (GetApple2Type()) {
        case A2TYPE_APPLE2:      machineType = "Apple2"; break;
        case A2TYPE_APPLE2PLUS:  machineType = "Apple2Plus"; break;
        case A2TYPE_APPLE2JPLUS: machineType = "Apple2JPlus"; break;
        case A2TYPE_APPLE2E:     machineType = "Apple2e"; break;
        case A2TYPE_APPLE2EENHANCED: machineType = "Apple2eEnhanced"; break;
        case A2TYPE_APPLE2C:     machineType = "Apple2c"; break;
        case A2TYPE_PRAVETS82:   machineType = "Pravets82"; break;
        case A2TYPE_PRAVETS8M:   machineType = "Pravets8M"; break;
        case A2TYPE_PRAVETS8A:   machineType = "Pravets8A"; break;
        case A2TYPE_TK30002E:    machineType = "TK30002e"; break;
        case A2TYPE_BASE64A:     machineType = "Base64A"; break;
        default: break;
    }
    lines.push_back(FormatLine("mach", "info", "type", machineType));

    // ===== CPU type (NEW - from 65501) =====
    const char* cpuType = "Unknown";
    switch (GetMainCpu()) {
        case CPU_6502:  cpuType = "6502"; break;
        case CPU_65C02: cpuType = "65C02"; break;
        case CPU_Z80:   cpuType = "Z80"; break;
        default: break;
    }
    lines.push_back(FormatLine("mach", "info", "cpuType", cpuType));

    // ===== Memory mode (get early for videoMode calculation) =====
    UINT memMode = GetMemMode();

    // ===== Video mode (NEW - from 65501) =====
    const char* videoMode = "Unknown";
    if (memMode & MF_HIRES) {
        videoMode = (memMode & MF_80STORE) ? "DoubleHiRes" : "HiRes";
    } else {
        videoMode = (memMode & MF_80STORE) ? "80ColText" : "TextLoRes";
    }
    lines.push_back(FormatLine("mach", "info", "videoMode", videoMode));

    // ===== Machine status =====
    const char* mode = "unknown";
    switch (g_nAppMode) {
        case MODE_LOGO:      mode = "logo"; break;
        case MODE_RUNNING:   mode = "running"; break;
        case MODE_DEBUG:     mode = "debug"; break;
        case MODE_STEPPING:  mode = "stepping"; break;
        case MODE_PAUSED:    mode = "paused"; break;
        case MODE_BENCHMARK: mode = "benchmark"; break;
        default: break;
    }
    lines.push_back(FormatLine("mach", "status", "mode", mode));

    // ===== Cumulative cycles (NEW - from 65501) =====
    lines.push_back(FormatLine("mach", "info", "cycles", std::to_string(g_nCumulativeCycles)));

    // ===== CPU registers =====
    lines.push_back(FormatLine("cpu", "reg", "a", ToHex8(regs.a)));
    lines.push_back(FormatLine("cpu", "reg", "x", ToHex8(regs.x)));
    lines.push_back(FormatLine("cpu", "reg", "y", ToHex8(regs.y)));
    lines.push_back(FormatLine("cpu", "reg", "pc", ToHex16(regs.pc)));
    lines.push_back(FormatLine("cpu", "reg", "sp", ToHex8(static_cast<uint8_t>(regs.sp & 0xFF))));
    lines.push_back(FormatLine("cpu", "reg", "p", ToHex8(regs.ps)));

    // ===== CPU flags =====
    uint8_t ps = regs.ps;
    lines.push_back(FormatLine("cpu", "flag", "n", (ps & AF_SIGN) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "v", (ps & AF_OVERFLOW) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "b", (ps & AF_BREAK) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "d", (ps & AF_DECIMAL) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "i", (ps & AF_INTERRUPT) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "z", (ps & AF_ZERO) ? "1" : "0"));
    lines.push_back(FormatLine("cpu", "flag", "c", (ps & AF_CARRY) ? "1" : "0"));

    // ===== CPU state =====
    lines.push_back(FormatLine("cpu", "state", "jammed", regs.bJammed ? "1" : "0"));

    // ===== Memory bank mode =====
    lines.push_back(FormatLine("mem", "bank", "mode", ToHex8(static_cast<uint8_t>(memMode & 0xFF))));

    // ===== Memory flags (NEW - from 65501) =====
    lines.push_back(FormatLine("mem", "flag", "80store", (memMode & MF_80STORE) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "auxRead", (memMode & MF_AUXREAD) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "auxWrite", (memMode & MF_AUXWRITE) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "altZP", (memMode & MF_ALTZP) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "highRam", (memMode & MF_HIGHRAM) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "bank2", (memMode & MF_BANK2) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "writeRam", (memMode & MF_WRITERAM) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "page2", (memMode & MF_PAGE2) ? "1" : "0"));
    lines.push_back(FormatLine("mem", "flag", "hires", (memMode & MF_HIRES) ? "1" : "0"));

    // =========================================================================
    // NEW: I/O Information (65502 compatibility)
    // =========================================================================

    // ===== Soft switches =====
    lines.push_back(FormatLine("io", "switch", "80store", (memMode & MF_80STORE) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "ramrd", (memMode & MF_AUXREAD) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "ramwrt", (memMode & MF_AUXWRITE) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "altzp", (memMode & MF_ALTZP) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "page2", (memMode & MF_PAGE2) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "hires", (memMode & MF_HIRES) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "lcram", (memMode & MF_HIGHRAM) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "lcbank2", (memMode & MF_BANK2) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "lcwrite", (memMode & MF_WRITERAM) ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "intcxrom", MemCheckINTCXROM() ? "1" : "0"));
    lines.push_back(FormatLine("io", "switch", "slotc3rom", MemCheckSLOTC3ROM() ? "1" : "0"));

    // ===== Slot information =====
    CardManager& cardMgr = GetCardMgr();
    for (int slot = 0; slot <= 7; slot++) {
        SS_CARDTYPE cardType = cardMgr.QuerySlot(slot);
        std::string typeName = (cardType != CT_Empty) ? Card::GetCardName(cardType) : "Empty";

        std::map<std::string, std::string> slotExtra;
        slotExtra["idx"] = std::to_string(slot);
        lines.push_back(FormatLine("io", "slot", "type", typeName, slotExtra));

        slotExtra.clear();
        slotExtra["idx"] = std::to_string(slot);
        lines.push_back(FormatLine("io", "slot", "active", (cardType != CT_Empty) ? "1" : "0", slotExtra));
    }

    // ===== Annunciators =====
    for (int i = 0; i < 4; i++) {
        std::map<std::string, std::string> annExtra;
        annExtra["idx"] = std::to_string(i);
        lines.push_back(FormatLine("io", "ann", "state", MemGetAnnunciator(i) ? "1" : "0", annExtra));
    }

    // =========================================================================
    // NEW: Extended CPU Information (65503 compatibility)
    // =========================================================================

    // ===== Breakpoints =====
    lines.push_back(FormatLine("dbg", "bp", "count", std::to_string(g_nBreakpoints)));
    for (int i = 0; i < MAX_BREAKPOINTS; i++) {
        const Breakpoint_t& bp = g_aBreakpoints[i];
        if (bp.bSet) {
            std::map<std::string, std::string> bpExtra;
            bpExtra["idx"] = std::to_string(i);
            bpExtra["addr"] = ToHex16(bp.nAddress);
            lines.push_back(FormatLine("dbg", "bp", "enabled", bp.bEnabled ? "1" : "0", bpExtra));

            bpExtra.clear();
            bpExtra["idx"] = std::to_string(i);
            lines.push_back(FormatLine("dbg", "bp", "source", g_aBreakpointSource[bp.eSource], bpExtra));

            bpExtra.clear();
            bpExtra["idx"] = std::to_string(i);
            lines.push_back(FormatLine("dbg", "bp", "hits", std::to_string(bp.nHitCount), bpExtra));
        }
    }

    // ===== CPU Stack contents =====
    {
        uint8_t sp = regs.sp & 0xFF;
        lines.push_back(FormatLine("cpu", "stack", "sp", ToHex8(sp)));
        lines.push_back(FormatLine("cpu", "stack", "depth", std::to_string(0xFF - sp)));

        // Stack contents (up to 16 entries from current SP)
        for (int i = sp + 1; i <= 0xFF && i <= sp + 16; i++) {
            uint16_t addr = 0x0100 + i;
            uint8_t value = mem[addr];

            std::map<std::string, std::string> stackExtra;
            stackExtra["addr"] = ToHex16(addr);
            stackExtra["idx"] = std::to_string(i - sp - 1);
            lines.push_back(FormatLine("cpu", "stack", "val", ToHex8(value), stackExtra));
        }
    }

    // ===== Disassembly around PC =====
    {
        uint16_t disasmAddr = regs.pc;
        static const char* mnemonics[256] = {
            "BRK", "ORA", "???", "???", "???", "ORA", "ASL", "???",
            "PHP", "ORA", "ASL", "???", "???", "ORA", "ASL", "???",
            "BPL", "ORA", "???", "???", "???", "ORA", "ASL", "???",
            "CLC", "ORA", "???", "???", "???", "ORA", "ASL", "???",
            "JSR", "AND", "???", "???", "BIT", "AND", "ROL", "???",
            "PLP", "AND", "ROL", "???", "BIT", "AND", "ROL", "???",
            "BMI", "AND", "???", "???", "???", "AND", "ROL", "???",
            "SEC", "AND", "???", "???", "???", "AND", "ROL", "???",
            "RTI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
            "PHA", "EOR", "LSR", "???", "JMP", "EOR", "LSR", "???",
            "BVC", "EOR", "???", "???", "???", "EOR", "LSR", "???",
            "CLI", "EOR", "???", "???", "???", "EOR", "LSR", "???",
            "RTS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
            "PLA", "ADC", "ROR", "???", "JMP", "ADC", "ROR", "???",
            "BVS", "ADC", "???", "???", "???", "ADC", "ROR", "???",
            "SEI", "ADC", "???", "???", "???", "ADC", "ROR", "???",
            "???", "STA", "???", "???", "STY", "STA", "STX", "???",
            "DEY", "???", "TXA", "???", "STY", "STA", "STX", "???",
            "BCC", "STA", "???", "???", "STY", "STA", "STX", "???",
            "TYA", "STA", "TXS", "???", "???", "STA", "???", "???",
            "LDY", "LDA", "LDX", "???", "LDY", "LDA", "LDX", "???",
            "TAY", "LDA", "TAX", "???", "LDY", "LDA", "LDX", "???",
            "BCS", "LDA", "???", "???", "LDY", "LDA", "LDX", "???",
            "CLV", "LDA", "TSX", "???", "LDY", "LDA", "LDX", "???",
            "CPY", "CMP", "???", "???", "CPY", "CMP", "DEC", "???",
            "INY", "CMP", "DEX", "???", "CPY", "CMP", "DEC", "???",
            "BNE", "CMP", "???", "???", "???", "CMP", "DEC", "???",
            "CLD", "CMP", "???", "???", "???", "CMP", "DEC", "???",
            "CPX", "SBC", "???", "???", "CPX", "SBC", "INC", "???",
            "INX", "SBC", "NOP", "???", "CPX", "SBC", "INC", "???",
            "BEQ", "SBC", "???", "???", "???", "SBC", "INC", "???",
            "SED", "SBC", "???", "???", "???", "SBC", "INC", "???"
        };
        static const uint8_t lengths[256] = {
            1,2,1,1,1,2,2,1, 1,2,1,1,1,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
            3,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
            1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
            1,2,1,1,1,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
            1,2,1,1,2,2,2,1, 1,1,1,1,3,3,3,1,
            2,2,1,1,2,2,2,1, 1,3,1,1,1,3,1,1,
            2,2,2,1,2,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,2,2,2,1, 1,3,1,1,3,3,3,1,
            2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1,
            2,2,1,1,2,2,2,1, 1,2,1,1,3,3,3,1,
            2,2,1,1,1,2,2,1, 1,3,1,1,1,3,3,1
        };

        for (int i = 0; i < 8 && disasmAddr <= 0xFFFF; i++) {
            uint8_t opcode = mem[disasmAddr];
            uint8_t len = lengths[opcode];

            std::string operand;
            if (len >= 2 && (disasmAddr + 1) <= 0xFFFF) {
                uint8_t op1 = mem[disasmAddr + 1];
                if (len == 2) {
                    operand = ToHex8(op1);
                }
            }
            if (len >= 3 && (disasmAddr + 2) <= 0xFFFF) {
                uint8_t op1 = mem[disasmAddr + 1];
                uint8_t op2 = mem[disasmAddr + 2];
                uint16_t word = op1 | (op2 << 8);
                operand = ToHex16(word);
            }

            std::string disasmStr = mnemonics[opcode];
            if (!operand.empty()) {
                disasmStr += " " + operand;
            }

            std::map<std::string, std::string> disasmExtra;
            disasmExtra["addr"] = ToHex16(disasmAddr);
            disasmExtra["idx"] = std::to_string(i);
            lines.push_back(FormatLine("dbg", "disasm", "line", disasmStr, disasmExtra));

            disasmAddr += len;
        }
    }

    // =========================================================================
    // NEW: Memory Information (65504 compatibility)
    // =========================================================================

    // ===== Zero Page dump ($0000-$00FF) =====
    for (uint16_t addr = 0x0000; addr < 0x0100; addr += 16) {
        std::string hexData;
        for (int i = 0; i < 16; i++) {
            if (i > 0) hexData += " ";
            hexData += ToHex8(mem[addr + i]);
        }

        std::map<std::string, std::string> zpExtra;
        zpExtra["addr"] = ToHex16(addr);
        zpExtra["len"] = "16";
        lines.push_back(FormatLine("mem", "zp", "data", hexData, zpExtra));
    }

    // ===== Stack Page dump ($0100-$01FF) =====
    for (uint16_t addr = 0x0100; addr < 0x0200; addr += 16) {
        std::string hexData;
        for (int i = 0; i < 16; i++) {
            if (i > 0) hexData += " ";
            hexData += ToHex8(mem[addr + i]);
        }

        std::map<std::string, std::string> stackPageExtra;
        stackPageExtra["addr"] = ToHex16(addr);
        stackPageExtra["len"] = "16";
        lines.push_back(FormatLine("mem", "stackpage", "data", hexData, stackPageExtra));
    }

    // ===== Text screen =====
    {
        bool page2 = (memMode & MF_PAGE2) != 0;
        uint16_t baseAddr = page2 ? 0x0800 : 0x0400;

        lines.push_back(FormatLine("mem", "text", "page", page2 ? "2" : "1"));

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
                if (ch >= 0xA0 && ch <= 0xFF) {
                    text += static_cast<char>(ch & 0x7F);
                } else if (ch >= 0x80 && ch <= 0x9F) {
                    text += static_cast<char>(ch & 0x7F);
                } else if (ch >= 0x20 && ch <= 0x7E) {
                    text += static_cast<char>(ch);
                } else {
                    text += '.';
                }
            }

            std::map<std::string, std::string> textExtra;
            textExtra["idx"] = std::to_string(row);
            textExtra["addr"] = ToHex16(rowAddr);
            lines.push_back(FormatLine("mem", "text", "row", EscapeJson(text), textExtra));
        }
    }

    return lines;
}

//-----------------------------------------------------------------------------
// Utility Methods
//-----------------------------------------------------------------------------

int64_t DebugStreamProvider::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string DebugStreamProvider::FormatLine(const char* cat, const char* sec, const char* fld,
                                            const std::string& val,
                                            const std::map<std::string, std::string>& extra) {
    std::ostringstream json;

    json << "{\"emu\":\"apple\""
         << ",\"cat\":\"" << cat << "\""
         << ",\"sec\":\"" << sec << "\""
         << ",\"fld\":\"" << fld << "\""
         << ",\"val\":\"" << val << "\"";

    for (const auto& kv : extra) {
        json << ",\"" << kv.first << "\":\"" << kv.second << "\"";
    }

    json << "}";
    return json.str();
}

std::string DebugStreamProvider::ToHex8(uint8_t value) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(value);
    return oss.str();
}

std::string DebugStreamProvider::ToHex16(uint16_t value) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
        << static_cast<int>(value);
    return oss.str();
}

std::string DebugStreamProvider::EscapeJson(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 16);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - escape as \u00XX
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
                    result += oss.str();
                } else {
                    result += c;
                }
                break;
        }
    }

    return result;
}

} // namespace debugserver
