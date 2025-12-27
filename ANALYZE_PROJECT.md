# AppleWin Linux 프로젝트 분석

## 1. 프로젝트 개요

AppleWin은 Windows용 Apple //e 에뮬레이터의 Linux/macOS 포트입니다.

- **원본 프로젝트**: https://github.com/AppleWin/AppleWin (Windows)
- **Linux 포트**: https://github.com/audetto/AppleWin
- **지원 CPU**: MOS 6502, WDC 65C02, Z80 (CP/M 카드)
- **지원 시스템**: Apple ][, Apple ][+, Apple //e, Enhanced Apple //e

---

## 2. 프로젝트 구조

### 2.1 최상위 디렉토리

```
AppleWin/
├── source/                    # 핵심 소스 코드 (16MB, 629+ 파일)
├── resource/                  # ROM, 이미지, 폰트 리소스
├── firmware/                  # 펌웨어 파일
├── libyaml/                   # YAML 라이브러리 (의존성)
├── minizip/                   # ZIP 압축 라이브러리 (의존성)
├── zlib/                      # 압축 라이브러리 (의존성)
├── test/                      # 테스트 코드
├── help/                      # 도움말 파일
├── bin/                       # 바이너리/심볼 파일
├── CMakeLists.txt             # CMake 빌드 설정 (루트)
├── AppleWin-VS2022.sln        # Visual Studio 2022 솔루션
└── .github/workflows/         # GitHub CI/CD 설정
```

### 2.2 source/ 디렉토리 구조

```
source/
├── CPU.cpp/h                  # CPU 에뮬레이션 (6502/65C02)
├── Memory.cpp/h               # 메모리 관리 (99KB)
├── Video.cpp/h                # 비디오 처리
├── NTSC.cpp/h                 # NTSC 신호 처리 (96KB)
├── Core.cpp/h                 # 코어 에뮬레이션 로직
├── Disk.cpp/h                 # 디스크 드라이브 에뮬레이션
├── Harddisk.cpp/h             # 하드디스크 지원
├── Speaker.cpp/h              # 스피커 에뮬레이션
├── Mockingboard.cpp/h         # Mockingboard 음성 카드
├── AY8910.cpp/h               # AY8913 PSG 칩
├── 6522.cpp/h                 # SY6522 VIA 칩
├── SSI263.cpp/h               # SSI263 음성 합성
│
├── CPU/                       # CPU 명령어 구현
│   ├── cpu6502.h              # 6502 CPU 정의
│   ├── cpu65C02.h             # 65C02 CPU 정의
│   ├── cpu_instructions.inl   # 명령어 구현 (21KB)
│   ├── cpu_general.inl        # 일반 CPU 로직
│   └── cpu_heatmap.inl        # 히트맵 지원
│
├── Debugger/                  # 디버거 모듈 (840KB)
│   ├── Debug.cpp/h            # 디버거 핵심 (262KB)
│   ├── Debugger_Types.h       # 타입/구조체 정의 (49KB)
│   ├── Debugger_Commands.cpp  # 명령어 테이블 (43KB)
│   ├── Debugger_Display.cpp   # 화면 출력 (116KB)
│   ├── Debugger_Disassembler.cpp  # 역어셈블러
│   ├── Debugger_Assembler.cpp # 어셈블러 (51KB)
│   ├── Debugger_Symbols.cpp   # 심볼 테이블 (30KB)
│   ├── Debugger_Parser.cpp    # 명령어 파서
│   ├── Debugger_Console.cpp/h # 콘솔 입출력
│   └── Debugger_Help.cpp      # 도움말 (58KB)
│
├── Configuration/             # 설정 관리
│   ├── Config.cpp/h
│   ├── PropertySheet*.cpp
│   └── Page*.cpp
│
├── Windows/                   # Windows UI (Windows 포트용)
│   ├── AppleWin.cpp           # 메인 애플리케이션 (34KB)
│   ├── WinFrame.cpp           # 프레임 관리 (108KB)
│   └── Win32Frame.cpp/h
│
├── Tfe/                       # 네트워크 지원 (Uthernet)
│   ├── NetworkBackend.cpp/h
│   ├── PCapBackend.cpp/h      # libpcap 지원
│   ├── DNS.cpp/h
│   └── IPRaw.cpp/h
│
├── Z80VICE/                   # Z80 CPU (CP/M 카드용)
│   ├── z80.cpp/h
│   └── z80mem.cpp/h
│
├── linux/                     # Linux 포트 특화
│   ├── linuxframe.cpp/h       # Linux 프레임 관리
│   ├── linuxsoundbuffer.cpp/h
│   ├── libwindows/            # Windows API 대체
│   ├── network/               # libslirp 지원
│   └── duplicates/            # Windows 파일 Linux 재구현
│
├── frontends/                 # UI 프론트엔드 (4가지)
│   ├── common2/               # 공용 코드
│   ├── sdl/                   # SDL2 + ImGui (sa2)
│   ├── ncurses/               # ncurses (applen)
│   ├── qt/                    # Qt5/Qt6 (qapple)
│   └── libretro/              # libretro 코어 (ra2)
│
└── debugserver/               # HTTP 디버그 서버
    ├── DebugServerManager.cpp/h
    ├── CPUInfoProvider.cpp/h
    ├── MemoryInfoProvider.cpp/h
    ├── HttpServer.cpp/h
    └── JsonBuilder.cpp/h
```

---

## 3. CPU 에뮬레이션 분석

### 3.1 CPU 레지스터 구조 (`CPU.h:5-14`)

```cpp
struct regsrec {
    BYTE a;       // Accumulator (누산기)
    BYTE x;       // Index X (X 인덱스 레지스터)
    BYTE y;       // Index Y (Y 인덱스 레지스터)
    BYTE ps;      // Processor Status (프로세서 상태)
    WORD pc;      // Program Counter (프로그램 카운터)
    WORD sp;      // Stack Pointer (스택 포인터)
    BYTE bJammed; // CPU crashed (NMOS 6502 only)
};
```

### 3.2 프로세서 상태 플래그 (`CPU.h:17-26`)

```cpp
enum {
    AF_SIGN      = 0x80,  // N: Negative (부호)
    AF_OVERFLOW  = 0x40,  // V: Overflow (오버플로우)
    AF_RESERVED  = 0x20,  // R: Reserved (항상 1)
    AF_BREAK     = 0x10,  // B: Break
    AF_DECIMAL   = 0x08,  // D: Decimal (BCD 모드)
    AF_INTERRUPT = 0x04,  // I: Interrupt disable
    AF_ZERO      = 0x02,  // Z: Zero
    AF_CARRY     = 0x01   // C: Carry
};
```

### 3.3 CPU 종류 정의

```cpp
enum eCpuType {
    CPU_UNKNOWN = 0,
    CPU_6502 = 1,     // NMOS 6502 (Apple ][, ][+)
    CPU_65C02,        // WDC 65C02 (Enhanced Apple //e)
    CPU_Z80           // Z80 (CP/M 카드)
};
```

### 3.4 CPU 실행 모드 (`CPU.cpp:608-644`)

```cpp
static uint32_t InternalCpuExecute(const uint32_t uTotalCycles, const bool bVideoUpdate)
{
    if (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_BENCHMARK)
    {
        // 일반 실행 모드
        if (GetMainCpu() == CPU_6502)
            return Cpu6502(uTotalCycles, bVideoUpdate);
        else
            return Cpu65C02(uTotalCycles, bVideoUpdate);
    }
    else  // MODE_STEPPING || MODE_DEBUG
    {
        // 디버거 모드 - Heatmap 추적 활성화
        if (GetMainCpu() == CPU_6502)
            return Cpu6502_debug(uTotalCycles, bVideoUpdate);
        else
            return Cpu65C02_debug(uTotalCycles, bVideoUpdate);
    }
}
```

### 3.5 CPU 함수 변형

| 함수명 | 설명 |
|--------|------|
| `Cpu6502()` | 6502 일반 실행 |
| `Cpu6502_debug()` | 6502 디버거 모드 (Heatmap 활성화) |
| `Cpu6502_altRW()` | 6502 대체 읽기/쓰기 지원 |
| `Cpu65C02()` | 65C02 일반 실행 |
| `Cpu65C02_debug()` | 65C02 디버거 모드 |
| `Cpu65C02_altRW()` | 65C02 대체 읽기/쓰기 지원 |

### 3.6 6502 vs 65C02 주요 차이점

#### 새로운 명령어 (65C02만)

| 명령어 | 설명 |
|--------|------|
| `TSB` | Test and Set Bits |
| `TRB` | Test and Reset Bits |
| `BRA` | Branch Always (무조건 분기) |
| `STZ` | Store Zero |
| `PHX/PHY` | Push X/Y |
| `PLX/PLY` | Pull X/Y |
| `INA/DEA` | Increment/Decrement A |

#### JMP 간접 주소 버그

- **6502 (NMOS)**: `JMP ($xxFF)` 버그 - 같은 페이지의 xx00에서 읽음
- **65C02 (CMOS)**: 버그 수정됨, 추가 사이클 소요

#### BCD 모드 차이

- **65C02**: BCD 연산 시 추가 사이클, D 플래그 BRK에서 클리어

---

## 4. 인터럽트 처리 (`CPU.cpp`)

### 4.1 IRQ 소스 정의

```cpp
enum eIRQSRC {
    IS_6522 = 0,        // Mockingboard 6522
    IS_SPEECH,          // Speech API
    IS_SSC,             // Serial card
    IS_MOUSE,           // Mouse card
    IS_BREAKPOINTCARD
};
```

### 4.2 IRQ 처리 (`CPU.cpp:448-507`)

```cpp
static __forceinline bool IRQ(ULONG& uExecutedCycles, ...) {
    if (g_bmIRQ && !(regs.ps & AF_INTERRUPT)) {
        // 스택에 상태 저장
        _PUSH(regs.pc >> 8)
        _PUSH(regs.pc & 0xFF)
        _PUSH(regs.ps & ~AF_BREAK)  // B=0으로 IRQ 구분

        regs.ps |= AF_INTERRUPT;
        if (GetMainCpu() == CPU_65C02)
            regs.ps &= ~AF_DECIMAL;  // 65C02는 D 플래그 클리어

        regs.pc = *(WORD*)(mem + _6502_INTERRUPT_VECTOR);
        CYC(7);
    }
}
```

### 4.3 인터럽트 벡터

| 주소 | 벡터 |
|------|------|
| `$FFFA-$FFFB` | NMI 벡터 |
| `$FFFC-$FFFD` | RESET 벡터 |
| `$FFFE-$FFFF` | IRQ/BRK 벡터 |

### 4.4 IRQ 제어 함수

```cpp
void CpuIrqAssert(eIRQSRC Device);   // IRQ 신호 설정
void CpuIrqDeassert(eIRQSRC Device); // IRQ 신호 해제
void CpuNmiAssert(eIRQSRC Device);   // NMI 신호 설정
void CpuNmiDeassert(eIRQSRC Device); // NMI 신호 해제
```

---

## 5. CPU 디버거 분석

### 5.1 디버거 버전

```cpp
const int DEBUGGER_VERSION = MAKE_VERSION(2,9,3,4);
```

### 5.2 브레이크포인트 시스템 (`Debugger_Types.h`)

#### 브레이크포인트 소스 타입 (20가지)

```cpp
enum BreakpointSource_t {
    // 레지스터
    BP_SRC_REG_A,              // Accumulator
    BP_SRC_REG_X,              // X 레지스터
    BP_SRC_REG_Y,              // Y 레지스터
    BP_SRC_REG_PC,             // Program Counter
    BP_SRC_REG_S,              // Stack Pointer
    BP_SRC_REG_P,              // Processor Status

    // 플래그
    BP_SRC_FLAG_C,             // Carry
    BP_SRC_FLAG_Z,             // Zero
    BP_SRC_FLAG_I,             // Interrupt
    BP_SRC_FLAG_D,             // Decimal
    BP_SRC_FLAG_B,             // Break
    BP_SRC_FLAG_R,             // Reserved
    BP_SRC_FLAG_V,             // Overflow
    BP_SRC_FLAG_N,             // Sign (Negative)

    // 메모리/IO
    BP_SRC_OPCODE,             // Opcode
    BP_SRC_MEM_RW,             // 메모리 읽기/쓰기
    BP_SRC_MEM_READ_ONLY,      // 메모리 읽기만
    BP_SRC_MEM_WRITE_ONLY,     // 메모리 쓰기만
    BP_SRC_VIDEO_SCANNER,      // 비디오 스캐너 위치
};
```

#### 브레이크포인트 연산자 (9가지)

```cpp
enum BreakpointOperator_t {
    BP_OP_LESS_EQUAL,      // <=
    BP_OP_LESS_THAN,       // <
    BP_OP_EQUAL,           // = (기본값)
    BP_OP_NOT_EQUAL,       // !=
    BP_OP_GREATER_THAN,    // >
    BP_OP_GREATER_EQUAL,   // >=
    BP_OP_READ,            // @ (읽기)
    BP_OP_WRITE,           // * (쓰기)
    BP_OP_READ_WRITE,      // ? (읽기/쓰기)
};
```

#### 브레이크포인트 구조체 (`Debugger_Types.h:229-261`)

```cpp
struct Breakpoint_t {
    WORD                 nAddress;   // 주소 또는 값
    UINT                 nLength;    // 범위 길이
    BreakpointSource_t   eSource;    // 소스 타입
    BreakpointOperator_t eOperator;  // 연산자
    bool                 bSet;       // 설정 여부
    bool                 bEnabled;   // 활성화 여부
    bool                 bTemp;      // 임시 브레이크포인트
    bool                 bHit;       // 히트 여부
    bool                 bStop;      // 정지 여부
    uint32_t             nHitCount;  // 히트 횟수
    AddressPrefix_t      addrPrefix; // 주소 접두사 (슬롯/뱅크)
};
```

### 5.3 CPU 디버거 명령어

| 명령어 | 설명 |
|--------|------|
| `G` | 일반 속도로 실행 (Go Normal Speed) |
| `GG` | 최대 속도로 실행 (Go Full Speed) |
| `P` | Step Over (서브루틴 건너뛰기) |
| `T` | Trace (단일 명령어 실행) |
| `TL` | Trace Line (사이클 카운팅 포함) |
| `RTS` | Step Out (서브루틴 종료까지 실행) |
| `R` | 레지스터 설정 |
| `U` | Unassemble (역어셈블) |
| `.` | PC 위치로 커서 이동 |
| `=` | 현재 명령어를 PC로 설정 |

### 5.4 브레이크포인트 명령어

| 명령어 | 설명 |
|--------|------|
| `BP` / `BPR` | 레지스터 브레이크포인트 추가 |
| `BPX` | PC 브레이크포인트 추가 |
| `BPM` | 메모리 접근 브레이크포인트 |
| `BPMR` | 메모리 읽기 브레이크포인트 |
| `BPMW` | 메모리 쓰기 브레이크포인트 |
| `BPIO` | I/O 주소 브레이크포인트 ($C0xx) |
| `BPV` | 비디오 스캐너 위치 브레이크포인트 |
| `BPC` | 브레이크포인트 삭제 |
| `BPD` | 브레이크포인트 비활성화 |
| `BPE` | 브레이크포인트 활성화 |
| `BPL` | 브레이크포인트 목록 |
| `BRK` | BRK/INVALID 명령어에서 중단 |
| `BRKOP` | 특정 opcode에서 중단 |
| `BRKINT` | 인터럽트 발생 시 중단 |

### 5.5 브레이크포인트 체크 함수

```cpp
int CheckBreakpointsIO();        // IO 메모리 브레이크포인트
int CheckBreakpointsReg();       // 레지스터 브레이크포인트
int CheckBreakpointsVideo();     // 비디오 스캐너 브레이크포인트
```

---

## 6. 메모리 시스템 분석

### 6.1 메모리 플래그 (`Memory.h:8-23`)

```cpp
#define MF_80STORE    0x00000001  // 80컬럼 스토어
#define MF_ALTZP      0x00000002  // 대체 제로페이지
#define MF_AUXREAD    0x00000004  // 보조 메모리 읽기 (RAMRD)
#define MF_AUXWRITE   0x00000008  // 보조 메모리 쓰기 (RAMWRT)
#define MF_BANK2      0x00000010  // Language Card Bank 2
#define MF_HIGHRAM    0x00000020  // Language Card RAM 활성화
#define MF_HIRES      0x00000040  // Hi-res 모드
#define MF_PAGE2      0x00000080  // 페이지 2
#define MF_SLOTC3ROM  0x00000100  // 슬롯 C3 ROM
#define MF_INTCXROM   0x00000200  // 내부 CX ROM
#define MF_WRITERAM   0x00000400  // Language Card RAM 쓰기 활성화
#define MF_IOUDIS     0x00000800  // IOU 비활성화 (//c only)
#define MF_ALTROM0    0x00001000  // 대체 ROM 페이지 0
#define MF_ALTROM1    0x00002000  // 대체 ROM 페이지 1
#define MF_LANGCARD_MASK (MF_WRITERAM|MF_HIGHRAM|MF_BANK2)
```

### 6.2 메모리 구조

```cpp
LPBYTE memshadow[256];      // 각 256바이트 페이지의 소스 포인터
LPBYTE memwrite[256];       // 각 페이지의 쓰기 대상 포인터
BYTE memreadPageType[256];  // 페이지 읽기 유형
LPBYTE mem;                 // 현재 메모리 캐시 (64KB)
LPBYTE memdirty;            // 더티 플래그 (256 바이트)
LPBYTE memaux;              // 보조 메모리 (64KB, //e only)
LPBYTE memmain;             // 주 메모리 (64KB)
LPBYTE memrom;              // ROM 메모리 ($D000-$FFFF)
LPBYTE pCxRomInternal;      // 내부 Cx ROM (4KB)
LPBYTE pCxRomPeripheral;    // 슬롯 확장 ROM (4KB)
```

### 6.3 메모리 맵

| 주소 범위 | 설명 |
|-----------|------|
| $0000-$00FF | Zero Page (빠른 접근) |
| $0100-$01FF | 스택 영역 |
| $0200-$03FF | 시스템 사용 |
| $0400-$07FF | TEXT 페이지 1 |
| $0800-$0BFF | TEXT 페이지 2 |
| $2000-$3FFF | Hi-Res 페이지 1 |
| $4000-$5FFF | Hi-Res 페이지 2 |
| $C000-$C0FF | I/O 영역 (소프트 스위치) |
| $C100-$C7FF | 슬롯 ROM 영역 |
| $C800-$CFFF | 펌웨어 확장 ROM |
| $D000-$FFFF | ROM/Language Card 영역 |

### 6.4 메모리 뷰 모드

```cpp
enum MemoryView_e {
    MEM_VIEW_HEX,    // 16진수 표시
    MEM_VIEW_ASCII,  // ASCII 문자 표시
    MEM_VIEW_APPLE,  // Apple 텍스트 (하이비트)
};
```

### 6.5 메모리 디버거 명령어

| 명령어 | 설명 |
|--------|------|
| `MD1` / `MD2` | 미니 메모리 덤프 (Hex) |
| `MA1` / `MA2` | 미니 메모리 덤프 (ASCII) |
| `MT1` / `MT2` | 미니 메모리 덤프 (Apple Text) |
| `ME` | 메모리 편집기 |
| `MEB` | 바이트 입력 |
| `MEW` | 워드 입력 |
| `M` | 메모리 이동 |
| `MC` | 메모리 비교 |
| `F` | 메모리 채우기 |
| `S` | 메모리 검색 |
| `SH` | 16진수 값 검색 |
| `BLOAD` | 메모리 영역 로드 |
| `BSAVE` | 메모리 영역 저장 |

### 6.6 Language Card 동작

```cpp
class LanguageCardUnit : public Card {
    LPBYTE m_pMemory;        // 16K 뱅크
    UINT m_memMode;          // 메모리 모드
    BOOL m_uLastRamWrite;    // RAM 쓰기 추적
};
```

**Language Card 주소:**
- $C080-$C083: Bank 1
- $C088-$C08B: Bank 2

### 6.7 RamWorks III 지원

```cpp
static UINT g_uMaxExBanks = 1;              // 최대 256개 뱅크
static UINT g_uActiveBank = 0;              // 현재 뱅크
static LPBYTE RWpages[kMaxExMemoryBanks];   // 256 * 64K = 16MB
```

### 6.8 Heatmap 시스템 (`cpu_heatmap.inl`)

```cpp
inline void Heatmap_R(uint16_t address) { /* 읽기 추적 */ }
inline void Heatmap_W(uint16_t address) { /* 쓰기 추적 */ }
inline void Heatmap_X(uint16_t address) { /* 실행 추적 */ }

inline uint8_t Heatmap_ReadByte(uint16_t addr, int uExecutedCycles) {
    Heatmap_R(addr);
    return _READ(addr);
}
```

---

## 7. 심볼 테이블 시스템

### 7.1 심볼 테이블 인덱스 (`Debugger_Types.h:1586-1598`)

```cpp
enum SymbolTable_Index_e {
    SYMBOLS_MAIN,        // APPLE2E.SYM - Apple II ROM
    SYMBOLS_APPLESOFT,   // A2_BASIC.SYM - Applesoft BASIC
    SYMBOLS_ASSEMBLY,    // A2_ASM.SYM - 어셈블리
    SYMBOLS_USER_1,      // A2_USER1.SYM - 사용자 정의 1
    SYMBOLS_USER_2,      // A2_USER2.SYM - 사용자 정의 2
    SYMBOLS_SRC_1,       // A2_SRC1.SYM - 소스 1
    SYMBOLS_SRC_2,       // A2_SRC2.SYM - 소스 2
    SYMBOLS_DOS33,       // A2_DOS33.SYM - DOS 3.3
    SYMBOLS_PRODOS,      // A2_PRODOS.SYM - ProDOS
    NUM_SYMBOL_TABLES = 9
};
```

### 7.2 심볼 명령어

| 명령어 | 설명 |
|--------|------|
| `SYM` | 심볼 조회/정의 |
| `SYMINFO` | 심볼 요약 정보 |
| `SYMLIST` | 심볼 목록 |
| `SYMMAIN` | 메인 심볼 테이블 |
| `SYMUSER` | 사용자 심볼 테이블 |
| `SYMDOS33` | DOS 3.3 심볼 테이블 |
| `SYMPRODOS` | ProDOS 심볼 테이블 |

---

## 8. 비디오/그래픽 에뮬레이션

### 8.1 비디오 모드

```cpp
enum VideoType_e {
    VT_MONO_CUSTOM,           // 흑백 커스텀
    VT_COLOR_IDEALIZED,       // 이상적인 RGB
    VT_COLOR_VIDEOCARD_RGB,   // RGB 카드/모니터
    VT_COLOR_MONITOR_NTSC,    // NTSC/PAL 컴포지트
    VT_COLOR_TV,              // 컬러 TV
    VT_MONO_TV,               // 흑백 TV
    VT_MONO_AMBER,            // 앰버 모니터
    VT_MONO_GREEN,            // 그린 모니터
    VT_MONO_WHITE,            // 화이트 모니터
};
```

### 8.2 NTSC 렌더링

- **스캔라인**: 65 x 262 사이클 (NTSC) / 65 x 312 사이클 (PAL)
- **프레임당 사이클**: 17,030 (NTSC)
- **해상도**: 280x192 (기본) / 560x384 (더블)

### 8.3 프레임버퍼

```cpp
GetFrameBufferBorderlessWidth()   // 280*2 또는 320*2
GetFrameBufferBorderlessHeight()  // 192*2 또는 200*2
VideoGetScannerAddress()          // 실시간 스캔 위치
```

---

## 9. 사운드/오디오 에뮬레이션

### 9.1 스피커 (`Speaker.cpp`)

- 44.1 kHz 샘플 레이트
- PWM 기반 8-bit DAC 에뮬레이션
- $C030 토글로 소리 출력

### 9.2 Mockingboard (`Mockingboard.cpp`)

```cpp
struct MB_SUBUNIT {
    SY6522 sy6522;              // Timer/IO 제어
    AY8913 ay8913[2];           // PSG 칩 (최대 2개)
    SSI263 ssi263;              // 음성 합성
};
```

### 9.3 AY8913 PSG 칩

- 3개 톤 채널 (A, B, C)
- 노이즈 생성기
- Envelope 제어 (ADSR)
- 16개 볼륨 레벨

### 9.4 SSI263 음성 합성

- 64개 음소(Phoneme) 지원
- Votrax SC01 호환

---

## 10. 디스크/스토리지 에뮬레이션

### 10.1 Disk II 인터페이스

```cpp
class Disk2InterfaceCard : public Card {
    FloppyDrive m_floppyDrive[2];     // 드라이브 1, 2
    BYTE m_shiftReg;                   // 데이터 시퀀서
    WORD m_magnetStates;               // 스테퍼 상태
};
```

### 10.2 지원 이미지 형식

| 형식 | 설명 |
|------|------|
| WOZ | Disk Jockey 형식 (최신) |
| .dsk | 표준 니블 이미지 |
| .do | DOS 순서 |
| .po | ProDOS 순서 |
| ZIP | 압축 지원 |

### 10.3 펌웨어 지원

- 13-Sector (5.25" 디스크)
- 16-Sector (표준)

---

## 11. 네트워크 에뮬레이션

### 11.1 Super Serial Card

```cpp
class CSuperSerialCard : public Card {
    BYTE m_uControlByte;    // 제어 레지스터
    BYTE m_uCommandByte;    // 명령 레지스터
    UINT m_uBaudRate;       // 보드레이트 (기본 9600)
};
```

### 11.2 Uthernet 네트워크 카드

```cpp
class Uthernet1 : public NetworkCard {
    BYTE tfe[TFE_COUNT_IO_REGISTER];              // 16개 I/O 레지스터
    BYTE tfe_packetpage[MAX_PACKETPAGE_ARRAY];    // 4KB 패킷 페이지
};
```

---

## 12. 카드 슬롯 시스템

### 12.1 카드 타입

```cpp
enum SS_CARDTYPE {
    CT_Empty = 0,
    CT_Disk2,               // Disk II (슬롯 6)
    CT_SSC,                 // Super Serial Card (슬롯 2)
    CT_MockingboardC,       // Mockingboard (슬롯 4)
    CT_GenericPrinter,      // 프린터 (슬롯 1)
    CT_GenericHDD,          // 하드디스크
    CT_MouseInterface,      // 마우스
    CT_Z80,                 // Z80 카드
    CT_Phasor,              // Phasor 음성
    CT_80Col,               // 80컬럼 (1K)
    CT_Extended80Col,       // 확장 80컬럼 (64K)
    CT_RamWorksIII,         // RamWorks III (8MB)
    CT_Uthernet,            // Uthernet 네트워크
    CT_LanguageCard,        // Language Card
    CT_Saturn128K,          // Saturn 128K
    CT_VidHD,               // VidHD 확장
    // ...
};
```

### 12.2 기본 슬롯 구성

| 슬롯 | 기본 카드 |
|------|----------|
| 0 | Language Card |
| 1 | 프린터 |
| 2 | Super Serial Card |
| 3 | 빈 슬롯 |
| 4 | 빈 슬롯 (Mockingboard 권장) |
| 5 | 빈 슬롯 |
| 6 | Disk II |
| 7 | 빈 슬롯 |
| AUX | Extended 80Col (64K) |

---

## 13. 프론트엔드 시스템

### 13.1 프론트엔드 계층 구조

```
LinuxFrame (linux/linuxframe.h)
    ↓
CommonFrame (common2/commonframe.h)
    ↓
GNUFrame (common2/gnuframe.h)
    ├─ SDLFrame (sdl/) → SDLImGuiFrame
    ├─ NFrame (ncurses/)
    ├─ RetroFrame (libretro/)
    └─ QtFrame (qt/)
```

### 13.2 프론트엔드 비교

| 특징 | SDL (sa2) | Qt (qapple) | ncurses (applen) | libretro |
|------|-----------|-------------|------------------|----------|
| GUI | ImGui | 네이티브 Qt | 텍스트 | RetroArch |
| 렌더링 | OpenGL ES2+ | Qt Widgets | ncurses | 프레임버퍼 |
| 사운드 | SDL Audio | Qt Multimedia | 불가 | 호스트 |
| 디버거 | ImGui 통합 | 별도 | 불가 | RetroArch |
| 권장 용도 | 일반 사용 | Qt 환경 | 서버/SSH | 레트로 게이밍 |

### 13.3 HTTP 디버그 서버 (SDL 프론트엔드)

| 포트 | 정보 |
|------|------|
| 65501 | 머신 정보 |
| 65502 | I/O 정보 |
| 65503 | CPU 정보 |
| 65504 | 메모리 정보 |

---

## 14. 주요 상수

| 상수 | 값 | 설명 |
|------|-----|------|
| `MAX_BREAKPOINTS` | 16 | 최대 브레이크포인트 수 |
| `MAX_BOOKMARKS` | 10 | 최대 북마크 수 |
| `MAX_WATCHES` | 6 | 최대 감시점 수 |
| `MAX_ZEROPAGE_POINTERS` | 8 | 최대 제로페이지 포인터 수 |
| `MAX_SYMBOLS_LEN` | 51 | 최대 심볼 이름 길이 |
| `NUM_SYMBOL_TABLES` | 9 | 심볼 테이블 개수 |
| `NUM_MEM_DUMPS` | 2 | 메모리 덤프 영역 수 |
| `CONSOLE_WIDTH` | 80 | 콘솔 폭 |
| `CONSOLE_HEIGHT` | 768 | 콘솔 높이 |

---

## 15. 빌드 시스템

### 15.1 CMake 옵션

```cmake
BUILD_APPLEN   # ncurses 프론트엔드
BUILD_QAPPLE   # Qt 프론트엔드
BUILD_SA2      # SDL2 프론트엔드 (권장)
BUILD_LIBRETRO # libretro 코어
```

### 15.2 주요 의존성

- **공통**: libyaml, minizip, zlib
- **SDL**: SDL2, SDL2_image, OpenGL
- **Qt**: Qt6/Qt5 (Widgets, Multimedia)
- **ncurses**: ncurses, libevdev
- **네트워크**: libslirp (권장), libpcap (폴백)

---

## 16. 파일 참조

| 파일 | 설명 |
|------|------|
| `source/CPU.cpp` | CPU 에뮬레이션 메인 |
| `source/CPU.h` | CPU 헤더 (레지스터 구조체) |
| `source/Memory.cpp` | 메모리 관리 (99KB) |
| `source/Memory.h` | 메모리 헤더 (플래그, I/O) |
| `source/NTSC.cpp` | NTSC 비디오 처리 (96KB) |
| `source/Debugger/Debug.cpp` | 디버거 메인 로직 (262KB) |
| `source/Debugger/Debugger_Types.h` | 디버거 타입 정의 (49KB) |
| `source/Debugger/Debugger_Commands.cpp` | 명령어 테이블 (43KB) |
| `source/Debugger/Debugger_Display.cpp` | 화면 출력 (116KB) |
| `source/CPU/cpu_heatmap.inl` | Heatmap 메모리 추적 |
| `source/Mockingboard.cpp` | Mockingboard 음성 카드 |
| `source/Disk.cpp` | Disk II 에뮬레이션 |

---

## 17. 코드 통계

| 항목 | 값 |
|------|-----|
| 총 소스 파일 (.cpp) | 280+ 개 |
| 총 헤더 파일 (.h) | 283+ 개 |
| source/ 총 크기 | 16MB |
| source/Debugger | 840KB |
| 가장 큰 파일 | Debug.cpp (262KB) |

---

*마지막 갱신: 2025-12-27*
