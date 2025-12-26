# AppleWin Linux 프로젝트 분석

## 1. 프로젝트 개요

AppleWin은 Windows용 Apple //e 에뮬레이터의 Linux/macOS 포트입니다.

- **원본 프로젝트**: https://github.com/AppleWin/AppleWin (Windows)
- **Linux 포트**: https://github.com/audetto/AppleWin
- **지원 CPU**: MOS 6502, WDC 65C02, Z80 (CP/M 카드)

---

## 2. 프로젝트 구조

```
AppleWin/
├── source/
│   ├── CPU.cpp/h              # CPU 에뮬레이션 (6502/65C02)
│   ├── Memory.cpp/h           # 메모리 관리
│   ├── CPU/
│   │   ├── cpu6502.h          # 6502 명령어 구현
│   │   ├── cpu65C02.h         # 65C02 명령어 구현
│   │   ├── cpu_general.inl    # 공통 CPU 매크로
│   │   ├── cpu_instructions.inl # 명령어 구현
│   │   └── cpu_heatmap.inl    # 디버거용 메모리 접근 추적
│   ├── Debugger/              # 디버거 모듈
│   │   ├── Debug.cpp/h        # 메인 디버거 로직
│   │   ├── Debugger_Commands.cpp  # 명령어 정의
│   │   ├── Debugger_Types.h   # 타입/구조체 정의
│   │   ├── Debugger_Display.cpp   # 화면 출력
│   │   ├── Debugger_Disassembler.cpp # 역어셈블러
│   │   ├── Debugger_Symbols.cpp   # 심볼 테이블
│   │   └── ...
│   ├── frontends/
│   │   ├── sdl/               # SDL2 프론트엔드 (sa2)
│   │   ├── ncurses/           # ncurses 프론트엔드 (applen)
│   │   ├── qt/                # Qt 프론트엔드 (qapple)
│   │   └── libretro/          # libretro 코어 (ra2)
│   └── ...
└── ...
```

---

## 3. CPU 에뮬레이션 분석

### 3.1 CPU 레지스터 구조 (`CPU.h:5-14`)

```cpp
struct regsrec {
    BYTE a;      // Accumulator
    BYTE x;      // Index X
    BYTE y;      // Index Y
    BYTE ps;     // Processor Status (flags)
    WORD pc;     // Program Counter
    WORD sp;     // Stack Pointer
    BYTE bJammed; // CPU crashed (NMOS 6502 only)
};
```

### 3.2 프로세서 상태 플래그 (`CPU.h:17-26`)

```cpp
enum {
    AF_SIGN      = 0x80,  // N: Negative
    AF_OVERFLOW  = 0x40,  // V: Overflow
    AF_RESERVED  = 0x20,  // R: Reserved
    AF_BREAK     = 0x10,  // B: Break
    AF_DECIMAL   = 0x08,  // D: Decimal
    AF_INTERRUPT = 0x04,  // I: Interrupt disable
    AF_ZERO      = 0x02,  // Z: Zero
    AF_CARRY     = 0x01   // C: Carry
};
```

### 3.3 CPU 실행 모드 (`CPU.cpp:608-644`)

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

### 3.4 CPU 함수 변형

| 함수명 | 설명 |
|--------|------|
| `Cpu6502()` | 6502 일반 실행 |
| `Cpu6502_debug()` | 6502 디버거 모드 (Heatmap 활성화) |
| `Cpu6502_altRW()` | 6502 대체 읽기/쓰기 지원 |
| `Cpu65C02()` | 65C02 일반 실행 |
| `Cpu65C02_debug()` | 65C02 디버거 모드 |
| `Cpu65C02_altRW()` | 65C02 대체 읽기/쓰기 지원 |

---

## 4. CPU 디버거 분석

### 4.1 디버거 버전

```cpp
const int DEBUGGER_VERSION = MAKE_VERSION(2,9,3,4);
```

### 4.2 브레이크포인트 시스템 (`Debugger_Types.h`)

#### 브레이크포인트 소스 타입

```cpp
enum BreakpointSource_t {
    BP_SRC_REG_A,      // 레지스터 A
    BP_SRC_REG_X,      // 레지스터 X
    BP_SRC_REG_Y,      // 레지스터 Y
    BP_SRC_REG_PC,     // Program Counter
    BP_SRC_REG_S,      // Stack Pointer
    BP_SRC_REG_P,      // Processor Status
    BP_SRC_FLAG_C,     // Carry 플래그
    BP_SRC_FLAG_Z,     // Zero 플래그
    BP_SRC_FLAG_I,     // Interrupt 플래그
    BP_SRC_FLAG_D,     // Decimal 플래그
    BP_SRC_FLAG_B,     // Break 플래그
    BP_SRC_FLAG_R,     // Reserved 플래그
    BP_SRC_FLAG_V,     // Overflow 플래그
    BP_SRC_FLAG_N,     // Sign 플래그
    BP_SRC_OPCODE,     // Opcode
    BP_SRC_MEM_RW,     // 메모리 읽기/쓰기
    BP_SRC_MEM_READ_ONLY,  // 메모리 읽기 전용
    BP_SRC_MEM_WRITE_ONLY, // 메모리 쓰기 전용
    BP_SRC_VIDEO_SCANNER,  // 비디오 스캐너 위치
};
```

#### 브레이크포인트 연산자

```cpp
enum BreakpointOperator_t {
    BP_OP_LESS_EQUAL,    // <=
    BP_OP_LESS_THAN,     // <
    BP_OP_EQUAL,         // =
    BP_OP_NOT_EQUAL,     // !=
    BP_OP_GREATER_THAN,  // >
    BP_OP_GREATER_EQUAL, // >=
    BP_OP_READ,          // @ (메모리 읽기)
    BP_OP_WRITE,         // * (메모리 쓰기)
    BP_OP_READ_WRITE,    // ? (메모리 읽기/쓰기)
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

### 4.3 CPU 디버거 명령어

| 명령어 | 설명 |
|--------|------|
| `G` | 일반 속도로 실행 |
| `GG` | 최대 속도로 실행 |
| `P` | Step Over (서브루틴 건너뛰기) |
| `T` | Trace (단일 명령어 실행) |
| `TL` | Trace Line (사이클 카운팅 포함) |
| `RTS` | Step Out (서브루틴 종료까지 실행) |
| `R` | 레지스터 설정 |
| `U` | Unassemble (역어셈블) |
| `.` | PC 위치로 커서 이동 |
| `=` | 현재 명령어를 PC로 설정 |

### 4.4 브레이크포인트 명령어

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

---

## 5. 메모리 디버거 분석

### 5.1 메모리 플래그 (`Memory.h:8-23`)

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
```

### 5.2 메모리 덤프 구조체 (`Debugger_Types.h:1243-1250`)

```cpp
struct MemoryDump_t {
    bool           bActive;      // 활성화 여부
    WORD           nAddress;     // 시작 주소
    DEVICE_e       eDevice;      // 장치 타입
    MemoryView_e   eView;        // 보기 모드 (HEX/ASCII/APPLE)
    AddressPrefix_t addrPrefix;  // 주소 접두사
};
```

### 5.3 메모리 뷰 모드

```cpp
enum MemoryView_e {
    MEM_VIEW_HEX,    // 16진수 표시
    MEM_VIEW_ASCII,  // ASCII 문자 표시
    MEM_VIEW_APPLE,  // Apple 텍스트 (하이비트)
};
```

### 5.4 메모리 디버거 명령어

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
| `@` | 검색 결과 표시 |
| `BLOAD` | 메모리 영역 로드 |
| `BSAVE` | 메모리 영역 저장 |

### 5.5 Heatmap 시스템 (`cpu_heatmap.inl`)

디버거 모드에서 메모리 접근을 추적하는 시스템:

```cpp
inline void Heatmap_R(uint16_t address) { /* 읽기 추적 */ }
inline void Heatmap_W(uint16_t address) { /* 쓰기 추적 */ }
inline void Heatmap_X(uint16_t address) { /* 실행 추적 */ }

inline uint8_t Heatmap_ReadByte(uint16_t addr, int uExecutedCycles) {
    Heatmap_R(addr);
    return _READ(addr);
}

inline void Heatmap_WriteByte(uint16_t addr, uint16_t value, int uExecutedCycles) {
    Heatmap_W(addr);
    _WRITE(value);
}
```

---

## 6. 브레이크포인트 체크 로직

### 6.1 레지스터 브레이크포인트 체크 (`Debug.cpp:1480-1521`)

```cpp
int CheckBreakpointsReg() {
    for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++) {
        Breakpoint_t* pBP = &g_aBreakpoints[iBreakpoint];

        switch (pBP->eSource) {
            case BP_SRC_REG_PC:
                bBreakpointHit = _CheckBreakpointValue(pBP, regs.pc);
                break;
            case BP_SRC_REG_A:
                bBreakpointHit = _CheckBreakpointValue(pBP, regs.a);
                break;
            case BP_SRC_REG_X:
                bBreakpointHit = _CheckBreakpointValue(pBP, regs.x);
                break;
            // ... 기타 레지스터
        }
    }
}
```

### 6.2 비디오 브레이크포인트 체크 (`Debug.cpp:1525-1549`)

```cpp
int CheckBreakpointsVideo() {
    for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++) {
        Breakpoint_t* pBP = &g_aBreakpoints[iBreakpoint];

        if (pBP->eSource != BP_SRC_VIDEO_SCANNER)
            continue;

        uint16_t vert = NTSC_GetVideoVertForDebugger();
        if (_CheckBreakpointValue(pBP, vert)) {
            iBreakpointHit = HitBreakpoint(pBP, BP_HIT_VIDEO_POS, iBreakpoint);
            pBP->bEnabled = false;  // 같은 스캔라인에서 중복 트리거 방지
        }
    }
}
```

---

## 7. 심볼 테이블 시스템

### 7.1 심볼 테이블 인덱스 (`Debugger_Types.h:1586-1598`)

```cpp
enum SymbolTable_Index_e {
    SYMBOLS_MAIN,        // 메인/ROM 심볼
    SYMBOLS_APPLESOFT,   // Applesoft BASIC 심볼
    SYMBOLS_ASSEMBLY,    // 어셈블리 심볼
    SYMBOLS_USER_1,      // 사용자 정의 1
    SYMBOLS_USER_2,      // 사용자 정의 2
    SYMBOLS_SRC_1,       // 소스 1
    SYMBOLS_SRC_2,       // 소스 2
    SYMBOLS_DOS33,       // DOS 3.3 심볼
    SYMBOLS_PRODOS,      // ProDOS 심볼
    NUM_SYMBOL_TABLES
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

## 8. 인터럽트 처리 (`CPU.cpp`)

### 8.1 IRQ 처리 (`CPU.cpp:448-507`)

```cpp
static __forceinline bool IRQ(ULONG& uExecutedCycles, ...) {
    if (g_bmIRQ && !(regs.ps & AF_INTERRUPT)) {
        // IRQ 서비스 루틴 진입
        _PUSH(regs.pc >> 8)
        _PUSH(regs.pc & 0xFF)
        _PUSH(regs.ps & ~AF_BREAK)
        regs.ps |= AF_INTERRUPT;
        if (GetMainCpu() == CPU_65C02)
            regs.ps &= ~AF_DECIMAL;  // 65C02는 Decimal 모드 클리어
        regs.pc = *(WORD*)(mem + _6502_INTERRUPT_VECTOR);
    }
}
```

### 8.2 IRQ 소스 관리

```cpp
void CpuIrqAssert(eIRQSRC Device);   // IRQ 신호 설정
void CpuIrqDeassert(eIRQSRC Device); // IRQ 신호 해제
void CpuNmiAssert(eIRQSRC Device);   // NMI 신호 설정
void CpuNmiDeassert(eIRQSRC Device); // NMI 신호 해제
```

---

## 9. 주요 상수

| 상수 | 값 | 설명 |
|------|-----|------|
| `MAX_BREAKPOINTS` | 16 | 최대 브레이크포인트 수 |
| `MAX_BOOKMARKS` | 10 | 최대 북마크 수 |
| `MAX_WATCHES` | 6 | 최대 감시점 수 |
| `MAX_ZEROPAGE_POINTERS` | 8 | 최대 제로페이지 포인터 수 |
| `MAX_SYMBOLS_LEN` | 51 | 최대 심볼 이름 길이 |
| `NUM_MEM_DUMPS` | 2 | 메모리 덤프 영역 수 |

---

## 10. 파일 참조

| 파일 | 설명 |
|------|------|
| `source/CPU.cpp` | CPU 에뮬레이션 메인 |
| `source/CPU.h` | CPU 헤더 (레지스터 구조체) |
| `source/Memory.cpp` | 메모리 관리 |
| `source/Memory.h` | 메모리 헤더 (플래그, I/O) |
| `source/Debugger/Debug.cpp` | 디버거 메인 로직 |
| `source/Debugger/Debugger_Types.h` | 디버거 타입 정의 |
| `source/Debugger/Debugger_Commands.cpp` | 명령어 테이블 |
| `source/CPU/cpu_heatmap.inl` | Heatmap 메모리 추적 |
