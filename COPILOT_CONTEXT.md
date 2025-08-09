# GitHub Copilot 대화 컨텍스트 가이드

## 🤖 다음 세션에서 Copilot과 대화를 이어가는 방법

### 1. 📂 프로젝트 파일 열기
VS Code에서 이 프로젝트 폴더를 열면 Copilot이 자동으로 프로젝트 구조를 인식합니다.

### 2. 💬 대화 시작 시 언급할 키워드
다음 세션에서 Copilot에게 이렇게 말하세요:

```
"이전에 M1 Mac FFmpeg 프로젝트를 함께 개발했는데, 
VideoToolbox 하드웨어 가속과 GUI 비디오 플레이어까지 완성했어. 
SDL2 기반 윈도우 플레이어와 루프 재생 기능도 모두 구현했고,
DEVELOPMENT_LOG.md 파일을 참고해서 이어서 작업하고 싶어."
```

### 3. 🔍 컨텍스트 힌트 제공
- **현재 상태**: "GUI 비디오 플레이어와 하드웨어 가속 벤치마크까지 완성"
- **기술 스택**: "FFmpeg 7.1.1 + M1 VideoToolbox + SDL2 + C++17 + 멀티스레딩"
- **핵심 파일**: 
  - `examples/advanced/gui_video_player.cpp` (SDL2 GUI 플레이어, 루프 재생)
  - `examples/advanced/hardware_decoder.cpp` (루프 벤치마크, HW→SW 전송)
  - `examples/advanced/video_player.cpp` (콘솔 비디오 플레이어)
  - `CMakeLists.txt` (SDL2 통합, M1 최적화 빌드)

### 4. 🎯 진행 상황 공유
"GUI 비디오 플레이어(SDL2)가 완성되어서 실제 윈도우에서 비디오 재생되고,
하드웨어 디코더는 루프 재생으로 420+ FPS 성능이 나와. 
멀티스레딩과 자동 루프, 키보드 제어까지 모두 구현했어."

## 📝 프로젝트 상태 요약 (Copilot 참고용)

### ✅ 완성된 기능들
- M1 Mac VideoToolbox 하드웨어 가속 디코딩 (루프 벤치마크 포함)
- SDL2 기반 GUI 비디오 플레이어 (윈도우 기반 실시간 재생)
- 멀티스레드 아키텍처 (디코더/렌더러 분리)
- 자동 루프 재생 (EOF 감지 시 seek & flush)
- 키보드 제어 (재생/일시정지, 속도 조절)
- 실시간 콘솔 비디오 플레이어  
- RTMP 라이브 스트리밍
- 실시간 비디오 필터링
- VS Code 완전 통합 (IntelliSense, 디버깅, 빌드)

### 🔧 현재 빌드 상태
모든 예제가 빌드되고 정상 실행됨:
- ffmpeg-info, video-analysis, frame-extract, simple-encoder
- hardware-decoder (루프 모드 추가), video-filter, rtmp-streamer, video-player
- gui-video-player (SDL2 GUI 플레이어) ⭐ 신규!

### 🎬 테스트 완료 항목
- H.264/HEVC 하드웨어 디코딩 (420+ FPS, 루프 재생)
- SDL2 GUI 윈도우에서 실시간 비디오 재생
- 하드웨어→소프트웨어 프레임 전송 (NV12 포맷)
- 멀티스레드 Producer-Consumer 패턴
- 자동 루프 재생 및 디코더 플러시
- 동적 픽셀 포맷 변환 (SwsContext)
- 키보드 이벤트 처리 및 재생 속도 제어
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
