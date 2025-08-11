# Windows 개발 환경 설정 가이드

이 프로젝트는 원래 M1 Mac에서 개발되었지만, Windows에서도 개발할 수 있도록 수정되었습니다.

## Windows 개발 환경 요구사항

### 필수 도구
- Windows 10/11
- Visual Studio 2019/2022 (Community Edition 이상)
- CMake 3.20 이상
- Git

### FFmpeg 설치

#### 방법 1: 자동 설치 (권장)
```powershell
# Windows Package Manager 사용
winget install --id=Gyan.FFmpeg

# 개발용 라이브러리 다운로드 (프로젝트 디렉토리에서 실행)
Invoke-WebRequest -Uri "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip" -OutFile "ffmpeg-shared.zip"
Expand-Archive -Path "ffmpeg-shared.zip" -DestinationPath "." -Force
```

#### 방법 2: 수동 설치
1. [FFmpeg Windows builds](https://github.com/BtbN/FFmpeg-Builds/releases) 에서 `ffmpeg-master-latest-win64-gpl-shared.zip` 다운로드
2. 프로젝트 루트 디렉토리에 압축 해제

## 빌드 방법

```powershell
# 프로젝트 클론
git clone https://github.com/HyunWookKim/ffmpeg-study.git
cd ffmpeg-study

# CMake 구성
cmake -B build -S .

# 빌드
cmake --build build --config Debug
```

## 플랫폼별 차이점

### 하드웨어 가속
- **macOS**: VideoToolbox 사용
- **Windows**: D3D11VA/DXVA2 사용 (사용 불가능시 소프트웨어 디코딩)
- **Linux**: VAAPI 사용

### 실행 파일 위치
빌드된 실행 파일은 다음 위치에 생성됩니다:
```
build/Debug/          # Windows (Visual Studio)
  ├── ffmpeg-info.exe
  ├── video-analysis.exe
  ├── frame-extract.exe
  ├── hardware-decoder.exe
  └── ...

build/                # macOS/Linux
  ├── ffmpeg-info
  ├── video-analysis
  ├── frame-extract
  └── ...
```

## 사용 예제

```powershell
# 미디어 정보 분석
.\build\Debug\ffmpeg-info.exe .\media\samples\h264_sample.mp4

# 하드웨어 가속 테스트
.\build\Debug\hardware-decoder.exe .\media\samples\h264_sample.mp4

# 비디오 분석
.\build\Debug\video-analysis.exe .\media\samples\h264_sample.mp4
```

## 문제 해결

### 1. FFmpeg 라이브러리를 찾을 수 없는 경우
- `ffmpeg-master-latest-win64-gpl-shared` 폴더가 프로젝트 루트에 있는지 확인
- CMake 캐시 삭제 후 재구성: `rm -rf build && cmake -B build -S .`

### 2. 문자 인코딩 경고
- 현재 CMakeLists.txt에서 `/utf-8` 옵션으로 해결됨
- 추가 문제 발생시 Visual Studio에서 "UTF-8 with BOM" 으로 파일 저장

### 3. 하드웨어 가속 실패
- Windows에서 하드웨어 가속이 지원되지 않는 경우 자동으로 소프트웨어 디코딩으로 전환
- 성능이 떨어질 수 있지만 정상 동작함

## 성능 비교

| 플랫폼 | 하드웨어 가속 | 예상 성능 |
|--------|---------------|-----------|
| M1 Mac | VideoToolbox | 420+ FPS |
| Windows (GPU) | D3D11VA/DXVA2 | 200-300 FPS |
| Windows (CPU) | Software | 50-100 FPS |

## 개발 팁

### Visual Studio 설정
1. 솔루션 파일: `build/ffmpeg-study.sln` 열기
2. 디버깅 시 작업 디렉토리를 프로젝트 루트로 설정
3. 명령 인수에 테스트 파일 경로 지정

### VS Code 설정
`.vscode/launch.json` 예제:
```json
{
    "configurations": [
        {
            "name": "Debug ffmpeg-info",
            "type": "cppvsdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/Debug/ffmpeg-info.exe",
            "args": ["media/samples/h264_sample.mp4"],
            "cwd": "${workspaceFolder}"
        }
    ]
}
```
