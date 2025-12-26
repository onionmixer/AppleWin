# Debug Server Integration Guide

## Integration Points

The debug server should be integrated into the following files:

### Linux Frontend: `source/linux/context.cpp`

```cpp
// Add include at the top:
#include "debugserver/DebugServerManager.h"

// Modify InitialiseEmulator() - add after DebugInitialize():
void InitialiseEmulator(const AppMode_e mode)
{
    g_nAppMode = mode;
    LogFileOutput("Initialisation\n");

    g_bFullSpeed = false;

    GetVideo().SetVidHD(false);
    LoadConfiguration(true);
    SetCurrentCLK6502();
    GetAppleWindowTitle();
    GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);

    SpkrInitialize();
    MemInitialize();

    CardManager &cardManager = GetCardMgr();
    cardManager.Reset(true);

    Snapshot_Startup();

    DebugInitialize();
    KeybReset();

    // === ADD THIS: Start Debug HTTP Server ===
    if (DebugServer_Start()) {
        LogFileOutput("Debug HTTP Server started on ports 65501-65504\n");
    }
}

// Modify DestroyEmulator() - add at the beginning:
void DestroyEmulator()
{
    // === ADD THIS: Stop Debug HTTP Server first ===
    DebugServer_Stop();

    CardManager &cardManager = GetCardMgr();
    cardManager.Destroy();

    Snapshot_Shutdown();
    MemDestroy();
    SpkrDestroy();
    CpuDestroy();
    DebugDestroy();
}
```

### Windows Frontend: `source/Windows/AppleWin.cpp`

Similar integration in WinMain():

```cpp
// Add include:
#include "debugserver/DebugServerManager.h"

// In initialization section (after DebugInitialize):
DebugServer_Start();

// In cleanup section (before other Destroy calls):
DebugServer_Stop();
```

## CMake Integration

In the main `source/CMakeLists.txt`, add:

```cmake
# Add debug server subdirectory
add_subdirectory(debugserver)

# Link to your target
target_link_libraries(applewin debugserver)
```

Or in your frontend's CMakeLists.txt:

```cmake
# For SDL2 frontend (sa2)
target_link_libraries(sa2
    appleii
    debugserver  # Add this
    ${SDL2_LIBRARIES}
    ...
)
```

## Conditional Compilation (Optional)

If you want to make the debug server optional:

```cmake
option(ENABLE_DEBUG_SERVER "Enable HTTP debug server" ON)

if(ENABLE_DEBUG_SERVER)
    add_subdirectory(debugserver)
    target_compile_definitions(applewin PRIVATE ENABLE_DEBUG_SERVER)
endif()
```

Then in code:

```cpp
#ifdef ENABLE_DEBUG_SERVER
#include "debugserver/DebugServerManager.h"
#endif

void InitialiseEmulator(const AppMode_e mode)
{
    // ... existing code ...

#ifdef ENABLE_DEBUG_SERVER
    if (DebugServer_Start()) {
        LogFileOutput("Debug HTTP Server started\n");
    }
#endif
}

void DestroyEmulator()
{
#ifdef ENABLE_DEBUG_SERVER
    DebugServer_Stop();
#endif

    // ... existing code ...
}
```

## Testing the Integration

1. Build and run AppleWin
2. Open browser to: http://127.0.0.1:65501/
3. You should see the Machine Info dashboard
4. Navigate to other ports:
   - http://127.0.0.1:65502/ - I/O Info
   - http://127.0.0.1:65503/ - CPU Info
   - http://127.0.0.1:65504/ - Memory Info

## API Testing with curl

```bash
# Machine info
curl http://127.0.0.1:65501/api/info

# CPU registers
curl http://127.0.0.1:65503/api/registers

# Memory dump
curl "http://127.0.0.1:65504/api/dump?addr=\$C600&lines=8"

# Breakpoints
curl http://127.0.0.1:65503/api/breakpoints
```
