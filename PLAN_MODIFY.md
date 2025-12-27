# HTTP 디버그 서버 구현 문서

## 1. 개요

AppleWin Linux 에뮬레이터에 HTTP 기반 디버깅 정보 서버를 구현하였습니다. 외부 클라이언트(웹 브라우저, curl 등)에서 실시간으로 에뮬레이터 상태를 모니터링할 수 있습니다.

### 1.1 구현 상태: **완료**

- 4개의 독립적인 HTTP 서버 포트 운영
- 실시간 에뮬레이터 상태 정보 제공
- JSON API 및 HTML 대시보드 지원
- 외부 라이브러리 없이 순수 C++17 구현

### 1.2 포트 할당

| 포트 | 정보 유형 | 설명 |
|------|----------|------|
| 65501 | 머신 정보 | Apple II 타입, 모드, 메모리 상태 |
| 65502 | IO 정보 | 소프트 스위치, 슬롯, 어나운시에이터 |
| 65503 | CPU 정보 | 레지스터, 플래그, 브레이크포인트, 역어셈블 |
| 65504 | 메모리 정보 | 메모리 덤프, 제로 페이지, 스택 |

### 1.3 라이선스 및 의존성

- **라이선스**: GPL-2.0 (AppleWin 프로젝트와 동일)
- **외부 라이브러리**: 없음 (순수 내부 구현)
- **의존성**: C++17 표준 라이브러리 + POSIX/Winsock

---

## 2. 파일 구조

```
source/debugserver/
├── CMakeLists.txt              # 빌드 설정
│
├── # HTTP 서버 코어
├── HttpServer.h/cpp            # TCP 소켓 서버 (393줄)
├── HttpRequest.h/cpp           # HTTP 요청 파서 (278줄)
├── HttpResponse.h/cpp          # HTTP 응답 빌더 (237줄)
│
├── # 유틸리티
├── JsonBuilder.h/cpp           # JSON 생성기 (341줄)
├── SimpleTemplate.h/cpp        # HTML 템플릿 엔진 (516줄)
│
├── # 정보 제공자
├── InfoProvider.h/cpp          # 기본 인터페이스 (126줄)
├── MachineInfoProvider.h/cpp   # 머신 정보 (340줄)
├── CPUInfoProvider.h/cpp       # CPU 정보 (634줄)
├── IOInfoProvider.h/cpp        # IO 정보 (328줄)
├── MemoryInfoProvider.h/cpp    # 메모리 정보 (476줄)
│
├── # 매니저
├── DebugServerManager.h/cpp    # 메인 서버 관리자 (287줄)
│
├── # 문서
├── README.md                   # 사용 가이드
├── INTEGRATION.md              # 통합 가이드
│
└── # 테스트
    ├── test_server.cpp         # HTTP 서버 테스트
    └── test_template.cpp       # 템플릿 엔진 테스트
```

**총 코드량**: 약 3,956줄 (헤더 + 소스)

---

## 3. 핵심 구현 상세

### 3.1 HTTP 서버 (`HttpServer.h/cpp`)

#### 소켓 추상화

```cpp
// 플랫폼별 소켓 헤더
#ifdef _WIN32
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    typedef int socket_t;
    #define CLOSE_SOCKET(s) close(s)
#endif

class HttpServer {
public:
    using RequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

    explicit HttpServer(uint16_t port);
    ~HttpServer();

    void SetHandler(RequestHandler handler);
    bool Start();
    void Stop();
    bool IsRunning() const;
    uint16_t GetPort() const;

private:
    void AcceptLoop();
    void HandleClient(socket_t clientSocket);
    bool InitSocket();
    void CleanupSocket();

    uint16_t m_port;
    socket_t m_serverSocket;
    RequestHandler m_handler;
    std::thread m_acceptThread;
    std::atomic<bool> m_running;
};
```

### 3.2 HTTP 요청 파서 (`HttpRequest.h/cpp`)

```cpp
class HttpRequest {
public:
    bool Parse(const char* data, size_t length);
    bool Parse(const std::string& data);

    const std::string& GetMethod() const;
    const std::string& GetPath() const;
    const std::string& GetQuery() const;
    std::string GetQueryParam(const std::string& key,
                               const std::string& defaultValue = "") const;
    std::string GetHeader(const std::string& key,
                          const std::string& defaultValue = "") const;

    static std::string UrlDecode(const std::string& str);

private:
    std::string m_method;
    std::string m_path;
    std::string m_query;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_queryParams;
};
```

### 3.3 HTTP 응답 빌더 (`HttpResponse.h/cpp`)

```cpp
class HttpResponse {
public:
    void SetStatus(int code, const std::string& reason = "");
    void SetHeader(const std::string& key, const std::string& value);
    void SetContentType(const std::string& contentType);
    void SetBody(const std::string& body);

    void SendHTML(const std::string& html);
    void SendJSON(const std::string& json);
    void SendText(const std::string& text);
    void SendError(int code, const std::string& message);

    std::string Build() const;
};
```

### 3.4 JSON 생성기 (`JsonBuilder.h/cpp`)

```cpp
class JsonBuilder {
public:
    JsonBuilder& BeginObject();
    JsonBuilder& EndObject();
    JsonBuilder& BeginArray();
    JsonBuilder& EndArray();
    JsonBuilder& Key(const std::string& key);
    JsonBuilder& Value(const std::string& val);
    JsonBuilder& Value(int val);
    JsonBuilder& Value(uint64_t val);
    JsonBuilder& Value(bool val);
    JsonBuilder& Null();

    // 편의 메서드
    JsonBuilder& Add(const std::string& key, const std::string& val);
    JsonBuilder& Add(const std::string& key, int val);
    JsonBuilder& Add(const std::string& key, bool val);

    std::string ToString() const;
};
```

### 3.5 템플릿 엔진 (`SimpleTemplate.h/cpp`)

**템플릿 문법:**

| 문법 | 설명 | 예시 |
|------|------|------|
| `{{변수}}` | 변수 치환 | `{{cpu_type}}` |
| `{{#배열}}...{{/배열}}` | 반복 블록 | `{{#slots}}...{{/slots}}` |
| `{{?조건}}...{{/조건}}` | 조건부 블록 | `{{?has_disk}}...{{/has_disk}}` |

```cpp
class SimpleTemplate {
public:
    bool LoadFromString(const std::string& templateStr);
    bool LoadFromFile(const std::string& filePath);

    void SetVariable(const std::string& name, const std::string& value);
    void SetArray(const std::string& name, const std::vector<VariableMap>& items);
    void SetCondition(const std::string& name, bool value);

    std::string Render();
};
```

---

## 4. 정보 제공자 API

### 4.1 MachineInfoProvider (포트 65501)

**엔드포인트:**

| 경로 | 설명 |
|------|------|
| `GET /` | HTML 대시보드 |
| `GET /api/status` | 서버 상태 |
| `GET /api/info` | 머신 정보 |

**JSON 응답 예시 (`/api/info`):**

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

### 4.2 IOInfoProvider (포트 65502)

**엔드포인트:**

| 경로 | 설명 |
|------|------|
| `GET /` | HTML 대시보드 |
| `GET /api/softswitches` | 소프트 스위치 상태 |
| `GET /api/slots` | 확장 슬롯 정보 |
| `GET /api/annunciators` | 어나운시에이터 상태 |

### 4.3 CPUInfoProvider (포트 65503)

**엔드포인트:**

| 경로 | 설명 |
|------|------|
| `GET /` | HTML 대시보드 |
| `GET /api/registers` | CPU 레지스터 |
| `GET /api/flags` | CPU 플래그 |
| `GET /api/breakpoints` | 브레이크포인트 목록 |
| `GET /api/disasm?addr=$XXXX&lines=N` | 역어셈블 |
| `GET /api/stack` | 스택 내용 |

**JSON 응답 예시 (`/api/registers`):**

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

### 4.4 MemoryInfoProvider (포트 65504)

**엔드포인트:**

| 경로 | 설명 |
|------|------|
| `GET /` | HTML 대시보드 |
| `GET /api/dump?addr=$XXXX&lines=N&width=16` | 메모리 덤프 |
| `GET /api/read?addr=$XXXX&len=N` | 바이트 읽기 |
| `GET /api/zeropage` | 제로 페이지 덤프 |
| `GET /api/stack` | 스택 페이지 덤프 |
| `GET /api/textscreen` | 텍스트 화면 내용 |

---

## 5. 통합 방법

### 5.1 헤더 포함

```cpp
#include "debugserver/DebugServerManager.h"
```

### 5.2 초기화 시 서버 시작

```cpp
void InitialiseEmulator(const AppMode_e mode)
{
    // ... 기존 초기화 코드 ...

    MemInitialize();
    DebugInitialize();

    // 디버그 서버 시작
    if (DebugServer_Start()) {
        LogFileOutput("Debug HTTP Server started on ports 65501-65504\n");
    }
}
```

### 5.3 종료 시 서버 중지

```cpp
void DestroyEmulator()
{
    // 디버그 서버 먼저 중지
    DebugServer_Stop();

    // ... 기존 정리 코드 ...
    MemDestroy();
}
```

### 5.4 CMake 설정

```cmake
# source/CMakeLists.txt에 추가
add_subdirectory(debugserver)
target_link_libraries(appleii debugserver)
```

---

## 6. 사용 예시

### 6.1 브라우저 접속

```
http://localhost:65501/  - 머신 정보 대시보드
http://localhost:65502/  - IO 정보 대시보드
http://localhost:65503/  - CPU 정보 대시보드
http://localhost:65504/  - 메모리 정보 대시보드
```

### 6.2 curl 테스트

```bash
# 머신 정보 (JSON)
curl http://localhost:65501/api/info

# CPU 레지스터 (JSON)
curl http://localhost:65503/api/registers

# 메모리 덤프 ($C600부터 8줄)
curl "http://localhost:65504/api/dump?addr=\$C600&lines=8"

# 브레이크포인트 목록
curl http://localhost:65503/api/breakpoints

# 역어셈블 ($C600부터 10줄)
curl "http://localhost:65503/api/disasm?addr=\$C600&lines=10"

# 제로 페이지
curl http://localhost:65504/api/zeropage
```

---

## 7. 보안 고려사항

1. **기본 바인드 주소**: `127.0.0.1` (로컬만)
2. **외부 접속**: 명시적 설정 필요
   ```cpp
   debugserver::DebugServerManager::GetInstance().SetBindAddress("0.0.0.0");
   ```
3. **읽기 전용**: 모든 API는 읽기 전용
4. **인증 없음**: 공용 네트워크에 노출 금지
5. **리버스 프록시**: 원격 접속 시 인증이 있는 리버스 프록시 사용 권장

---

## 8. 내장 HTML 대시보드 스타일

모든 대시보드는 동일한 스타일 테마 사용:

```css
body {
    font-family: monospace;
    background: #1e1e2e;
    color: #cdd6f4;
}
h1 { color: #89b4fa; }
h2 { color: #a6e3a1; border-bottom: 1px solid #45475a; }
table { border-collapse: collapse; width: 100%; }
th, td { padding: 8px; border: 1px solid #45475a; }
th { background: #313244; color: #cba6f7; }
.value { color: #f9e2af; }
.status-on { color: #a6e3a1; }
.status-off { color: #f38ba8; }
```

---

## 9. 기존 코드 활용

디버거 관련 기존 코드에서 재사용한 부분:

| 기존 파일 | 활용 내용 |
|----------|----------|
| `Debugger/Debugger_Disassembler.cpp` | 역어셈블 로직 |
| `Debugger/Debug.cpp` | 메모리 덤프 포맷팅 |
| `Debugger/Debugger_Types.h` | 브레이크포인트 구조체 |
| `CPU.h` | 레지스터 구조체, 플래그 정의 |
| `Memory.h` | 메모리 플래그, 접근 함수 |
| `CardManager.h` | 슬롯 정보 조회 |

---

## 10. 파일별 코드량

| 파일 | 헤더(줄) | 소스(줄) | 합계 |
|------|---------|---------|------|
| HttpServer | 124 | 269 | 393 |
| HttpRequest | 74 | 204 | 278 |
| HttpResponse | 63 | 174 | 237 |
| JsonBuilder | 240 | 101 | 341 |
| SimpleTemplate | 120 | 396 | 516 |
| InfoProvider | 78 | 48 | 126 |
| MachineInfoProvider | 51 | 289 | 340 |
| CPUInfoProvider | 63 | 571 | 634 |
| IOInfoProvider | 53 | 275 | 328 |
| MemoryInfoProvider | 63 | 413 | 476 |
| DebugServerManager | 119 | 168 | 287 |
| **총계** | **1,048** | **2,908** | **3,956** |

---

## 11. 향후 확장 가능 기능

### 11.1 구현 가능한 추가 기능

- [ ] WebSocket 실시간 스트리밍
- [ ] 메모리 쓰기 API (PUT/POST)
- [ ] 브레이크포인트 설정/해제 API
- [ ] CPU 단일 스텝 실행 API
- [ ] 디스크 이미지 정보 API
- [ ] 사운드 카드 상태 API

### 11.2 확장 예시 코드

```cpp
// 메모리 쓰기 API 추가 예시
void MemoryInfoProvider::HandleWriteRequest(const HttpRequest& req, HttpResponse& res)
{
    if (req.GetMethod() != "POST") {
        res.SendError(405, "Method Not Allowed");
        return;
    }

    uint16_t addr = ParseAddress(req.GetQueryParam("addr"));
    uint8_t value = ParseByte(req.GetBody());

    WriteByteToMemory(addr, value);

    JsonBuilder json;
    json.BeginObject()
        .Add("success", true)
        .Add("address", ToHex16(addr))
        .Add("value", ToHex8(value))
        .EndObject();

    res.SendJSON(json.ToString());
}
```

---

## 12. 빌드 및 테스트

### 12.1 독립 테스트 빌드

```bash
cd source/debugserver

# HTTP 서버 테스트
g++ -std=c++17 -o test_server test_server.cpp HttpServer.cpp HttpRequest.cpp HttpResponse.cpp -pthread

# 템플릿 엔진 테스트
g++ -std=c++17 -o test_template test_template.cpp SimpleTemplate.cpp
```

### 12.2 AppleWin 통합 빌드

```bash
mkdir build && cd build
cmake .. -DBUILD_SA2=ON
make -j$(nproc)
```

---

*문서 작성: 2025-12-25*
*마지막 갱신: 2025-12-27*
