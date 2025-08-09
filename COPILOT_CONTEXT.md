# GitHub Copilot 대화 컨텍스트 가이드

## 🤖 다음 세션에서 Copilot과 대화를 이어가는 방법

### 1. 📂 프로젝트 파일 열기
VS Code에서 이 프로젝트 폴더를 열면 Copilot이 자동으로 프로젝트 구조를 인식합니다.

### 2. 💬 대화 시작 시 언급할 키워드
다음 세션에서 Copilot에게 이렇게 말하세요:

```
"이전에 M1 Mac FFmpeg 프로젝트를 함께 개발했는데, 
VideoToolbox 하드웨어 가속과 실시간 비디오 플레이어까지 완성했어. 
DEVELOPMENT_LOG.md 파일을 참고해서 이어서 작업하고 싶어."
```

### 3. 🔍 컨텍스트 힌트 제공
- **현재 상태**: "모든 기본 기능이 완성되어 있고 GitHub에 업로드됨"
- **기술 스택**: "FFmpeg 7.1.1 + M1 Mac VideoToolbox + C++17 + CMake"
- **핵심 파일**: 
  - `examples/advanced/video_player.cpp` (멀티스레드 비디오 플레이어)
  - `examples/advanced/hardware_decoder.cpp` (M1 하드웨어 가속)
  - `CMakeLists.txt` (M1 최적화 빌드)

### 4. 🎯 진행 상황 공유
"hardware-decoder로 VideoToolbox 하드웨어 가속이 정상 동작하고,
video-player로 실시간 재생도 완성했어. 다음엔 [원하는 기능]을 추가하고 싶어."

## 📝 프로젝트 상태 요약 (Copilot 참고용)

### ✅ 완성된 기능들
- M1 Mac VideoToolbox 하드웨어 가속 디코딩
- 실시간 멀티스레드 비디오 플레이어  
- RTMP 라이브 스트리밍
- 실시간 비디오 필터링
- VS Code 완전 통합 (IntelliSense, 디버깅, 빌드)

### 🔧 현재 빌드 상태
모든 예제가 빌드되고 정상 실행됨:
- ffmpeg-info, video-analysis, frame-extract, simple-encoder
- hardware-decoder, video-filter, rtmp-streamer, video-player

### 🎬 테스트 완료 항목
- H.264/HEVC 하드웨어 디코딩 (330+ FPS)
- 실시간 비디오 재생 (하드웨어 가속)
- 다양한 비디오 필터 효과
- RTMP 스트리밍 (웹캠 지원)

### 🚀 향후 확장 가능 영역
- WebRTC 브라우저 통신
- Core ML AI 분석
- Metal Performance Shaders
- Qt 기반 GUI
- 클라우드 스트리밍 연동

## 💡 Copilot에게 효과적으로 컨텍스트 전달하는 팁

### 1. 파일 첨부하기
대화 시 관련 파일을 첨부하면 Copilot이 현재 상태를 더 잘 이해합니다:
- `DEVELOPMENT_LOG.md` (전체 개발 과정)
- `README.md` (프로젝트 개요)
- `ADVANCED_GUIDE.md` (상세 기능 설명)

### 2. 구체적인 요청하기
❌ "비디오 관련 기능 추가해줘"
✅ "video_player.cpp에 재생 속도 조절 기능을 추가하고 싶어. 현재 멀티스레드 구조를 유지하면서 1x, 2x, 0.5x 속도로 재생할 수 있게 해줘."

### 3. 현재 상태 명시하기
"현재 VideoToolbox 하드웨어 가속이 정상 동작하고 있고, 
video_player.cpp의 producer_thread와 display_thread가 
frame_queue로 통신하는 구조야."

## 🔄 세션 연결 체크리스트

다음 세션 시작 시 확인사항:

- [ ] 프로젝트 폴더가 VS Code에서 열려있는가?
- [ ] `DEVELOPMENT_LOG.md` 파일을 Copilot에게 보여줬는가?
- [ ] 현재 작업하고 싶은 구체적인 목표를 명시했는가?
- [ ] 기존 코드의 어떤 부분을 수정/확장하고 싶은지 설명했는가?

이렇게 하면 Copilot이 이전 대화 내용을 빠르게 파악하고 
연속성 있는 개발을 도와줄 수 있습니다! 🚀
