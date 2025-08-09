cmake_minimum_required(VERSION 3.20)
project(ffmpeg-study)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apple Silicon Mac specific settings
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    # Homebrew ARM64 path for Apple Silicon
    set(CMAKE_PREFIX_PATH "/opt/homebrew" ${CMAKE_PREFIX_PATH})
    include_directories("/opt/homebrew/include")
    link_directories("/opt/homebrew/lib")
endif()

# Find FFmpeg libraries using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavformat 
    libavcodec 
    libavutil 
    libswscale 
    libswresample
    libavfilter
)

# Include directories
include_directories(${FFMPEG_INCLUDE_DIRS})

# Create main executable
add_executable(ffmpeg-info src/ffmpeg_info.cpp)
target_link_libraries(ffmpeg-info ${FFMPEG_LIBRARIES})
target_compile_options(ffmpeg-info PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Create video analysis example
add_executable(video-analysis examples/video_analysis.cpp)
target_link_libraries(video-analysis ${FFMPEG_LIBRARIES})
target_compile_options(video-analysis PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Create frame extraction example
add_executable(frame-extract examples/frame_extraction.cpp)
target_link_libraries(frame-extract ${FFMPEG_LIBRARIES})
target_compile_options(frame-extract PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Create simple encoder example
add_executable(simple-encoder examples/simple_encoder.cpp)
target_link_libraries(simple-encoder ${FFMPEG_LIBRARIES})
target_compile_options(simple-encoder PRIVATE ${FFMPEG_CFLAGS_OTHER})

# === Advanced Examples ===

# Hardware accelerated decoder (M1 Mac VideoToolbox)
add_executable(hardware-decoder examples/advanced/hardware_decoder.cpp)
target_link_libraries(hardware-decoder ${FFMPEG_LIBRARIES})
target_compile_options(hardware-decoder PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Advanced video filter processor
add_executable(video-filter examples/advanced/video_filter.cpp)
target_link_libraries(video-filter ${FFMPEG_LIBRARIES})
target_compile_options(video-filter PRIVATE ${FFMPEG_CFLAGS_OTHER})

# RTMP live streamer
add_executable(rtmp-streamer examples/advanced/rtmp_streamer.cpp)
target_link_libraries(rtmp-streamer ${FFMPEG_LIBRARIES})
target_compile_options(rtmp-streamer PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Real-time video processor with threading
add_executable(realtime-processor examples/advanced/realtime_processor.cpp)
target_link_libraries(realtime-processor ${FFMPEG_LIBRARIES})
target_compile_options(realtime-processor PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Hardware accelerated video player
add_executable(video-player examples/advanced/video_player.cpp)
target_link_libraries(video-player ${FFMPEG_LIBRARIES})
target_compile_options(video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})

# Set compiler flags for better debugging
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
