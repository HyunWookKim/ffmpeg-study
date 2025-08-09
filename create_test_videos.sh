#!/bin/bash

# 테스트 비디오 생성 스크립트
# M1 Mac에서 지원하는 다양한 코덱 테스트

echo "=== Creating test videos with various formats ==="

# 기본 디렉토리 생성
mkdir -p media/samples

# 1. H.264 (기본)
echo "Creating H.264 video..."
./build/simple-encoder 640 480 60 media/samples/h264_sample.mp4

# 2. 고해상도 H.264
echo "Creating HD H.264 video..."
./build/simple-encoder 1280 720 120 media/samples/hd_h264_sample.mp4

# 3. FFmpeg로 다른 포맷들 생성
echo "Creating various format videos using FFmpeg..."

# H.265/HEVC (Apple Silicon 하드웨어 가속 지원)
ffmpeg -f lavfi -i testsrc2=duration=3:size=640x480:rate=30 -c:v libx265 -preset fast -y media/samples/hevc_sample.mp4

# ProRes (Apple 네이티브 코덱)
ffmpeg -f lavfi -i testsrc2=duration=3:size=640x480:rate=30 -c:v prores_ks -profile:v 2 -y media/samples/prores_sample.mov

# VP9 (WebM)
ffmpeg -f lavfi -i testsrc2=duration=3:size=640x480:rate=30 -c:v libvpx-vp9 -b:v 1M -y media/samples/vp9_sample.webm

# AV1 (차세대 코덱)
ffmpeg -f lavfi -i testsrc2=duration=3:size=640x480:rate=30 -c:v libaom-av1 -b:v 1M -y media/samples/av1_sample.mp4

# 오디오 포함 버전
ffmpeg -f lavfi -i testsrc2=duration=3:size=640x480:rate=30 -f lavfi -i sine=frequency=440:duration=3 -c:v libx264 -c:a aac -y media/samples/audio_video_sample.mp4

# 다양한 해상도
ffmpeg -f lavfi -i testsrc2=duration=2:size=320x240:rate=15 -c:v libx264 -y media/samples/low_res_sample.mp4
ffmpeg -f lavfi -i testsrc2=duration=2:size=1920x1080:rate=60 -c:v libx264 -b:v 5M -y media/samples/full_hd_sample.mp4

echo "=== Test videos created successfully! ==="
echo "Available test files:"
ls -la media/samples/
