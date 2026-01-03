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
 * Debug Stream Provider
 * Generates JSON Lines format output according to OUTPUT_SPEC_V01.md specification.
 * Each line is a complete JSON object with mandatory "emu":"apple" field.
 *
 * Format: {"emu":"apple","cat":"<category>","sec":"<section>","fld":"<field>","val":"<value>"}
 *
 * Categories:
 *   cpu  - CPU registers, flags, interrupt state
 *   mem  - Memory read/write, bank switching
 *   io   - I/O port access, soft switches
 *   mach - Machine info, status
 *   dbg  - Breakpoints, trace execution
 *   sys  - System messages (hello, goodbye, errors)
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace debugserver {

/**
 * DebugStreamProvider - Generates JSON Lines output for debug streaming
 *
 * Thread-safety: All methods are thread-safe for reading emulator state.
 *
 * Usage:
 *   DebugStreamProvider provider;
 *
 *   // On CPU step:
 *   std::string data = provider.GetCPURegisters();
 *   server.Broadcast(data);
 *
 *   // On breakpoint hit:
 *   std::string bp = provider.GetBreakpointHit(0, 0xC600);
 *   server.Broadcast(bp);
 */
class DebugStreamProvider {
public:
    DebugStreamProvider();
    ~DebugStreamProvider() = default;

    // Non-copyable
    DebugStreamProvider(const DebugStreamProvider&) = delete;
    DebugStreamProvider& operator=(const DebugStreamProvider&) = delete;

    //-------------------------------------------------------------------------
    // System Messages (cat: sys)
    //-------------------------------------------------------------------------

    // Hello message - sent on connection
    std::string GetHelloMessage();

    // Goodbye message - sent on disconnect
    std::string GetGoodbyeMessage();

    // Error message
    std::string GetErrorMessage(const std::string& error);

    //-------------------------------------------------------------------------
    // CPU Information (cat: cpu)
    //-------------------------------------------------------------------------

    // Get all CPU registers as multiple JSON lines
    std::string GetCPURegisters();

    // Get single register value
    std::string GetCPURegister(const std::string& regName);

    // Get all CPU flags as multiple JSON lines
    std::string GetCPUFlags();

    // Get CPU state (jammed, etc.)
    std::string GetCPUState();

    //-------------------------------------------------------------------------
    // Memory Information (cat: mem)
    //-------------------------------------------------------------------------

    // Memory read event
    std::string GetMemoryRead(uint16_t addr, uint8_t value);

    // Memory write event
    std::string GetMemoryWrite(uint16_t addr, uint8_t value);

    // Memory dump (multiple bytes)
    std::string GetMemoryDump(uint16_t startAddr, const std::vector<uint8_t>& data);

    // Memory bank/mode status
    std::string GetMemoryBankStatus();

    // Get all memory flags (soft switch states) as JSON line
    std::string GetMemoryFlags();

    //-------------------------------------------------------------------------
    // I/O Information (cat: io)
    //-------------------------------------------------------------------------

    // Soft switch read
    std::string GetSoftSwitchRead(uint16_t addr, uint8_t value);

    // Soft switch write
    std::string GetSoftSwitchWrite(uint16_t addr, uint8_t value);

    //-------------------------------------------------------------------------
    // Machine Information (cat: mach)
    //-------------------------------------------------------------------------

    // Machine type and configuration
    std::string GetMachineInfo();

    // Machine status (running, paused, stepping, etc.)
    std::string GetMachineStatus(const std::string& mode);

    //-------------------------------------------------------------------------
    // Debug Information (cat: dbg)
    //-------------------------------------------------------------------------

    // Breakpoint hit event
    std::string GetBreakpointHit(int index, uint16_t addr);

    // Trace execution event (single instruction)
    std::string GetTraceExec(uint16_t addr, const std::string& disasm);

    // Trace memory access during instruction
    std::string GetTraceMemory(uint16_t addr, uint8_t value, bool isWrite);

    //-------------------------------------------------------------------------
    // I/O Information (cat: io) - NEW for 65502 compatibility
    //-------------------------------------------------------------------------

    // Get all soft switch states
    std::vector<std::string> GetSoftSwitches();

    // Get expansion slot card info
    std::vector<std::string> GetSlotInfo();

    // Get annunciator states
    std::vector<std::string> GetAnnunciators();

    //-------------------------------------------------------------------------
    // Extended CPU Information (cat: dbg) - NEW for 65503 compatibility
    //-------------------------------------------------------------------------

    // Get all breakpoints list
    std::vector<std::string> GetBreakpointList();

    // Get disassembly around an address
    std::vector<std::string> GetDisassembly(uint16_t addr, int lines);

    // Get CPU stack contents
    std::vector<std::string> GetCPUStack();

    //-------------------------------------------------------------------------
    // Extended Memory Information (cat: mem) - NEW for 65504 compatibility
    //-------------------------------------------------------------------------

    // Get zero page dump ($0000-$00FF)
    std::vector<std::string> GetZeroPageDump();

    // Get stack page dump ($0100-$01FF)
    std::vector<std::string> GetStackPageDump();

    // Get text screen contents
    std::vector<std::string> GetTextScreen();

    // Get memory dump at specific address
    std::vector<std::string> GetMemoryDumpAt(uint16_t startAddr, int bytes);

    //-------------------------------------------------------------------------
    // Full State Snapshot
    //-------------------------------------------------------------------------

    // Get complete system state (for initial connection)
    // Now includes all data from 65501-65504
    std::vector<std::string> GetFullSnapshot();

    //-------------------------------------------------------------------------
    // Utility Methods
    //-------------------------------------------------------------------------

    // Get current timestamp in milliseconds
    static int64_t GetTimestamp();

private:
    // Format a single JSON line
    std::string FormatLine(const char* cat, const char* sec, const char* fld,
                           const std::string& val,
                           const std::map<std::string, std::string>& extra = {});

    // Hex formatting helpers
    static std::string ToHex8(uint8_t value);
    static std::string ToHex16(uint16_t value);

    // Escape string for JSON
    static std::string EscapeJson(const std::string& str);

    // Version info
    static constexpr const char* VERSION = "1.0";

    // Mutex for thread-safe access to emulator state
    mutable std::mutex m_mutex;
};

} // namespace debugserver
