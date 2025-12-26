# AppleWin Linux 컴파일 가이드

AppleWin은 Apple II 에뮬레이터입니다. CMake를 사용하여 빌드합니다.

## 프로젝트 정보

이 프로젝트는 [audetto의 AppleWin Linux 포트](https://github.com/audetto/AppleWin)를 기반으로 한 확장 Fork입니다.

| 원본 프로젝트 | URL |
|-------------|-----|
| AppleWin (Windows) | https://github.com/AppleWin/AppleWin |
| AppleWin Linux 포트 | https://github.com/audetto/AppleWin |

## 요구사항

- CMake 3.13 이상
- C++17 지원 컴파일러 (g++, clang++)
- Git (서브모듈 클론용)

---

## 1. 필수 패키지 설치 (빌드 전 반드시 설치)

> **중요**: 아래 패키지들을 먼저 설치해야 빌드가 가능합니다.

### Ubuntu/Debian

**필수 패키지:**
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    g++ \
    xxd \
    ninja-build \
    pkg-config \
    libncurses-dev \
    libevdev-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libgles-dev \
    libslirp-dev
```

**Qt 프론트엔드 (선택):**
```bash
# Qt5
sudo apt-get install -y \
    qtbase5-dev \
    qtmultimedia5-dev \
    libqt5gamepad5-dev \
    libqt5multimedia5-plugins

# 또는 Qt6
sudo apt-get install -y \
    qt6-base-dev \
    qt6-multimedia-dev
```

### Fedora

**필수 패키지:**
```bash
sudo dnf install -y \
    git \
    cmake \
    gcc-c++ \
    xxd \
    rpm-build \
    ncurses-devel \
    libevdev-devel \
    SDL2-devel \
    SDL2_image-devel \
    libglvnd-devel \
    libslirp-devel \
    libpcap-devel
```

**Qt 프론트엔드 (선택):**
```bash
sudo dnf install -y \
    qt5-qtbase-devel \
    qt5-qtmultimedia-devel \
    qt5-qtgamepad-devel
```

---

## 2. 빌드 프론트엔드 종류

| 프론트엔드 | 설명 | 필수 여부 |
|-----------|------|----------|
| **sa2** | SDL2 + ImGui (권장) | 선택 |
| **applen** | ncurses 터미널 기반 | 선택 |
| **qapple** | Qt5/Qt6 GUI | 선택 |
| **ra2** | libretro 코어 | 선택 |

> **참고**: libretro(ra2)는 RetroArch 등 libretro 호환 프론트엔드에서 사용하는 코어입니다.
> 일반 사용자는 **sa2** (SDL2) 프론트엔드만 빌드해도 충분합니다.

---

## 3. 빌드 방법

### 3.1 소스 클론 (서브모듈 포함)

```bash
git clone --recursive https://github.com/audetto/AppleWin.git
cd AppleWin
```

기존 클론에서 서브모듈이 누락된 경우:
```bash
git submodule update --init --recursive
```

### 3.2 빌드 디렉토리 생성

```bash
mkdir build
cd build
```

### 3.3 CMake 설정

**전체 빌드 (모든 프론트엔드):**
```bash
cmake -DCMAKE_BUILD_TYPE=RELEASE ..
```

**권장: SDL2 프론트엔드만 빌드:**
```bash
cmake -DBUILD_SA2=ON -DCMAKE_BUILD_TYPE=RELEASE ..
```

**ncurses + SDL2 프론트엔드 빌드:**
```bash
cmake -DBUILD_SA2=ON -DBUILD_APPLEN=ON -DCMAKE_BUILD_TYPE=RELEASE ..
```

### 3.4 컴파일

```bash
make -j $(nproc)
```

### 3.5 실행

```bash
./sa2      # SDL2 프론트엔드
./applen   # ncurses 프론트엔드
./qapple   # Qt 프론트엔드
```

실행 시 **Debug HTTP Server**가 자동으로 시작됩니다:
```
AppleWin Debug Server started:
  Machine Info: http://127.0.0.1:65501/
  I/O Info:     http://127.0.0.1:65502/
  CPU Info:     http://127.0.0.1:65503/
  Memory Info:  http://127.0.0.1:65504/
```

---

## 4. CMake 옵션

| 옵션 | 설명 |
|-----|------|
| `-DBUILD_SA2=ON` | SDL2 프론트엔드 빌드 |
| `-DBUILD_APPLEN=ON` | ncurses 프론트엔드 빌드 |
| `-DBUILD_QAPPLE=ON` | Qt 프론트엔드 빌드 |
| `-DBUILD_LIBRETRO=ON` | libretro 코어 빌드 |
| `-DQAPPLE_USE_QT5=ON` | Qt6 대신 Qt5 사용 |
| `-DCMAKE_BUILD_TYPE=DEBUG` | 디버그 빌드 |
| `-DCMAKE_BUILD_TYPE=RELEASE` | 릴리즈 빌드 |

> **참고**: 프론트엔드 옵션을 지정하지 않으면 모든 프론트엔드가 빌드됩니다.

---

## 5. 네트워크 기능 (Uthernet)

libpcap 사용 시 권한 설정 필요:
```bash
sudo setcap cap_net_raw=ep ./sa2
```

---

## 6. Debug HTTP Server

이 Fork에서 추가된 기능으로, 에뮬레이터 실행 시 자동으로 HTTP 디버그 서버가 시작됩니다.

### 6.1 포트 및 기능

| 포트 | 기능 | URL |
|-----|------|-----|
| 65501 | Machine Info | http://127.0.0.1:65501/ |
| 65502 | I/O Info (소프트 스위치, 슬롯) | http://127.0.0.1:65502/ |
| 65503 | CPU Info (레지스터, 브레이크포인트) | http://127.0.0.1:65503/ |
| 65504 | Memory Info (메모리 덤프) | http://127.0.0.1:65504/ |

### 6.2 API 사용 예제

```bash
# 머신 정보 조회
curl http://127.0.0.1:65501/api/info

# CPU 레지스터 조회
curl http://127.0.0.1:65503/api/registers

# 메모리 덤프 ($C600부터 8줄)
curl "http://127.0.0.1:65504/api/dump?addr=0xC600&lines=8"

# 확장 슬롯 카드 정보
curl http://127.0.0.1:65502/api/slots

# 브레이크포인트 목록
curl http://127.0.0.1:65503/api/breakpoints
```

### 6.3 웹 대시보드

브라우저에서 각 포트의 루트 URL (예: http://127.0.0.1:65501/)에 접속하면 실시간 대시보드를 확인할 수 있습니다.

> 자세한 API 문서: [source/debugserver/README.md](source/debugserver/README.md)

---

## 7. 설치 및 패키지 생성

**시스템 설치:**
```bash
sudo make install
```

**패키지 생성:**
```bash
cpack -G DEB  # Debian 패키지
cpack -G RPM  # RPM 패키지
```

---

## 8. 문제 해결

| 문제 | 해결 방법 |
|-----|---------|
| 서브모듈 누락 | `git submodule update --init --recursive` |
| CMake 버전 오류 | CMake 3.13 이상 설치 |
| Qt 빌드 실패 | Qt 개발 패키지 설치 확인 |
| SDL2 관련 오류 | `libsdl2-dev`, `libsdl2-image-dev` 설치 확인 |
| OpenGL ES 오류 | `libgles-dev` (Ubuntu) 또는 `libglvnd-devel` (Fedora) 설치 |
| Debug Server 포트 사용 중 | 65501-65504 포트가 사용 가능한지 확인 |
| curl 연결 실패 | 에뮬레이터가 실행 중인지 확인, `http://127.0.0.1:65501/` 접속 테스트 |

---

## 9. 관련 문서

- [README.md](README.md) - 프로젝트 개요
- [source/debugserver/README.md](source/debugserver/README.md) - Debug HTTP Server API 문서
- [.github/README.md](.github/README.md) - Linux 포트 상세 문서
- [source/frontends/sdl/README.md](source/frontends/sdl/README.md) - SDL2 프론트엔드 옵션
