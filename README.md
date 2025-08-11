# FFmpeg Study - Cross-Platform Video Processing Project

[![Cross-Platform](https://img.shields.io/badge/Platform-Windows%20|%20macOS%20|%20Linux-blue?style=flat)](https://github.com/HyunWookKim/ffmpeg-study)
[![FFmpeg](https://img.shields.io/badge/FFmpeg-7.1.1-green?style=flat&logo=ffmpeg)](https://ffmpeg.org/)
[![C++17](https://img.shields.io/badge/C++-17-blue?style=flat&logo=cplusplus)](https://isocpp.org/)
[![SDL2](https://img.shields.io/badge/SDL2-2.28.5-orange?style=flat)](https://www.libsdl.org/)

A comprehensive project for learning advanced video processing with FFmpeg and hardware acceleration across Windows, macOS, and Linux platforms.

## ✨ Key Features

### 🚀 Hardware Acceleration
- **VideoToolbox**: Apple Silicon M1/M2 hardware acceleration (macOS)
- **Software Fallback**: Reliable software decoding on all platforms
- **H.264/HEVC Support**: Hardware and software decoding for major codecs
- **Performance Optimization**: 60-80% CPU usage reduction with hardware acceleration

### 🎬 Advanced Examples
- **Video Player**: Real-time hardware-accelerated playback
- **GUI Video Player**: SDL2-based windowed player with loop playback and speed control
- **Hardware Acceleration Benchmark**: Loop playback and performance measurement
- **Real-time Streaming**: RTMP protocol support
- **Video Filters**: Real-time video effects processing
- **Multithreaded Processing**: Low-latency video pipeline

### 🎮 GUI Player Features
- **Hardware Acceleration**: VideoToolbox-based H.264/HEVC decoding (macOS)
- **SDL2 Rendering**: High-performance YUV texture rendering
- **Multithreading**: Separate decoder/renderer threads for smooth playback
- **Keyboard Controls**: Play/pause, speed control (0.25x~4x)
- **Auto Loop**: Automatic seek and decoder flush on EOF
- **Performance Monitoring**: Real-time frame counter and status display

### 🔧 Development Environment
- **VS Code Integration**: IntelliSense, debugging, build tasks
- **CMake Build**: Cross-platform optimized configuration
- **Multiple Codecs**: H.264, HEVC, VP9, AV1, ProRes

## 🏗️ Project Structure

```
ffmpeg-study/
├── src/                          # 기본 예제
│   └── ffmpeg_info.cpp          # 미디어 정보 분석
├── examples/                     # 비디오 처리 예제
│   ├── video_analysis.cpp       # 프레임별 분석
│   ├── frame_extraction.cpp     # 프레임 추출
│   ├── simple_encoder.cpp       # 비디오 인코더
│   └── advanced/                # 고급 예제
│       ├── hardware_decoder.cpp # 하드웨어 가속 디코더
│       ├── video_filter.cpp     # 비디오 필터
│       ├── rtmp_streamer.cpp    # 실시간 스트리밍
│       ├── realtime_processor.cpp # 실시간 처리
│       ├── video_player.cpp     # 비디오 플레이어
│       └── gui_video_player.cpp # GUI 비디오 플레이어
├── media/samples/               # 테스트 비디오 파일
├── .vscode/                     # VS Code 설정
└── build/                       # 빌드 출력
```

## 💻 Platform Support

This project supports multiple platforms with platform-specific optimizations:

### Windows
- **FFmpeg**: Shared libraries from BtbN builds
- **SDL2**: Official development libraries
- **Hardware Acceleration**: Software decoding (reliable fallback)
- **Compiler**: Visual Studio 2022 with UTF-8 support

### macOS (Apple Silicon)
- **FFmpeg**: Homebrew installation with VideoToolbox
- **SDL2**: Homebrew installation
- **Hardware Acceleration**: VideoToolbox for H.264/HEVC
- **Compiler**: Clang with macOS SDK

### Linux
- **FFmpeg**: Package manager or manual compilation
- **SDL2**: Development packages
- **Hardware Acceleration**: VAAPI/VDPAU support
- **Compiler**: GCC/Clang

## 🚀 Quick Start

### Windows Setup

1. **Download FFmpeg**:
   ```powershell
   # Download from https://github.com/BtbN/FFmpeg-Builds/releases
   # Extract to project root as 'ffmpeg-master-latest-win64-gpl-shared'
   ```

2. **Download SDL2** (for GUI player):
   ```powershell
   # Download from https://github.com/libsdl-org/SDL/releases
   # Extract to project root as 'SDL2-2.28.5'
   ```

3. **Build**:
   ```powershell
   cmake -B build -S .
   cmake --build build
   ```

4. **Run Examples**:
   ```powershell
   .\build\Debug\ffmpeg-info.exe .\media\samples\h264_sample.mp4
   .\build\Debug\gui-video-player.exe .\media\samples\h264_sample.mp4
   ```

### Windows + Visual Studio 2022 Setup 🎯

For full IDE development experience with Visual Studio 2022:

1. **Prerequisites**: 
   - Visual Studio 2022 (Community/Professional/Enterprise)
   - Follow Windows Setup steps above (FFmpeg + SDL2)

2. **Generate Visual Studio Project**:
   ```powershell
   # Generate Visual Studio 2022 solution files
   cmake -B build-vs -S . -G "Visual Studio 17 2022" -A x64
   ```

3. **Open in Visual Studio**:
   ```powershell
   # Method 1: Open from PowerShell
   start .\build-vs\ffmpeg-study.sln
   
   # Method 2: Open in Visual Studio
   # File → Open → Project/Solution → build-vs/ffmpeg-study.sln
   ```

4. **Visual Studio Features**:
   - **Solution Explorer**: All 9 projects organized
   - **IntelliSense**: Full FFmpeg API auto-completion
   - **Debugging**: Set breakpoints, step through code (F5)
   - **Build**: Individual projects or entire solution (Ctrl+Shift+B)
   - **Project Structure**:
     ```
     ffmpeg-study (Solution)
     ├── ffmpeg-info          # Media information analysis
     ├── video-analysis       # Frame-by-frame analysis  
     ├── frame-extract        # Frame extraction utility
     ├── simple-encoder       # Video encoding examples
     ├── hardware-decoder     # Hardware acceleration testing
     ├── video-filter         # Real-time video effects
     ├── rtmp-streamer        # Live streaming
     ├── video-player         # Console video player
     └── gui-video-player     # SDL2 GUI player ⭐
     ```

5. **Building and Running**:
   - **Build All**: Right-click solution → Build Solution
   - **Run Project**: Right-click project → Set as StartUp Project → F5
   - **Debug Mode**: Full debugging with breakpoints
   - **Release Mode**: Optimized performance builds

6. **CMake Integration**:
   - **Main Config**: `CMakeLists.txt` (project root)
   - **Regenerate**: Re-run cmake command to update project files
   - **Clean Build**: Delete `build-vs` folder and regenerate

### macOS Setup

1. **Install Dependencies**:
   ```bash
   brew install ffmpeg cmake sdl2
   ```

2. **Build**:
   ```bash
   cmake -B build -S .
   make -C build -j4
   ```

3. **Run Examples**:
   ```bash
   ./build/ffmpeg-info media/samples/h264_sample.mp4
   ./build/gui-video-player media/samples/h264_sample.mp4
   ```

### General Requirements
- **CMake**: 3.20 or higher
- **C++ Compiler**: C++17 support required
- **FFmpeg**: 7.1.1 (or compatible version)
- **SDL2**: 2.28.5 (for GUI applications)

### Windows Specific
- **Windows 10/11**: x64 architecture
- **Compiler Options**:
  - **Command Line**: Visual Studio 2022 Build Tools + CMake
  - **IDE Development**: Visual Studio 2022 (Community/Professional/Enterprise)
- **PowerShell**: For build commands
- **FFmpeg**: Pre-built shared libraries (BtbN builds)
- **SDL2**: Official development libraries for GUI support

### macOS Specific  
- **macOS**: 11.0 Big Sur or higher
- **Apple Silicon**: M1/M2 recommended for hardware acceleration
- **Xcode**: Command line tools

## 📖 Detailed Guide

For comprehensive information about all features and usage, please refer to [**ADVANCED_GUIDE.md**](ADVANCED_GUIDE.md) and [**WINDOWS_SETUP.md**](WINDOWS_SETUP.md).

## 🎯 Example Showcase

### 1. Hardware Accelerated Decoding
```bash
# Single playback
./build/hardware-decoder media/samples/hevc_sample.mp4

# Loop playback (10 seconds performance measurement)
./build/hardware-decoder media/samples/h264_sample.mp4 loop
```
```
🍎 M1 Mac Hardware Accelerated Video Decoder
✅ VideoToolbox hardware acceleration initialized!
🚀 Found hevc decoder with VideoToolbox support
🔄 Loop 1: End of file reached, restarting from beginning (59 frames processed)
📊 Frame 100 | HW: 100 | SW: 0 | Average FPS: 420.5
🔄 HW→SW transfer successful: nv12 (640x480)

=== Final Benchmark Results ===
Average decoding speed: 425.20 FPS
Hardware acceleration ratio: 100.0%
```

### 2. Real-time Video Player
```bash
./build/video-player media/samples/h264_sample.mp4
```
```
🎬 Hardware Accelerated Video Player
📹 Duration: 2.36 seconds | 640x480 @ 25 FPS
Hardware acceleration: YES (VideoToolbox)
🎬 Frame 30 | Time: 1.20s | 🖥️ HW | Queue: 2
```

### 3. GUI Video Player ⭐ New!
```bash
./build/gui-video-player media/samples/h264_sample.mp4
```
**Key Features:**
- 🖥️ **Hardware Acceleration**: VideoToolbox-based H.264/HEVC decoding
- 🎬 **SDL2 GUI**: Window-based real-time playback
- 🔄 **Auto Loop**: Automatic seek and restart on EOF  
- ⚡ **Keyboard Controls**: 
  - `SPACE`: Play/pause
  - `↑/↓`: Speed control (0.25x~4x)
  - `ESC/Q`: Exit
- 🧵 **Multithreading**: Separate decoder/renderer threads
- 📊 **Real-time Monitoring**: Frame counter, speed display

### 4. Video Filter Processing
```bash
./build/video-filter input.mp4 blurred.mp4 blur
```
- Gaussian blur, scaling, brightness adjustment
- Rotation, edge detection, vintage effects
- Real-time filter chain support

### 5. RTMP Live Streaming
```bash
./build/rtmp-streamer webcam rtmp://localhost/live/test
```
- Real-time webcam capture
- Low-latency H.264 encoding
- YouTube Live, Twitch compatible

## 📊 Performance Benchmarks

| Feature | Software | Hardware Acceleration | Improvement |
|---------|----------|----------------------|-------------|
| H.264 Decoding | ~1,000 FPS | ~330 FPS* | 60% CPU savings |
| HEVC Decoding | ~500 FPS | ~330 FPS* | 70% CPU savings |
| Real-time Filtering | 15 FPS | 30 FPS | 2x performance |
| Battery Life | Baseline | +40% | Power efficiency |

*Hardware acceleration actual throughput includes memory transfer

## 🛠️ Development Environment Options

### Option 1: VS Code (Cross-Platform)

The project is fully integrated with VS Code:

- **IntelliSense**: FFmpeg API auto-completion
- **Debugging**: Integrated GDB debugger
- **Build Tasks**: One-click build and test
- **Problem Detection**: Real-time compilation error detection

**Build in VS Code:**
1. `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Windows/Linux)
2. Select "Tasks: Run Task"
3. Select "build"

### Option 2: Visual Studio 2022 (Windows)

For Windows developers who prefer full IDE experience:

- **Project Generation**: `cmake -B build-vs -S . -G "Visual Studio 17 2022" -A x64`
- **Solution File**: `build-vs\ffmpeg-study.sln`
- **Features**:
  - **Full Debugging**: Breakpoints, step-through, watch variables
  - **IntelliSense**: Advanced code completion and analysis
  - **Project Management**: Solution Explorer with organized projects
  - **Build Configuration**: Debug/Release modes
  - **Integrated Git**: Built-in version control

**Getting Started with Visual Studio:**
1. Generate project: `cmake -B build-vs -S . -G "Visual Studio 17 2022" -A x64`
2. Open solution: `start .\build-vs\ffmpeg-study.sln`
3. Set startup project: Right-click desired project → "Set as StartUp Project"
4. Build and run: Press F5 for debugging or Ctrl+F5 for release run

### Command Line Build (Universal)
```bash
# Project configuration
cmake -B build -S .

# Build
cmake --build build
```

## 🔧 Extensions and Customization

### Adding New Filters
```cpp
// In video_filter.cpp
else if (filter_name == "my_custom_filter") {
    return "your_ffmpeg_filter_chain_here";
}
```

### Hardware Acceleration Extensions
- CUDA (NVIDIA GPU)
- OpenCL 
- Metal Performance Shaders

## 📚 Learning Resources

- [FFmpeg Official Documentation](https://ffmpeg.org/documentation.html)
- [Apple VideoToolbox](https://developer.apple.com/documentation/videotoolbox)
- [libav* API Reference](https://ffmpeg.org/doxygen/trunk/index.html)

## 📝 Learning Notes

### FFmpeg Key Structures
- `AVFormatContext`: Container format information
- `AVCodecContext`: Codec configuration and state
- `AVFrame`: Decoded frame data
- `AVPacket`: Encoded packet data

### Important Functions
- `avformat_open_input()`: Open file
- `avformat_find_stream_info()`: Find stream information
- `avcodec_find_decoder/encoder()`: Find codec
- `avcodec_send_packet/receive_frame()`: Decoding
- `avcodec_send_frame/receive_packet()`: Encoding

### Memory Management
- All FFmpeg objects require proper cleanup function calls
- `av_frame_alloc()` → `av_frame_free()`
- `av_packet_alloc()` → `av_packet_free()`
- `avcodec_alloc_context3()` → `avcodec_free_context()`

### Project Configuration
- **Main CMake Config**: `CMakeLists.txt` (project root)
- **Cross-Platform Support**: Automatic FFmpeg and SDL2 detection
- **Build Generators**:
  - **VS Code/Command Line**: `cmake -B build -S .`
  - **Visual Studio 2022**: `cmake -B build-vs -S . -G "Visual Studio 17 2022" -A x64`
  - **Ninja (Fast)**: `cmake -B build-ninja -S . -G "Ninja"`
- **Build Outputs**: 
  - **Standard**: `build/` directory
  - **Visual Studio**: `build-vs/` directory with `.sln` and `.vcxproj` files
- **Regeneration**: Delete build directory and re-run cmake to update configuration

## 🚀 Roadmap

- [ ] **WebRTC Support**: Browser-based real-time communication
- [ ] **Core ML Integration**: AI-based video analysis
- [ ] **Cloud Integration**: AWS, GCP streaming services
- [ ] **GUI Interface**: Qt-based visual editor

## 🤝 Contributing

Contributions to this project are welcome! 

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📝 License

This project is distributed under the MIT License. See [LICENSE](LICENSE) file for more information.

## 👨‍💻 Author

**HyunWook Kim** - [@HyunWookKim](https://github.com/HyunWookKim)

Project Link: [https://github.com/HyunWookKim/ffmpeg-study](https://github.com/HyunWookKim/ffmpeg-study)

---

⭐ Please give this project a star if you found it helpful!

## 🎬 Test Media Files

Add test video files to the `media/` folder:
- Small size MP4 files recommended
- Test with various codecs (H.264, H.265, VP9, etc.)

## 📖 추가 학습 자료

- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [FFmpeg Tutorial](https://github.com/leandromoreira/ffmpeg-libav-tutorial)
- [Doxygen API Reference](https://www.ffmpeg.org/doxygen/trunk/)

## 🤝 기여하기

새로운 예제나 개선사항이 있다면 언제든 추가해주세요!

## 📄 라이선스

이 프로젝트는 학습 목적으로 제작되었습니다.
