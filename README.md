# AppleWin for Linux (Extended Fork)

This is an extended fork of [AppleWin Linux port](https://github.com/audetto/AppleWin) by audetto, which is based on the original [AppleWin](https://github.com/AppleWin/AppleWin) Windows emulator.

## Upstream Projects

| Project | URL | Description |
|---------|-----|-------------|
| AppleWin (Original) | https://github.com/AppleWin/AppleWin | Apple II emulator for Windows |
| AppleWin Linux Port | https://github.com/audetto/AppleWin | Linux port by audetto |

## What's New in This Fork

### Debug HTTP Server

A built-in HTTP debug server provides real-time access to emulator state via web browser or API calls. The server starts automatically when the emulator runs.

| Port  | Description | Dashboard URL |
|-------|-------------|---------------|
| 65501 | Machine Info (Apple II type, mode, memory state) | http://127.0.0.1:65501/ |
| 65502 | I/O Info (soft switches, slot cards, annunciators) | http://127.0.0.1:65502/ |
| 65503 | CPU Info (registers, flags, breakpoints, disassembly) | http://127.0.0.1:65503/ |
| 65504 | Memory Info (memory dumps, zero page, stack) | http://127.0.0.1:65504/ |

**API Examples:**

```bash
# Get machine info
curl http://127.0.0.1:65501/api/info

# Get CPU registers
curl http://127.0.0.1:65503/api/registers

# Dump memory at address $C600
curl "http://127.0.0.1:65504/api/dump?addr=0xC600&lines=8"

# Get expansion slot cards
curl http://127.0.0.1:65502/api/slots
```

See [Debug Server Documentation](source/debugserver/README.md) for full API reference.

### Debug Stream Server (Telnet)

A real-time debug streaming server provides continuous emulator state output via TCP/Telnet connection. Data is output in JSON Lines format, suitable for real-time monitoring and integration with external tools.

| Port  | Description | Connection |
|-------|-------------|------------|
| 65505 | Debug Stream (JSON Lines) | `telnet 127.0.0.1 65505` or `nc 127.0.0.1 65505` |

**Output Format (JSON Lines):**

Each line is an independent JSON object with the following structure:
```json
{"emu":"apple","cat":"cpu","sec":"reg","fld":"pc","val":"C600"}
{"emu":"apple","cat":"cpu","sec":"reg","fld":"a","val":"00"}
{"emu":"apple","cat":"mem","sec":"write","fld":"byte","val":"20","addr":"0300"}
```

**Key Fields:**
| Field | Description |
|-------|-------------|
| `emu` | Emulator identifier (`apple`) |
| `cat` | Category (`cpu`, `mem`, `io`, `mach`, `dbg`, `sys`) |
| `sec` | Section (e.g., `reg`, `flag`, `dump`) |
| `fld` | Field name |
| `val` | Value (always string) |

**Usage Examples:**

```bash
# Connect and view real-time stream
telnet 127.0.0.1 65505

# Using netcat
nc 127.0.0.1 65505

# Filter CPU register changes
nc 127.0.0.1 65505 | grep '"cat":"cpu"'

# Save to file
nc 127.0.0.1 65505 > debug_log.jsonl
```

See [Debug Stream Specification](RetroDeveloperEnvironmentProject_OUTPUT_SPEC_V01.md) for full protocol documentation.

## Apple II Models Supported

* Apple ][
* Apple ][+
* Apple //e
* Apple //e Enhanced
* Various clones (Pravets, TK3000, Base 64)

## Peripheral Cards Supported

- Mockingboard, Phasor and SAM sound cards
- Disk II interface for floppy disk drives
- Hard disk controller
- Super Serial Card (SSC)
- Parallel printer card
- Mouse interface
- Apple IIe Extended 80-Column Text Card and RamWorks III (8MB)
- RGB cards: Apple's Extended 80-Column Text/AppleColor Adaptor Card and 'Le Chat Mauve' Féline
- CP/M SoftCard
- Uthernet I and II (ethernet cards)
- Language Card and Saturn 64/128K
- VidHD card
- No Slot Clock (NSC)

## Building

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt-get -y install $(cat source/linux/raspbian.list.txt)
```

**Fedora:**
```bash
# Install packages from source/linux/fedora.list.txt
```

### Checkout

```bash
git clone --recursive <repository-url>
```

### Build

```bash
cd AppleWin
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
make
```

### Frontend Selection

```bash
# Build only SDL2 frontend
cmake -DBUILD_SA2=ON -DBUILD_QAPPLE=OFF -DBUILD_APPLEN=OFF -DBUILD_LIBRETRO=OFF ..
```

Available options: `BUILD_APPLEN`, `BUILD_QAPPLE`, `BUILD_SA2`, `BUILD_LIBRETRO`

## Running

```bash
./sa2
```

The Debug HTTP Server will start automatically on ports 65501-65504, and the Debug Stream Server on port 65505.

## Frontends

| Frontend | Description |
|----------|-------------|
| **sa2** | SDL2 + ImGui (recommended) |
| applen | ncurses with ASCII art graphics |
| qapple | Qt5/Qt6 frontend |
| ra2 | libretro core |

See [.github/README.md](.github/README.md) for detailed documentation on each frontend.

## Documentation

- [Debug Server API](source/debugserver/README.md) - HTTP debug server documentation
- [Debug Stream Specification](RetroDeveloperEnvironmentProject_OUTPUT_SPEC_V01.md) - Telnet debug stream protocol
- [SDL2 Frontend](source/frontends/sdl/README.md) - sa2 specific options
- [Linux Port Details](.github/README.md) - Full Linux port documentation
- [Network Setup](source/Tfe/README.md) - Uthernet/network configuration

## License

GPL-2.0 - Same as AppleWin

## Credits

- [AppleWin Team](https://github.com/AppleWin/AppleWin) - Original Windows emulator
- [audetto](https://github.com/audetto/AppleWin) - Linux port
