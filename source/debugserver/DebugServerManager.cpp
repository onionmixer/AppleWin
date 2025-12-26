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

#include "DebugServerManager.h"
#include <iostream>

namespace debugserver {

DebugServerManager& DebugServerManager::GetInstance() {
    static DebugServerManager instance;
    return instance;
}

DebugServerManager::DebugServerManager()
    : m_enabled(true)
    , m_bindAddress("127.0.0.1")
    , m_running(false)
{
    // Create providers
    m_machineProvider = std::make_unique<MachineInfoProvider>();
    m_cpuProvider = std::make_unique<CPUInfoProvider>();
    m_ioProvider = std::make_unique<IOInfoProvider>();
    m_memoryProvider = std::make_unique<MemoryInfoProvider>();
}

DebugServerManager::~DebugServerManager() {
    Stop();
}

std::unique_ptr<HttpServer> DebugServerManager::CreateServer(InfoProvider* provider) {
    auto server = std::make_unique<HttpServer>(provider->GetPort(), m_bindAddress);

    server->SetHandler([provider](const HttpRequest& request, HttpResponse& response) {
        provider->HandleRequest(request, response);
    });

    return server;
}

bool DebugServerManager::Start() {
    if (!m_enabled) {
        m_lastError = "Debug server is disabled";
        return false;
    }

    if (m_running.load()) {
        return true;  // Already running
    }

    m_lastError.clear();
    bool allStarted = true;

    // Create and start servers
    try {
        // Machine Info Server (port 65501)
        m_machineServer = CreateServer(m_machineProvider.get());
        if (!m_machineServer->Start()) {
            m_lastError += "Machine server failed: " + m_machineServer->GetLastError() + "\n";
            allStarted = false;
        }

        // CPU Info Server (port 65503)
        m_cpuServer = CreateServer(m_cpuProvider.get());
        if (!m_cpuServer->Start()) {
            m_lastError += "CPU server failed: " + m_cpuServer->GetLastError() + "\n";
            allStarted = false;
        }

        // IO Info Server (port 65502)
        m_ioServer = CreateServer(m_ioProvider.get());
        if (!m_ioServer->Start()) {
            m_lastError += "IO server failed: " + m_ioServer->GetLastError() + "\n";
            allStarted = false;
        }

        // Memory Info Server (port 65504)
        m_memoryServer = CreateServer(m_memoryProvider.get());
        if (!m_memoryServer->Start()) {
            m_lastError += "Memory server failed: " + m_memoryServer->GetLastError() + "\n";
            allStarted = false;
        }

    } catch (const std::exception& e) {
        m_lastError = std::string("Exception: ") + e.what();
        allStarted = false;
    }

    if (allStarted) {
        m_running.store(true);

        // Log startup info
        std::cout << "AppleWin Debug Server started:" << std::endl;
        std::cout << "  Machine Info: http://" << m_bindAddress << ":"
                  << static_cast<int>(DebugServerPort::Machine) << "/" << std::endl;
        std::cout << "  I/O Info:     http://" << m_bindAddress << ":"
                  << static_cast<int>(DebugServerPort::IO) << "/" << std::endl;
        std::cout << "  CPU Info:     http://" << m_bindAddress << ":"
                  << static_cast<int>(DebugServerPort::CPU) << "/" << std::endl;
        std::cout << "  Memory Info:  http://" << m_bindAddress << ":"
                  << static_cast<int>(DebugServerPort::Memory) << "/" << std::endl;
    } else {
        // Stop any servers that did start
        Stop();
    }

    return allStarted;
}

void DebugServerManager::Stop() {
    if (!m_running.load() &&
        !m_machineServer && !m_cpuServer && !m_ioServer && !m_memoryServer) {
        return;  // Nothing to stop
    }

    // Stop all servers
    if (m_machineServer) {
        m_machineServer->Stop();
        m_machineServer.reset();
    }

    if (m_cpuServer) {
        m_cpuServer->Stop();
        m_cpuServer.reset();
    }

    if (m_ioServer) {
        m_ioServer->Stop();
        m_ioServer.reset();
    }

    if (m_memoryServer) {
        m_memoryServer->Stop();
        m_memoryServer.reset();
    }

    m_running.store(false);
    std::cout << "AppleWin Debug Server stopped." << std::endl;
}

std::vector<DebugServerManager::ServerStatus> DebugServerManager::GetServerStatus() const {
    std::vector<ServerStatus> status;

    auto addStatus = [&](const char* name, uint16_t port, const HttpServer* server) {
        ServerStatus s;
        s.name = name;
        s.port = port;
        s.running = server && server->IsRunning();
        s.error = server ? server->GetLastError() : "Not created";
        status.push_back(s);
    };

    addStatus("Machine Info", static_cast<uint16_t>(DebugServerPort::Machine), m_machineServer.get());
    addStatus("I/O Info", static_cast<uint16_t>(DebugServerPort::IO), m_ioServer.get());
    addStatus("CPU Info", static_cast<uint16_t>(DebugServerPort::CPU), m_cpuServer.get());
    addStatus("Memory Info", static_cast<uint16_t>(DebugServerPort::Memory), m_memoryServer.get());

    return status;
}

} // namespace debugserver

//-----------------------------------------------------------------------------
// C-style interface implementation
//-----------------------------------------------------------------------------

bool DebugServer_Start(void) {
    return debugserver::DebugServerManager::GetInstance().Start();
}

void DebugServer_Stop(void) {
    debugserver::DebugServerManager::GetInstance().Stop();
}

bool DebugServer_IsRunning(void) {
    return debugserver::DebugServerManager::GetInstance().IsRunning();
}

void DebugServer_SetEnabled(bool enabled) {
    debugserver::DebugServerManager::GetInstance().SetEnabled(enabled);
}

bool DebugServer_IsEnabled(void) {
    return debugserver::DebugServerManager::GetInstance().IsEnabled();
}
