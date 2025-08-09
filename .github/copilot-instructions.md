# Copilot Instructions for FFmpeg C/C++ Study Project

<!-- Use this file to provide workspace-specific custom instructions to Copilot. For more details, visit https://code.visualstudio.com/docs/copilot/copilot-customization#_use-a-githubcopilotinstructionsmd-file -->

This is a C/C++ project for learning FFmpeg library usage, including video analysis, encoding, and decoding.

## Project Context
- **Language**: C/C++
- **Primary Library**: FFmpeg (libavformat, libavcodec, libavutil, libswscale, libswresample)
- **Platform**: macOS with Homebrew-installed FFmpeg
- **Build System**: CMake
- **IDE**: Visual Studio Code

## Coding Guidelines
- Use modern C++ (C++17 or later) when possible
- Follow RAII principles for resource management
- Always check return values from FFmpeg functions
- Use av_free() and appropriate cleanup functions for FFmpeg resources
- Include proper error handling with descriptive messages
- Use const-correctness where applicable

## FFmpeg-specific Guidelines
- Always call av_register_all() and avformat_network_init() for initialization (if using older FFmpeg versions)
- Use avformat_open_input() and avformat_find_stream_info() for input file handling
- Properly manage AVFormatContext, AVCodecContext, and AVFrame lifecycle
- Use av_packet_alloc() and av_frame_alloc() for memory allocation
- Handle both audio and video streams appropriately
- Use av_err2str() for human-readable error messages

## Example Patterns
When creating examples, include:
- Proper header includes
- Error checking and cleanup
- Command-line argument parsing
- Clear, educational comments
- Memory leak prevention
