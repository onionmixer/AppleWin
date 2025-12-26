# HTTP 디버그 서버 개발 계획서

## 1. 개요

AppleWin Linux 에뮬레이터에 HTTP 기반 디버깅 정보 서버를 추가하여, 외부 클라이언트에서 실시간으로 에뮬레이터 상태를 모니터링할 수 있도록 합니다.

### 1.1 목표

- 4개의 독립적인 HTTP 서버 포트 운영
- 설정 파일을 통한 활성화/비활성화 제어
- 커스터마이즈 가능한 출력 양식 지원
- 실시간 에뮬레이터 상태 정보 제공

### 1.2 포트 할당

| 포트 | 정보 유형 | 설명 |
|------|----------|------|
| 65501 | 머신 정보 | 드라이브, 슬롯, 카드 정보 |
| 65502 | IO 정보 | I/O 주소 읽기/쓰기 데이터 |
| 65503 | CPU 정보 | 레지스터, 플래그, 사이클 |
| 65504 | 메모리 정보 | 메모리 덤프, 메모리 모드 |

### 1.3 라이선스 및 의존성 정책

**핵심 원칙: 외부 라이브러리 없이 순수 내부 구현**

- **라이선스**: GPL-2.0 (AppleWin 프로젝트와 동일)
- **외부 라이브러리**: 사용하지 않음
- **의존성**: C++17 표준 라이브러리 + POSIX/Winsock만 사용
- **모든 기능 내부 구현**:
  - HTTP 프로토콜 파서
  - JSON 생성기
  - 템플릿 엔진
  - 소켓 통신

---

## 2. 프로젝트 구조 분석

### 2.1 정보 접근 가능한 기존 API

#### 머신 정보 (`source/CardManager.h`, `source/Disk.h`)

```cpp
// 카드 매니저 접근
CardManager& GetCardMgr(void);

// 슬롯별 카드 타입 조회
SS_CARDTYPE QuerySlot(UINT slot);  // CT_Empty, CT_Disk2, CT_SSC 등

// 디스크 정보
Disk2CardManager& GetDisk2CardMgr(void);
const std::string& GetFullDiskFilename(const int drive);
const std::string& GetBaseName(const int drive);
bool IsDriveEmpty(const int drive);
bool GetProtect(const int drive);  // Write protect 상태

// 머신 타입
eApple2Type GetApple2Type(void);  // A2TYPE_APPLE2E, A2TYPE_APPLE2EENHANCED 등
```

#### IO 정보 (`source/Memory.h`)

```cpp
// I/O 함수 배열
extern iofunction IORead[256];   // $C0xx 읽기
extern iofunction IOWrite[256];  // $C0xx 쓰기

// 메모리 모드 플래그
uint32_t GetMemMode(void);
// MF_80STORE, MF_ALTZP, MF_AUXREAD, MF_AUXWRITE, MF_BANK2, MF_HIGHRAM 등
```

#### CPU 정보 (`source/CPU.h`)

```cpp
// 레지스터 구조체
struct regsrec {
    BYTE a;      // Accumulator
    BYTE x;      // Index X
    BYTE y;      // Index Y
    BYTE ps;     // Processor Status
    WORD pc;     // Program Counter
    WORD sp;     // Stack Pointer
    BYTE bJammed;
};
extern regsrec regs;

// 사이클 정보
extern unsigned __int64 g_nCumulativeCycles;

// CPU 타입
eCpuType GetMainCpu(void);  // CPU_6502, CPU_65C02, CPU_Z80
eCpuType GetActiveCpu(void);

// 플래그
// AF_SIGN, AF_OVERFLOW, AF_RESERVED, AF_BREAK, AF_DECIMAL, AF_INTERRUPT, AF_ZERO, AF_CARRY
```

#### 메모리 정보 (`source/Memory.h`)

```cpp
extern LPBYTE mem;           // 64KB 메모리 포인터
extern LPBYTE memshadow[0x100];
extern LPBYTE memwrite[0x100];
extern LPBYTE memdirty;

// 메모리 읽기/쓰기 함수
uint8_t ReadByteFromMemory(uint16_t addr);
uint16_t ReadWordFromMemory(uint16_t addr);
void WriteByteToMemory(uint16_t addr, uint8_t data);

// 뱅크 메모리
LPBYTE MemGetBankPtr(const UINT nBank, const bool isSaveSnapshotOrDebugging = true);
LPBYTE MemGetAuxPtr(const WORD offset);
LPBYTE MemGetMainPtr(const WORD offset);
```

---

## 3. 신규 파일 구조

```
AppleWin/source/
├── debugserver/                    # 새 디렉토리
│   ├── CMakeLists.txt             # 빌드 설정
│   │
│   ├── # 핵심 서버 (내부 구현)
│   ├── DebugHttpServer.h          # 메인 서버 관리자
│   ├── DebugHttpServer.cpp
│   │
│   ├── # HTTP/소켓 (내부 구현 - 외부 라이브러리 없음)
│   ├── HttpServer.h               # HTTP 서버 (POSIX/Winsock 직접 사용)
│   ├── HttpServer.cpp
│   ├── HttpRequest.h              # HTTP 요청 파서 (내부 구현)
│   ├── HttpRequest.cpp
│   ├── HttpResponse.h             # HTTP 응답 빌더 (내부 구현)
│   ├── HttpResponse.cpp
│   │
│   ├── # JSON 생성기 (내부 구현 - 외부 라이브러리 없음)
│   ├── JsonBuilder.h              # 간단한 JSON 생성기
│   ├── JsonBuilder.cpp
│   │
│   ├── # 템플릿 엔진 (내부 구현 - 외부 라이브러리 없음)
│   ├── SimpleTemplate.h           # 간단한 템플릿 엔진
│   ├── SimpleTemplate.cpp
│   │
│   ├── # 정보 제공자
│   ├── InfoProvider.h             # 기본 인터페이스
│   ├── MachineInfoProvider.h      # 머신 정보
│   ├── MachineInfoProvider.cpp
│   ├── IOInfoProvider.h           # IO 정보
│   ├── IOInfoProvider.cpp
│   ├── CPUInfoProvider.h          # CPU 정보
│   ├── CPUInfoProvider.cpp
│   ├── MemoryInfoProvider.h       # 메모리 정보
│   ├── MemoryInfoProvider.cpp
│   │
│   └── templates/                 # 기본 템플릿 (선택적)
│       ├── machine.html
│       ├── io.html
│       ├── cpu.html
│       └── memory.html
```

---

## 4. 내부 구현 상세 설계

### 4.1 HTTP 서버 (외부 라이브러리 없음)

#### 4.1.1 소켓 추상화 계층

```cpp
// source/debugserver/HttpServer.h
// 라이선스: GPL-2.0 (AppleWin과 동일)
#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// 플랫폼별 소켓 헤더 (표준 시스템 헤더만 사용)
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE (-1)
    #define SOCKET_ERROR_VALUE (-1)
    #define CLOSE_SOCKET(s) close(s)
#endif

namespace debugserver {

class HttpRequest;
class HttpResponse;

class HttpServer {
public:
    using RequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

    explicit HttpServer(uint16_t port);
    ~HttpServer();

    // 복사/이동 금지
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void SetHandler(RequestHandler handler);
    bool Start();
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    uint16_t GetPort() const { return m_port; }

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
    std::mutex m_mutex;

#ifdef _WIN32
    static bool s_wsaInitialized;
    static std::mutex s_wsaMutex;
#endif
};

} // namespace debugserver
```

#### 4.1.2 HTTP 요청 파서 (내부 구현)

```cpp
// source/debugserver/HttpRequest.h
// 라이선스: GPL-2.0
#pragma once

#include <string>
#include <map>
#include <vector>

namespace debugserver {

class HttpRequest {
public:
    // 원시 데이터에서 파싱
    bool Parse(const char* data, size_t length);
    bool Parse(const std::string& data);

    // Getter
    const std::string& GetMethod() const { return m_method; }
    const std::string& GetPath() const { return m_path; }
    const std::string& GetQuery() const { return m_query; }
    const std::string& GetVersion() const { return m_version; }
    const std::string& GetBody() const { return m_body; }

    // 쿼리 파라미터
    std::string GetQueryParam(const std::string& key,
                               const std::string& defaultValue = "") const;
    bool HasQueryParam(const std::string& key) const;

    // 헤더
    std::string GetHeader(const std::string& key,
                          const std::string& defaultValue = "") const;
    bool HasHeader(const std::string& key) const;

    // URL 디코딩 (내부 구현)
    static std::string UrlDecode(const std::string& str);

private:
    bool ParseRequestLine(const std::string& line);
    bool ParseHeaders(const std::vector<std::string>& lines, size_t startIdx);
    void ParseQueryString(const std::string& queryString);

    std::string m_method;
    std::string m_path;
    std::string m_query;
    std::string m_version;
    std::string m_body;
    std::map<std::string, std::string> m_headers;
    std::map<std::string, std::string> m_queryParams;
};

} // namespace debugserver
```

#### 4.1.3 HTTP 응답 빌더 (내부 구현)

```cpp
// source/debugserver/HttpResponse.h
// 라이선스: GPL-2.0
#pragma once

#include <string>
#include <map>
#include <sstream>

namespace debugserver {

class HttpResponse {
public:
    HttpResponse();

    // 상태 코드
    void SetStatus(int code, const std::string& reason = "");

    // 헤더
    void SetHeader(const std::string& key, const std::string& value);
    void SetContentType(const std::string& contentType);

    // 본문
    void SetBody(const std::string& body);
    void AppendBody(const std::string& data);

    // 편의 메서드
    void SendHTML(const std::string& html);
    void SendJSON(const std::string& json);
    void SendText(const std::string& text);
    void SendError(int code, const std::string& message);

    // 직렬화
    std::string Build() const;

private:
    std::string GetStatusText(int code) const;

    int m_statusCode;
    std::string m_statusReason;
    std::map<std::string, std::string> m_headers;
    std::string m_body;
};

} // namespace debugserver
```

### 4.2 JSON 생성기 (내부 구현 - 외부 라이브러리 없음)

```cpp
// source/debugserver/JsonBuilder.h
// 라이선스: GPL-2.0
// 설명: 간단한 JSON 생성기. 파싱은 불필요하므로 생성만 지원.
#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <iomanip>

namespace debugserver {

class JsonBuilder {
public:
    JsonBuilder();

    // 객체 시작/종료
    JsonBuilder& BeginObject();
    JsonBuilder& EndObject();

    // 배열 시작/종료
    JsonBuilder& BeginArray();
    JsonBuilder& EndArray();

    // 키-값 쌍 (객체 내에서)
    JsonBuilder& Key(const std::string& key);

    // 값 추가
    JsonBuilder& Value(const std::string& val);
    JsonBuilder& Value(const char* val);
    JsonBuilder& Value(int val);
    JsonBuilder& Value(unsigned int val);
    JsonBuilder& Value(int64_t val);
    JsonBuilder& Value(uint64_t val);
    JsonBuilder& Value(double val);
    JsonBuilder& Value(bool val);
    JsonBuilder& Null();

    // 편의 메서드: 키-값 한번에
    JsonBuilder& Add(const std::string& key, const std::string& val);
    JsonBuilder& Add(const std::string& key, int val);
    JsonBuilder& Add(const std::string& key, uint64_t val);
    JsonBuilder& Add(const std::string& key, bool val);

    // 결과 문자열
    std::string ToString() const;

    // 리셋
    void Clear();

private:
    void AddCommaIfNeeded();
    std::string EscapeString(const std::string& str) const;

    std::ostringstream m_stream;
    std::vector<char> m_stack;  // '{' 또는 '['
    std::vector<bool> m_needComma;
};

// 구현 예시
inline JsonBuilder::JsonBuilder() {
    Clear();
}

inline void JsonBuilder::Clear() {
    m_stream.str("");
    m_stream.clear();
    m_stack.clear();
    m_needComma.clear();
}

inline JsonBuilder& JsonBuilder::BeginObject() {
    AddCommaIfNeeded();
    m_stream << '{';
    m_stack.push_back('{');
    m_needComma.push_back(false);
    return *this;
}

inline JsonBuilder& JsonBuilder::EndObject() {
    m_stream << '}';
    if (!m_stack.empty()) m_stack.pop_back();
    if (!m_needComma.empty()) m_needComma.pop_back();
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::BeginArray() {
    AddCommaIfNeeded();
    m_stream << '[';
    m_stack.push_back('[');
    m_needComma.push_back(false);
    return *this;
}

inline JsonBuilder& JsonBuilder::EndArray() {
    m_stream << ']';
    if (!m_stack.empty()) m_stack.pop_back();
    if (!m_needComma.empty()) m_needComma.pop_back();
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Key(const std::string& key) {
    AddCommaIfNeeded();
    m_stream << '"' << EscapeString(key) << "\":";
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(const std::string& val) {
    AddCommaIfNeeded();
    m_stream << '"' << EscapeString(val) << '"';
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(int val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(uint64_t val) {
    AddCommaIfNeeded();
    m_stream << val;
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Value(bool val) {
    AddCommaIfNeeded();
    m_stream << (val ? "true" : "false");
    if (!m_needComma.empty()) m_needComma.back() = true;
    return *this;
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, const std::string& val) {
    return Key(key).Value(val);
}

inline JsonBuilder& JsonBuilder::Add(const std::string& key, int val) {
    return Key(key).Value(val);
}

inline void JsonBuilder::AddCommaIfNeeded() {
    if (!m_needComma.empty() && m_needComma.back()) {
        m_stream << ',';
        m_needComma.back() = false;
    }
}

inline std::string JsonBuilder::EscapeString(const std::string& str) const {
    std::string result;
    result.reserve(str.size() + 10);
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

inline std::string JsonBuilder::ToString() const {
    return m_stream.str();
}

} // namespace debugserver
```

### 4.3 템플릿 엔진 (내부 구현 - 외부 라이브러리 없음)

```cpp
// source/debugserver/SimpleTemplate.h
// 라이선스: GPL-2.0
// 설명: 간단한 템플릿 엔진. {{변수}}, {{#배열}}...{{/배열}}, {{?조건}}...{{/조건}} 지원
#pragma once

#include <string>
#include <map>
#include <vector>
#include <functional>

namespace debugserver {

class SimpleTemplate {
public:
    using VariableMap = std::map<std::string, std::string>;
    using ArrayCallback = std::function<std::vector<VariableMap>()>;

    SimpleTemplate();

    // 템플릿 로드
    bool LoadFromString(const std::string& templateStr);
    bool LoadFromFile(const std::string& filePath);

    // 변수 설정
    void SetVariable(const std::string& name, const std::string& value);
    void SetVariables(const VariableMap& vars);

    // 배열 설정 (반복 블록용)
    void SetArray(const std::string& name, const std::vector<VariableMap>& items);

    // 조건 설정
    void SetCondition(const std::string& name, bool value);

    // 렌더링
    std::string Render();

    // 초기화
    void Clear();

private:
    std::string ProcessVariables(const std::string& text, const VariableMap& vars);
    std::string ProcessArrays(const std::string& text);
    std::string ProcessConditions(const std::string& text);

    std::string ReplaceVariable(const std::string& text,
                                 const std::string& name,
                                 const std::string& value);

    std::string m_template;
    VariableMap m_variables;
    std::map<std::string, std::vector<VariableMap>> m_arrays;
    std::map<std::string, bool> m_conditions;
};

// 구현
inline SimpleTemplate::SimpleTemplate() {}

inline bool SimpleTemplate::LoadFromString(const std::string& templateStr) {
    m_template = templateStr;
    return true;
}

inline void SimpleTemplate::SetVariable(const std::string& name, const std::string& value) {
    m_variables[name] = value;
}

inline void SimpleTemplate::SetVariables(const VariableMap& vars) {
    for (const auto& kv : vars) {
        m_variables[kv.first] = kv.second;
    }
}

inline void SimpleTemplate::SetArray(const std::string& name,
                                       const std::vector<VariableMap>& items) {
    m_arrays[name] = items;
}

inline void SimpleTemplate::SetCondition(const std::string& name, bool value) {
    m_conditions[name] = value;
}

inline std::string SimpleTemplate::Render() {
    std::string result = m_template;
    result = ProcessConditions(result);
    result = ProcessArrays(result);
    result = ProcessVariables(result, m_variables);
    return result;
}

inline void SimpleTemplate::Clear() {
    m_template.clear();
    m_variables.clear();
    m_arrays.clear();
    m_conditions.clear();
}

inline std::string SimpleTemplate::ProcessVariables(const std::string& text,
                                                      const VariableMap& vars) {
    std::string result = text;
    for (const auto& kv : vars) {
        std::string placeholder = "{{" + kv.first + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), kv.second);
            pos += kv.second.length();
        }
    }
    return result;
}

inline std::string SimpleTemplate::ProcessArrays(const std::string& text) {
    std::string result = text;

    for (const auto& arr : m_arrays) {
        std::string startTag = "{{#" + arr.first + "}}";
        std::string endTag = "{{/" + arr.first + "}}";

        size_t startPos = result.find(startTag);
        while (startPos != std::string::npos) {
            size_t endPos = result.find(endTag, startPos);
            if (endPos == std::string::npos) break;

            std::string blockContent = result.substr(
                startPos + startTag.length(),
                endPos - startPos - startTag.length()
            );

            std::string expanded;
            for (const auto& item : arr.second) {
                expanded += ProcessVariables(blockContent, item);
            }

            result.replace(startPos, endPos + endTag.length() - startPos, expanded);
            startPos = result.find(startTag);
        }
    }

    return result;
}

inline std::string SimpleTemplate::ProcessConditions(const std::string& text) {
    std::string result = text;

    for (const auto& cond : m_conditions) {
        std::string startTag = "{{?" + cond.first + "}}";
        std::string endTag = "{{/" + cond.first + "}}";

        size_t startPos = result.find(startTag);
        while (startPos != std::string::npos) {
            size_t endPos = result.find(endTag, startPos);
            if (endPos == std::string::npos) break;

            std::string blockContent = result.substr(
                startPos + startTag.length(),
                endPos - startPos - startTag.length()
            );

            std::string replacement = cond.second ? blockContent : "";
            result.replace(startPos, endPos + endTag.length() - startPos, replacement);
            startPos = result.find(startTag);
        }
    }

    return result;
}

} // namespace debugserver
```

### 4.4 정보 제공자 인터페이스

```cpp
// source/debugserver/InfoProvider.h
// 라이선스: GPL-2.0
#pragma once

#include "JsonBuilder.h"
#include "SimpleTemplate.h"
#include <string>
#include <map>

namespace debugserver {

class InfoProvider {
public:
    virtual ~InfoProvider() = default;

    // 변수 맵 생성 (템플릿용)
    virtual std::map<std::string, std::string> GetVariables() = 0;

    // 다양한 형식 출력
    virtual std::string ToJSON() = 0;
    virtual std::string ToHTML() = 0;
    virtual std::string ToText() = 0;

protected:
    // 유틸리티: 16진수 포맷
    static std::string ToHex8(uint8_t val);
    static std::string ToHex16(uint16_t val);
    static std::string ToHex32(uint32_t val);

    // HTML 이스케이프
    static std::string EscapeHTML(const std::string& str);
};

// 인라인 구현
inline std::string InfoProvider::ToHex8(uint8_t val) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X", val);
    return buf;
}

inline std::string InfoProvider::ToHex16(uint16_t val) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", val);
    return buf;
}

inline std::string InfoProvider::ToHex32(uint32_t val) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%08X", val);
    return buf;
}

inline std::string InfoProvider::EscapeHTML(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 20);
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c;
        }
    }
    return result;
}

} // namespace debugserver
```

### 4.5 설정 구조

```cpp
// source/frontends/common2/programoptions.h 에 추가
struct EmulatorOptions {
    // ... 기존 옵션들 ...

    // 디버그 HTTP 서버 옵션
    bool debugServerEnabled = false;
    std::string debugServerBindAddress = "127.0.0.1";  // 기본: 로컬만
    uint16_t debugServerPortMachine = 65501;
    uint16_t debugServerPortIO = 65502;
    uint16_t debugServerPortCPU = 65503;
    uint16_t debugServerPortMemory = 65504;
    std::string debugServerTemplateDir = "";  // 빈 문자열이면 내장 템플릿 사용
    bool debugServerMachineEnabled = true;
    bool debugServerIOEnabled = true;
    bool debugServerCPUEnabled = true;
    bool debugServerMemoryEnabled = true;
    size_t debugServerIOBufferSize = 1000;  // IO 이벤트 버퍼 크기
};
```

---

## 5. 출력 양식 정의

### 5.1 템플릿 문법 (내부 구현)

| 문법 | 설명 | 예시 |
|------|------|------|
| `{{변수}}` | 변수 치환 | `{{cpu_type}}` |
| `{{#배열}}...{{/배열}}` | 반복 블록 | `{{#slots}}...{{/slots}}` |
| `{{?조건}}...{{/조건}}` | 조건부 블록 | `{{?has_disk}}...{{/has_disk}}` |

### 5.2 내장 HTML 템플릿 (머신 정보)

```cpp
// source/debugserver/MachineInfoProvider.cpp 내 상수로 정의
static const char* DEFAULT_MACHINE_TEMPLATE = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>AppleWin Debug - Machine</title>
    <meta http-equiv="refresh" content="1">
    <style>
        body { font-family: monospace; background: #1e1e2e; color: #cdd6f4; margin: 20px; }
        h1 { color: #89b4fa; }
        h2 { color: #a6e3a1; border-bottom: 1px solid #45475a; }
        table { border-collapse: collapse; width: 100%; margin: 10px 0; }
        th, td { padding: 8px; text-align: left; border: 1px solid #45475a; }
        th { background: #313244; color: #cba6f7; }
        .value { color: #f9e2af; }
        .status-on { color: #a6e3a1; }
        .status-off { color: #f38ba8; }
    </style>
</head>
<body>
    <h1>Machine Information</h1>

    <h2>System</h2>
    <table>
        <tr><th>Property</th><th>Value</th></tr>
        <tr><td>Machine Type</td><td class="value">{{machine_type}}</td></tr>
        <tr><td>CPU Type</td><td class="value">{{cpu_type}}</td></tr>
        <tr><td>Clock Speed</td><td class="value">{{clock_speed}} MHz</td></tr>
    </table>

    <h2>Disk Drives</h2>
    <table>
        <tr><th>Drive</th><th>Status</th><th>Image</th><th>Protected</th></tr>
        <tr>
            <td>Drive 1</td>
            <td class="{{drive1_status_class}}">{{drive1_status}}</td>
            <td>{{drive1_image}}</td>
            <td>{{drive1_protected}}</td>
        </tr>
        <tr>
            <td>Drive 2</td>
            <td class="{{drive2_status_class}}">{{drive2_status}}</td>
            <td>{{drive2_image}}</td>
            <td>{{drive2_protected}}</td>
        </tr>
    </table>

    <h2>Expansion Slots</h2>
    <table>
        <tr><th>Slot</th><th>Card</th></tr>
        {{#slots}}
        <tr><td>Slot {{num}}</td><td>{{card}}</td></tr>
        {{/slots}}
    </table>

    <p style="color:#6c7086; font-size:12px;">Updated: {{timestamp}}</p>
</body>
</html>
)HTML";
```

### 5.3 내장 JSON 형식

JSON은 `JsonBuilder`를 사용하여 동적 생성:

```cpp
std::string MachineInfoProvider::ToJSON() {
    JsonBuilder json;
    json.BeginObject()
        .Add("machine_type", GetMachineTypeName())
        .Add("cpu_type", GetCPUTypeName())
        .Add("clock_speed", GetClockSpeed())
        .Key("drives").BeginArray();

    for (int i = 0; i < 2; i++) {
        json.BeginObject()
            .Add("id", i + 1)
            .Add("status", GetDriveStatus(i))
            .Add("image", GetDriveImage(i))
            .Add("protected", IsDriveProtected(i))
            .EndObject();
    }

    json.EndArray()
        .Key("slots").BeginArray();

    for (int i = 0; i < 8; i++) {
        json.BeginObject()
            .Add("slot", i)
            .Add("card", GetCardName(i))
            .EndObject();
    }

    json.EndArray()
        .Add("timestamp", GetTimestamp())
        .EndObject();

    return json.ToString();
}
```

---

## 6. 구현 단계

### Phase 1: 핵심 인프라 (외부 라이브러리 없음)

1. **HttpServer 클래스** (POSIX/Winsock 직접 사용)
   - 소켓 생성, 바인드, 리슨
   - Accept 루프 (별도 스레드)
   - 클라이언트 처리

2. **HttpRequest 파서** (내부 구현)
   - HTTP/1.0, HTTP/1.1 요청 파싱
   - 헤더 파싱
   - 쿼리 스트링 파싱
   - URL 디코딩

3. **HttpResponse 빌더** (내부 구현)
   - 상태 코드 설정
   - 헤더 설정
   - 본문 설정
   - HTTP 응답 직렬화

### Phase 2: 유틸리티 (외부 라이브러리 없음)

1. **JsonBuilder** (내부 구현)
   - 객체/배열 생성
   - 문자열 이스케이프
   - 다양한 타입 값 지원

2. **SimpleTemplate** (내부 구현)
   - 변수 치환 `{{}}`
   - 반복 블록 `{{#}}...{{/}}`
   - 조건부 블록 `{{?}}...{{/}}`

### Phase 3: 정보 제공자

1. **MachineInfoProvider**
   - CardManager 연동
   - Disk2CardManager 연동

2. **CPUInfoProvider**
   - regs 구조체 접근
   - 플래그 해석
   - 역어셈블 (기존 디버거 코드 활용)

3. **MemoryInfoProvider**
   - mem 포인터 접근
   - 메모리 모드 플래그
   - 16진수 덤프 포맷팅

4. **IOInfoProvider**
   - IO 이벤트 버퍼링
   - 링 버퍼 구현

### Phase 4: 통합

1. **DebugHttpServer 싱글톤**
   - 4개 서버 관리
   - 설정 적용
   - 라이프사이클 관리

2. **프론트엔드 통합**
   - 옵션 파싱
   - 시작/종료 처리

---

## 7. CMake 빌드 설정

```cmake
# source/debugserver/CMakeLists.txt
# 라이선스: GPL-2.0

set(DEBUGSERVER_SOURCES
    DebugHttpServer.cpp
    HttpServer.cpp
    HttpRequest.cpp
    HttpResponse.cpp
    JsonBuilder.cpp
    SimpleTemplate.cpp
    MachineInfoProvider.cpp
    IOInfoProvider.cpp
    CPUInfoProvider.cpp
    MemoryInfoProvider.cpp
)

set(DEBUGSERVER_HEADERS
    DebugHttpServer.h
    HttpServer.h
    HttpRequest.h
    HttpResponse.h
    JsonBuilder.h
    SimpleTemplate.h
    InfoProvider.h
    MachineInfoProvider.h
    IOInfoProvider.h
    CPUInfoProvider.h
    MemoryInfoProvider.h
)

add_library(debugserver STATIC ${DEBUGSERVER_SOURCES} ${DEBUGSERVER_HEADERS})

target_include_directories(debugserver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# C++17 필요
target_compile_features(debugserver PUBLIC cxx_std_17)

# 스레드 라이브러리 (C++ 표준)
find_package(Threads REQUIRED)
target_link_libraries(debugserver PRIVATE Threads::Threads)

# Windows에서 Winsock 링크
if(WIN32)
    target_link_libraries(debugserver PRIVATE ws2_32)
endif()

# 상위 프로젝트와 연결
target_link_libraries(debugserver PRIVATE appleii)
```

---

## 8. 보안 고려사항

1. **기본 바인드 주소**: `127.0.0.1` (로컬만)
2. **외부 접속**: 명시적 설정 필요 (`--debug-server-bind=0.0.0.0`)
3. **읽기 전용**: 모든 API는 읽기 전용
4. **버퍼 제한**: IO 이벤트 최대 개수 제한
5. **요청 크기 제한**: HTTP 요청 최대 크기 제한

---

## 9. 라이선스 요약

| 구성 요소 | 라이선스 | 비고 |
|----------|---------|------|
| debugserver 전체 | GPL-2.0 | AppleWin과 동일 |
| HttpServer | GPL-2.0 | 내부 구현 |
| JsonBuilder | GPL-2.0 | 내부 구현 |
| SimpleTemplate | GPL-2.0 | 내부 구현 |
| C++ 표준 라이브러리 | - | 시스템 제공 |
| POSIX 소켓 | - | 시스템 API |
| Winsock | - | Windows API |

**외부 라이브러리 의존성: 없음**

---

## 10. 파일 목록

| 파일 | 설명 | 라인 수 (예상) |
|------|------|---------------|
| `HttpServer.h/cpp` | HTTP 서버 (소켓) | ~400 |
| `HttpRequest.h/cpp` | HTTP 요청 파서 | ~250 |
| `HttpResponse.h/cpp` | HTTP 응답 빌더 | ~150 |
| `JsonBuilder.h/cpp` | JSON 생성기 | ~200 |
| `SimpleTemplate.h/cpp` | 템플릿 엔진 | ~200 |
| `InfoProvider.h` | 기본 인터페이스 | ~50 |
| `MachineInfoProvider.h/cpp` | 머신 정보 | ~300 |
| `IOInfoProvider.h/cpp` | IO 정보 | ~250 |
| `CPUInfoProvider.h/cpp` | CPU 정보 | ~300 |
| `MemoryInfoProvider.h/cpp` | 메모리 정보 | ~350 |
| `DebugHttpServer.h/cpp` | 메인 서버 | ~400 |
| `CMakeLists.txt` | 빌드 설정 | ~30 |
| **총계** | | **~2,880 라인** |

---

## 11. 참고: 기존 코드 활용

디버거 관련 기존 코드에서 재사용 가능한 부분:

- `source/Debugger/Debugger_Disassembler.cpp`: 역어셈블 로직
- `source/Debugger/Debug.cpp`: 메모리 덤프 포맷팅
- `source/frontends/sdl/imgui/sdldebugger.cpp`: 정보 표시 로직

---

## 12. 사용 예시

### 커맨드라인 옵션

```bash
# 디버그 서버 활성화
./sa2 --debug-server

# 특정 포트만 활성화
./sa2 --debug-server --debug-cpu-port=8080

# 외부 접속 허용 (주의)
./sa2 --debug-server --debug-server-bind=0.0.0.0

# 커스텀 템플릿 디렉토리
./sa2 --debug-server --debug-template-dir=/path/to/templates
```

### 접속 테스트

```bash
# 머신 정보 (HTML)
curl http://localhost:65501/

# CPU 정보 (JSON)
curl http://localhost:65503/?format=json

# 메모리 덤프
curl "http://localhost:65504/dump?start=0x0000&length=256"
```
