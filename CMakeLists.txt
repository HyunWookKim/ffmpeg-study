# =============================================================================
# FFmpeg 학습용 프로젝트 - CMake 설정 파일
# =============================================================================
# CMake는 C/C++ 프로젝트를 빌드하는 도구입니다.
# 이 파일은 프로젝트의 빌드 설정을 정의합니다.

# 최소 CMake 버전 요구사항 (3.20 이상)
# 버전이 낮으면 일부 기능이 동작하지 않을 수 있습니다
cmake_minimum_required(VERSION 3.20)

# 프로젝트 이름 설정
# 이 이름은 Visual Studio 솔루션 파일명 등에 사용됩니다
project(ffmpeg-study)

# C++ 표준 버전 설정 (C++17 사용)
# C++17은 현대적인 C++ 기능들을 제공합니다
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++17을 반드시 사용하도록 강제

# Windows 환경에서 한글 인코딩 문제 해결
# MSVC 컴파일러에서 UTF-8 인코딩을 사용하고 보안 경고를 비활성화
if(WIN32 AND MSVC)
    add_compile_options(/utf-8)                          # UTF-8 인코딩 사용
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)     # 보안 경고 비활성화
endif()

# =============================================================================
# 운영체제별 FFmpeg 라이브러리 설정
# =============================================================================
# FFmpeg는 비디오/오디오 처리를 위한 핵심 라이브러리입니다.
# 각 운영체제마다 설치 방법과 경로가 다르므로 분기 처리합니다.

# Windows 환경 설정
# 참고: WIN32는 이름과 달리 Windows 64비트도 포함합니다!
# CMake에서 WIN32 = 모든 Windows (32bit + 64bit), UNIX = Linux/macOS
if(WIN32)
    # FFmpeg가 프로젝트 폴더에 압축 해제되어 있다고 가정
    # 상대 경로를 사용하여 FFmpeg 라이브러리 위치 지정
    set(FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-master-latest-win64-gpl-shared")
    set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")  # 헤더 파일 경로
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")          # 라이브러리 파일 경로
    
    # 현재 빌드가 64비트인지 32비트인지 확인하고 표시
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "🏗️  Windows 64비트 빌드 환경 감지")
        # 64비트용 라이브러리 경로 (필요시 별도 설정 가능)
        # set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib/x64")
    else()
        message(STATUS "🏗️  Windows 32비트 빌드 환경 감지")
        # 32비트용 라이브러리 경로 (필요시 별도 설정 가능)
        # set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib/x86")
    endif()
    
    # 컴파일러가 헤더 파일을 찾을 수 있도록 경로 추가
    include_directories(${FFMPEG_INCLUDE_DIR})
    # 링커가 라이브러리 파일을 찾을 수 있도록 경로 추가
    link_directories(${FFMPEG_LIB_DIR})
    
    # Windows용 FFmpeg 라이브러리 목록
    # 각 라이브러리는 특정 기능을 담당합니다:
    # - avformat: 파일 형식 처리 (MP4, AVI 등)
    # - avcodec: 코덱 처리 (H.264, HEVC 등)
    # - avutil: 유틸리티 함수들
    # - swscale: 이미지 크기 변환
    # - swresample: 오디오 리샘플링
    # - avfilter: 비디오/오디오 필터
    set(FFMPEG_LIBRARIES
        avformat
        avcodec
        avutil
        swscale
        swresample
        avfilter
    )
    
    # 사용자에게 FFmpeg 경로 정보를 표시
    message(STATUS "Using FFmpeg from: ${FFMPEG_ROOT}")

# macOS ARM64 (Apple Silicon) 환경 설정
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    # Homebrew ARM64 설치 경로 (/opt/homebrew)를 CMake 탐색 경로에 추가
    set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})
    include_directories("/opt/homebrew/include")
    link_directories("/opt/homebrew/lib")
    
    # pkg-config를 사용하여 FFmpeg 라이브러리 자동 탐지
    # pkg-config는 라이브러리의 컴파일/링크 플래그를 자동으로 찾아줍니다
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED 
        libavformat    # 파일 형식 처리
        libavcodec     # 코덱 처리
        libavutil      # 유틸리티
        libswscale     # 이미지 변환
        libswresample  # 오디오 리샘플링
        libavfilter    # 필터
    )
    
    # FFmpeg 라이브러리 목록을 변수에 저장
    set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES})
    
# Linux 또는 기타 UNIX 계열 시스템 설정
else()
    # pkg-config를 사용하여 시스템에 설치된 FFmpeg 라이브러리 탐지
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED 
        libavformat    # 파일 형식 처리
        libavcodec     # 코덱 처리  
        libavutil      # 유틸리티
        libswscale     # 이미지 변환
        libswresample  # 오디오 리샘플링
        libavfilter    # 필터
    )
    
    # FFmpeg 라이브러리 목록을 변수에 저장
    set(FFMPEG_LIBRARIES ${FFMPEG_LIBRARIES})
endif()

# =============================================================================
# SDL2 라이브러리 설정 (GUI 비디오 플레이어용, 선택사항)
# =============================================================================
# SDL2는 멀티미디어 GUI 애플리케이션 개발을 위한 라이브러리입니다.
# 비디오 창을 띄우고 키보드/마우스 이벤트를 처리할 수 있습니다.

# Windows 환경에서 SDL2 설정
if(WIN32)
    # SDL2가 프로젝트 폴더에 압축 해제되어 있다고 가정
    set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/SDL2-2.28.5")
    set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")  # SDL2 헤더 파일 경로
    set(SDL2_LIB_DIR "${SDL2_ROOT}/lib/x64")      # SDL2 라이브러리 파일 경로 (64비트)
    
    # SDL2 디렉토리가 존재하는지 확인
    if(EXISTS ${SDL2_INCLUDE_DIR})
        set(SDL2_FOUND TRUE)                        # SDL2를 찾았다고 표시
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})   # 헤더 파일 경로 저장
        set(SDL2_LIBRARIES SDL2 SDL2main)           # 링크할 라이브러리 목록
        include_directories(${SDL2_INCLUDE_DIRS})    # 컴파일러에 헤더 경로 알림
        link_directories(${SDL2_LIB_DIR})           # 링커에 라이브러리 경로 알림
        message(STATUS "✅ Windows SDL2 found: ${SDL2_ROOT}")
    else()
        set(SDL2_FOUND FALSE)  # SDL2를 찾지 못했다고 표시
        message(STATUS "⚠️  SDL2 not found. Download from https://libsdl.org/")
    endif()
    
# macOS/Linux 환경에서 SDL2 설정
else()
    # 시스템에 설치된 SDL2를 자동으로 찾기 시도
    find_package(SDL2 QUIET)  # QUIET: 찾지 못해도 오류 메시지 출력 안함
endif()

# =============================================================================
# 헤더 파일 경로 설정
# =============================================================================
# 컴파일러가 #include 문으로 헤더 파일을 찾을 수 있도록 경로를 지정합니다.

# Windows가 아닌 시스템에서만 실행 (Windows는 이미 위에서 설정함)
if(NOT WIN32)
    # FFmpeg 헤더 파일 경로 추가
    include_directories(${FFMPEG_INCLUDE_DIRS})
    
    # SDL2가 발견된 경우 SDL2 헤더 파일 경로도 추가
    if(SDL2_FOUND)
        include_directories(${SDL2_INCLUDE_DIRS})
    endif()
endif()

# =============================================================================
# 실행 파일 생성 및 라이브러리 링크 설정
# =============================================================================
# add_executable(): 소스 파일을 컴파일하여 실행 파일을 만듭니다.
# target_link_libraries(): 실행 파일에 라이브러리를 링크합니다.

# 1. FFmpeg 정보 조회 도구
# 비디오 파일의 메타데이터(해상도, 코덱, 비트레이트 등)를 분석하는 도구
add_executable(ffmpeg-info src/ffmpeg_info.cpp)
target_link_libraries(ffmpeg-info ${FFMPEG_LIBRARIES})  # FFmpeg 라이브러리와 링크
if(NOT WIN32)
    # Windows가 아닌 경우 추가 컴파일 옵션 적용
    target_compile_options(ffmpeg-info PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 2. 비디오 분석 예제
# 비디오의 프레임별 상세 정보를 분석하는 도구
add_executable(video-analysis examples/video_analysis.cpp)
target_link_libraries(video-analysis ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(video-analysis PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 3. 프레임 추출 예제  
# 비디오에서 특정 간격으로 프레임을 이미지 파일로 저장하는 도구
add_executable(frame-extract examples/frame_extraction.cpp)
target_link_libraries(frame-extract ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(frame-extract PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 4. 간단한 인코더 예제
# 프로그래밍 방식으로 비디오를 생성하고 인코딩하는 도구
add_executable(simple-encoder examples/simple_encoder.cpp)
target_link_libraries(simple-encoder ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(simple-encoder PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# =============================================================================
# 고급 예제들 (Advanced Examples)
# =============================================================================
# 더 복잡한 FFmpeg 기능들을 다루는 예제들입니다.

# 5. 하드웨어 가속 디코더 (Apple Silicon VideoToolbox 지원)
# GPU를 활용한 고속 비디오 디코딩 도구
add_executable(hardware-decoder examples/advanced/hardware_decoder.cpp)
target_link_libraries(hardware-decoder ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(hardware-decoder PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 6. 고급 비디오 필터 프로세서
# 블러, 엣지 검출, 크기 조정 등 다양한 필터를 적용하는 도구
add_executable(video-filter examples/advanced/video_filter.cpp)
target_link_libraries(video-filter ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(video-filter PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 7. RTMP 라이브 스트리머
# 실시간으로 비디오를 인터넷으로 송출하는 도구 (YouTube Live, Twitch 등)
add_executable(rtmp-streamer examples/advanced/rtmp_streamer.cpp)
target_link_libraries(rtmp-streamer ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(rtmp-streamer PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 8. 하드웨어 가속 비디오 플레이어 (콘솔)
# 터미널에서 실행되는 고성능 비디오 재생기
add_executable(video-player examples/advanced/video_player.cpp)
target_link_libraries(video-player ${FFMPEG_LIBRARIES})
if(NOT WIN32)
    target_compile_options(video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})
endif()

# 9. GUI 비디오 플레이어 (SDL2 기반, 조건부 빌드)
# 창이 있는 그래픽 환경에서 실행되는 비디오 플레이어
# SDL2 라이브러리가 설치된 경우에만 빌드됩니다.
if(SDL2_FOUND)
    add_executable(gui-video-player examples/advanced/gui_video_player.cpp)
    # FFmpeg과 SDL2 라이브러리 모두 링크
    target_link_libraries(gui-video-player ${FFMPEG_LIBRARIES} ${SDL2_LIBRARIES})
    if(NOT WIN32)
        # Windows가 아닌 경우 추가 컴파일 옵션 적용
        target_compile_options(gui-video-player PRIVATE ${FFMPEG_CFLAGS_OTHER} ${SDL2_CFLAGS_OTHER})
    endif()
    message(STATUS "✅ GUI 비디오 플레이어가 빌드에 포함됩니다 (SDL2 발견)")
else()
    # SDL2가 없는 경우 사용자에게 안내 메시지 출력
    message(STATUS "⚠️  SDL2를 찾을 수 없어 GUI 플레이어가 제외됩니다. 'brew install sdl2'로 설치하세요.")
endif()

# =============================================================================
# 컴파일러 디버깅 설정
# =============================================================================
# 디버깅을 위한 컴파일러 플래그를 설정합니다.
# Debug 모드에서는 최적화를 끄고 디버그 정보를 포함시킵니다.

# 빌드 타입을 Debug로 설정 (개발 중에는 디버그 모드 사용)
set(CMAKE_BUILD_TYPE Debug)

# 컴파일러별 디버그 플래그 설정
if(MSVC)
    # Microsoft Visual C++ 컴파일러용 설정
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")  # /Zi: 디버그 정보 생성, /Od: 최적화 비활성화
    # UTF-8 문자 인코딩 설정 (Windows 한국어 환경에서 한글 주석 지원)
    add_compile_options(/utf-8)
else()
    # GCC/Clang 컴파일러용 설정 (Linux/macOS)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")   # -g: 디버그 정보 생성, -O0: 최적화 비활성화
endif()
