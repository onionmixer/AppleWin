# AppleWin Debug HTTP Server

HTTP-based debugging interface for AppleWin emulator.

## Features

- **No external dependencies** - Pure C++17 implementation
- **GPL-2.0 License** - Compatible with AppleWin
- **Cross-platform** - Works on Linux (POSIX) and Windows (Winsock)
- **4 separate debug ports** for different information categories
- **JSON API** - Easy integration with external tools
- **HTML Dashboard** - Real-time browser-based monitoring

## Server Ports

| Port  | Provider         | Description                         |
|-------|------------------|-------------------------------------|
| 65501 | MachineInfo      | Apple II type, mode, memory state   |
| 65502 | IOInfo           | Soft switches, slot cards, annunciators |
| 65503 | CPUInfo          | Registers, flags, breakpoints, disasm |
| 65504 | MemoryInfo       | Memory dumps, zero page, stack      |

## Integration with AppleWin

### Step 1: Include the Header

```cpp
#include "debugserver/DebugServerManager.h"
```

### Step 2: Start Debug Server (during initialization)

In your AppleWin initialization code (e.g., after `MemInitialize()`):

```cpp
// Option A: Using C++ interface
debugserver::DebugServerManager::GetInstance().Start();

// Option B: Using C-style interface
DebugServer_Start();
```

### Step 3: Stop Debug Server (during shutdown)

In your AppleWin cleanup code (e.g., before `MemDestroy()`):

```cpp
// Option A: Using C++ interface
debugserver::DebugServerManager::GetInstance().Stop();

// Option B: Using C-style interface
DebugServer_Stop();
```

### Example Integration (Linux/SDL2 Frontend)

```cpp
// In main() or initialization function
void InitializeEmulator() {
    // ... existing initialization code ...

    MemInitialize();
    CpuInitialize();

    // Start debug server
    if (DebugServer_Start()) {
        printf("Debug server started on ports 65501-65504\n");
    }

    // ... rest of initialization ...
}

void ShutdownEmulator() {
    // Stop debug server first
    DebugServer_Stop();

    // ... existing cleanup code ...
    CpuDestroy();
    MemDestroy();
}
```

## API Reference

### Machine Info (Port 65501)

```
GET /                    - HTML Dashboard
GET /api/status          - Server status
GET /api/info            - Machine information
```

Example response for `/api/info`:
```json
{
  "apple2Type": "Enhanced Apple //e",
  "cpuType": "65C02 (CMOS)",
  "mode": "Running",
  "videoMode": "Hi-Res",
  "memory": {
    "memMode": 96,
    "80store": false,
    "auxRead": false,
    "auxWrite": false,
    "altZP": false,
    "highRam": true,
    "bank2": true,
    "writeRam": false,
    "page2": false,
    "hires": true
  }
}
```

### CPU Info (Port 65503)

```
GET /                    - HTML Dashboard
GET /api/registers       - CPU registers
GET /api/flags           - CPU flags
GET /api/breakpoints     - Breakpoint list
GET /api/disasm?addr=$XXXX&lines=N  - Disassembly
GET /api/stack           - Stack contents
```

Example response for `/api/registers`:
```json
{
  "A": "$00",
  "X": "$01",
  "Y": "$02",
  "PC": "$C600",
  "SP": "$FF",
  "P": "$30",
  "jammed": false,
  "decimal": {
    "A": 0,
    "X": 1,
    "Y": 2,
    "PC": 50688,
    "SP": 255
  }
}
```

### I/O Info (Port 65502)

```
GET /                    - HTML Dashboard
GET /api/softswitches    - Soft switch states
GET /api/slots           - Expansion slot info
GET /api/annunciators    - Annunciator states
```

### Memory Info (Port 65504)

```
GET /                    - HTML Dashboard
GET /api/dump?addr=$XXXX&lines=N&width=16  - Memory dump
GET /api/read?addr=$XXXX&len=N             - Read bytes
GET /api/zeropage        - Zero page dump
GET /api/stack           - Stack page dump
GET /api/textscreen      - Text screen contents
```

## Configuration

### Bind Address

By default, the server binds to `127.0.0.1` (localhost only) for security.
To allow external access:

```cpp
debugserver::DebugServerManager::GetInstance().SetBindAddress("0.0.0.0");
debugserver::DebugServerManager::GetInstance().Start();
```

### Enable/Disable

```cpp
// Disable debug server
DebugServer_SetEnabled(false);

// Check if enabled
if (DebugServer_IsEnabled()) { ... }
```

## File Structure

```
source/debugserver/
├── HttpRequest.h/cpp       - HTTP request parser
├── HttpResponse.h/cpp      - HTTP response builder
├── HttpServer.h/cpp        - TCP socket server
├── JsonBuilder.h/cpp       - JSON generator
├── SimpleTemplate.h/cpp    - HTML template engine
├── InfoProvider.h/cpp      - Base provider interface
├── MachineInfoProvider.h/cpp
├── CPUInfoProvider.h/cpp
├── IOInfoProvider.h/cpp
├── MemoryInfoProvider.h/cpp
├── DebugServerManager.h/cpp - Main manager (singleton)
└── CMakeLists.txt
```

## Building

The debugserver is built as a static library. Add to your CMakeLists.txt:

```cmake
add_subdirectory(source/debugserver)
target_link_libraries(your_target debugserver)
```

## Security Notes

- Default bind address is localhost only (`127.0.0.1`)
- No authentication is implemented
- Do not expose to public networks without additional security measures
- Consider using a reverse proxy with authentication for remote access

## License

GPL-2.0 - Same as AppleWin
