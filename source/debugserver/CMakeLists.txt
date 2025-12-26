# AppleWin Debug HTTP Server
# License: GPL-2.0
# No external library dependencies - pure C++17 implementation

set(DEBUGSERVER_SOURCES
    # HTTP Server Core
    HttpRequest.cpp
    HttpResponse.cpp
    HttpServer.cpp
    # Utilities
    JsonBuilder.cpp
    SimpleTemplate.cpp
    # Information Providers
    InfoProvider.cpp
    MachineInfoProvider.cpp
    CPUInfoProvider.cpp
    IOInfoProvider.cpp
    MemoryInfoProvider.cpp
    # Manager
    DebugServerManager.cpp
)

set(DEBUGSERVER_HEADERS
    # HTTP Server Core
    HttpRequest.h
    HttpResponse.h
    HttpServer.h
    # Utilities
    JsonBuilder.h
    SimpleTemplate.h
    # Information Providers
    InfoProvider.h
    MachineInfoProvider.h
    CPUInfoProvider.h
    IOInfoProvider.h
    MemoryInfoProvider.h
    # Manager
    DebugServerManager.h
)

add_library(debugserver STATIC
    ${DEBUGSERVER_SOURCES}
    ${DEBUGSERVER_HEADERS}
)

target_include_directories(debugserver PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../linux/libwindows
)

# Require C++17
target_compile_features(debugserver PUBLIC cxx_std_17)

# Thread support (C++ standard library)
find_package(Threads REQUIRED)
target_link_libraries(debugserver PUBLIC Threads::Threads)

# Windows-specific: Link Winsock
if(WIN32)
    target_link_libraries(debugserver PUBLIC ws2_32)
endif()

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(debugserver PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
elseif(MSVC)
    target_compile_options(debugserver PRIVATE
        /W4
    )
endif()
