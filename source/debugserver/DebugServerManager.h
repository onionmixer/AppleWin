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
 * Debug Server Manager
 * Manages all HTTP debug servers and their lifecycle
 */

#pragma once

#include "HttpServer.h"
#include "InfoProvider.h"
#include "MachineInfoProvider.h"
#include "CPUInfoProvider.h"
#include "IOInfoProvider.h"
#include "MemoryInfoProvider.h"

#include <memory>
#include <vector>
#include <string>
#include <atomic>

namespace debugserver {

/**
 * DebugServerManager - Singleton manager for all debug HTTP servers
 *
 * Usage:
 *   // During AppleWin initialization:
 *   DebugServerManager::GetInstance().Start();
 *
 *   // During AppleWin shutdown:
 *   DebugServerManager::GetInstance().Stop();
 *
 *   // Check status:
 *   if (DebugServerManager::GetInstance().IsRunning()) { ... }
 */
class DebugServerManager {
public:
    // Singleton access
    static DebugServerManager& GetInstance();

    // Prevent copying
    DebugServerManager(const DebugServerManager&) = delete;
    DebugServerManager& operator=(const DebugServerManager&) = delete;

    // Start all debug servers
    // Returns true if all servers started successfully
    bool Start();

    // Stop all debug servers
    void Stop();

    // Check if servers are running
    bool IsRunning() const { return m_running.load(); }

    // Enable/disable the debug server feature
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Get bind address (default: 127.0.0.1 for security)
    const std::string& GetBindAddress() const { return m_bindAddress; }
    void SetBindAddress(const std::string& address) { m_bindAddress = address; }

    // Get last error message
    const std::string& GetLastError() const { return m_lastError; }

    // Get server status info
    struct ServerStatus {
        std::string name;
        uint16_t port;
        bool running;
        std::string error;
    };
    std::vector<ServerStatus> GetServerStatus() const;

    // Get individual providers (for testing/advanced use)
    InfoProvider* GetMachineInfoProvider() { return m_machineProvider.get(); }
    InfoProvider* GetCPUInfoProvider() { return m_cpuProvider.get(); }
    InfoProvider* GetIOInfoProvider() { return m_ioProvider.get(); }
    InfoProvider* GetMemoryInfoProvider() { return m_memoryProvider.get(); }

private:
    // Private constructor for singleton
    DebugServerManager();
    ~DebugServerManager();

    // Create HTTP server for a provider
    std::unique_ptr<HttpServer> CreateServer(InfoProvider* provider);

    // Configuration
    bool m_enabled;
    std::string m_bindAddress;
    std::atomic<bool> m_running;
    std::string m_lastError;

    // Providers
    std::unique_ptr<MachineInfoProvider> m_machineProvider;
    std::unique_ptr<CPUInfoProvider> m_cpuProvider;
    std::unique_ptr<IOInfoProvider> m_ioProvider;
    std::unique_ptr<MemoryInfoProvider> m_memoryProvider;

    // HTTP Servers
    std::unique_ptr<HttpServer> m_machineServer;
    std::unique_ptr<HttpServer> m_cpuServer;
    std::unique_ptr<HttpServer> m_ioServer;
    std::unique_ptr<HttpServer> m_memoryServer;
};

} // namespace debugserver

//-----------------------------------------------------------------------------
// C-style interface for easy integration with AppleWin
//-----------------------------------------------------------------------------

// Initialize and start debug servers (call from AppleWin initialization)
// Returns true on success
bool DebugServer_Start(void);

// Stop and cleanup debug servers (call from AppleWin shutdown)
void DebugServer_Stop(void);

// Check if debug servers are running
bool DebugServer_IsRunning(void);

// Enable/disable debug server feature
void DebugServer_SetEnabled(bool enabled);
bool DebugServer_IsEnabled(void);
