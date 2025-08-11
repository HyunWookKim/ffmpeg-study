#include <iostream>
#include <chrono>
#include <iomanip>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>

// 플랫폼별 하드웨어 가속 헤더 (Windows는 일단 제외)
#ifdef __APPLE__
#include <libavutil/hwcontext_videotoolbox.h>
#endif
}

// 하드웨어 픽셀 포맷 선택 함수
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
#ifdef __APPLE__
        if (*p == AV_PIX_FMT_VIDEOTOOLBOX) {
            return *p;
        }
#endif
    }
    
    std::cerr << "Failed to get HW surface format." << std::endl;
    return AV_PIX_FMT_NONE;
}

class HardwareAcceleratedDecoder {
private:
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVBufferRef* hw_device_ctx = nullptr;
    int video_stream_index = -1;
    
public:
    ~HardwareAcceleratedDecoder() {
        cleanup();
    }
    
    bool initialize_hardware_acceleration() {
#ifdef __APPLE__
        // M1 Mac의 VideoToolbox 하드웨어 가속 초기화
        int ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
        if (ret < 0) {
            print_error("Failed to create VideoToolbox device context", ret);
            return false;
        }
        std::cout << "[OK] VideoToolbox hardware acceleration initialized successfully!" << std::endl;
        return true;
#else
        // Windows 및 기타 플랫폼에서는 소프트웨어 디코딩 사용
        std::cout << "[WARN] Hardware acceleration not available on this platform, using software decoding" << std::endl;
        return false;
#endif
    }
    
    bool open_file(const char* filename) {
        // Open input file
        int ret = avformat_open_input(&format_ctx, filename, nullptr, nullptr);
        if (ret < 0) {
            print_error("Could not open input file", ret);
            return false;
        }
        
        // Find stream information
        ret = avformat_find_stream_info(format_ctx, nullptr);
        if (ret < 0) {
            print_error("Could not find stream information", ret);
            return false;
        }
        
        // Find video stream
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        
        if (video_stream_index == -1) {
            std::cerr << "Could not find video stream" << std::endl;
            return false;
        }
        
        // Get codec parameters
        AVCodecParameters* codecpar = format_ctx->streams[video_stream_index]->codecpar;
        
        // Find decoder that supports hardware acceleration
        const AVCodec* codec = nullptr;
        bool is_hardware_decoder = false;
        
        // 먼저 일반 디코더를 찾고, 하드웨어 가속은 별도로 설정
        // _by_name
        codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cerr << "Decoder not found for codec: " << avcodec_get_name(codecpar->codec_id) << std::endl;
            return false;
        }
        
        // VideoToolbox 하드웨어 가속 지원 코덱인지 확인
        if (codecpar->codec_id == AV_CODEC_ID_H264 || codecpar->codec_id == AV_CODEC_ID_HEVC) {
            // 하드웨어 가속이 가능한 코덱
            is_hardware_decoder = true;
            std::cout << "[INFO] Found " << avcodec_get_name(codecpar->codec_id) << " decoder with VideoToolbox support" << std::endl;
        } else {
            std::cout << "ℹ️  Using software decoder for codec: " << avcodec_get_name(codecpar->codec_id) << std::endl;
            is_hardware_decoder = false;
        }
        
        if (!codec) {
            std::cerr << "Unsupported codec" << std::endl;
            return false;
        }
        
        // Allocate codec context
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            std::cerr << "Could not allocate codec context" << std::endl;
            return false;
        }
        
        // Copy codec parameters to context
        ret = avcodec_parameters_to_context(codec_ctx, codecpar);
        if (ret < 0) {
            print_error("Could not copy codec parameters", ret);
            return false;
        }
        
        // Set hardware device context if using hardware decoder
        if (hw_device_ctx && is_hardware_decoder) {
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            codec_ctx->get_format = get_hw_format;
            std::cout << "🔧 Hardware device context attached to codec" << std::endl;
        }
        
        // Open codec
        ret = avcodec_open2(codec_ctx, codec, nullptr);
        if (ret < 0) {
            print_error("Could not open codec", ret);
            return false;
        }
        
        // 실제 하드웨어 가속 여부 확인
        if (is_hardware_decoder && codec_ctx->hw_device_ctx) {
            std::cout << "[OK] Hardware acceleration confirmed and active" << std::endl;
        }
        
        return true;
    }
    
    void benchmark_decoding(bool enable_loop = false) {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* sw_frame = av_frame_alloc(); // 하드웨어 프레임 전송용
        
        if (!packet || !frame || !sw_frame) {
            std::cerr << "Could not allocate packet or frame" << std::endl;
            return;
        }
        
        int frame_count = 0;
        int hw_frame_count = 0;
        int sw_frame_count = 0;
        int loop_count = 0;
        auto start_time = std::chrono::high_resolution_clock::now();
        auto frame_start_time = start_time;
        
        std::cout << "\n=== Hardware Accelerated Decoding Benchmark ===" << std::endl;
        std::cout << "Codec: " << codec_ctx->codec->name << std::endl;
        std::cout << "Hardware acceleration: " << (codec_ctx->hw_device_ctx ? "YES (VideoToolbox)" : "NO (Software)") << std::endl;
        std::cout << "Resolution: " << codec_ctx->width << "x" << codec_ctx->height << std::endl;
        std::cout << "Loop mode: " << (enable_loop ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // 최대 실행 시간 (루프 모드에서 무한 실행 방지)
        const auto max_duration = std::chrono::seconds(enable_loop ? 10 : 300);
        
        while (true) {
            int ret = av_read_frame(format_ctx, packet);
            
            // 파일 끝 처리 (GUI에서 개선한 루프 로직 적용)
            if (ret < 0) {
                if (ret == AVERROR_EOF && enable_loop) {
                    loop_count++;
                    std::cout << "🔄 Loop " << loop_count << ": 파일 끝 도달, 처음부터 다시 재생 (총 " << frame_count << " 프레임 처리)" << std::endl;
                    
                    // 디코더 상태 초기화
                    avcodec_flush_buffers(codec_ctx);
                    
                    // 파일 시작으로 seek
                    if (avformat_seek_file(format_ctx, video_stream_index, 0, 0, 0, AVSEEK_FLAG_FRAME) < 0) {
                        std::cerr << "❌ Seek 실패" << std::endl;
                        break;
                    }
                    
                    // 시간 제한 체크
                    auto current_time = std::chrono::high_resolution_clock::now();
                    if (current_time - start_time > max_duration) {
                        std::cout << "⏰ 시간 제한 도달, 벤치마크 종료" << std::endl;
                        break;
                    }
                    
                    continue;
                } else if (ret == AVERROR_EOF) {
                    std::cout << "📁 파일 끝 도달" << std::endl;
                    break;
                } else {
                    print_error("프레임 읽기 오류", ret);
                    break;
                }
            }
            
            if (packet->stream_index == video_stream_index) {
                ret = avcodec_send_packet(codec_ctx, packet);
                if (ret < 0) {
                    print_error("Error sending packet to decoder", ret);
                    break;
                }
                
                while (ret >= 0) {
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        print_error("Error during decoding", ret);
                        goto cleanup;
                    }
                    
                    frame_count++;
                    
                    // 프레임별 성능 측정
                    auto frame_end_time = std::chrono::high_resolution_clock::now();
                    auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_time - frame_start_time);
                    
                    // 하드웨어 프레임인지 확인
                    bool is_hw_frame = (frame->format == AV_PIX_FMT_VIDEOTOOLBOX);
                    
                    if (is_hw_frame) {
                        hw_frame_count++;
                        std::cout << "[HW] " << std::flush;  // 하드웨어 디코딩 표시
                        
                        // 하드웨어 프레임을 소프트웨어 메모리로 전송 데모
                        if (frame_count % 60 == 0) { // 60프레임마다 데모
                            if (av_hwframe_transfer_data(sw_frame, frame, 0) == 0) {
                                const char* sw_format_name = av_get_pix_fmt_name((AVPixelFormat)sw_frame->format);
                                std::cout << "\n� HW→SW 전송 성공: " << sw_format_name 
                                         << " (" << sw_frame->width << "x" << sw_frame->height << ")" << std::endl;
                            }
                        }
                    } else {
                        sw_frame_count++;
                        std::cout << "[SW] " << std::flush;  // 소프트웨어 디코딩 표시
                    }
                    
                    // 상세한 프레임 정보 (100프레임마다)
                    if (frame_count % 100 == 0) {
                        double avg_fps = frame_count * 1000000.0 / std::chrono::duration_cast<std::chrono::microseconds>(frame_end_time - start_time).count();
                        std::cout << "\n� Frame " << frame_count 
                                 << " | HW: " << hw_frame_count 
                                 << " | SW: " << sw_frame_count
                                 << " | 평균 FPS: " << std::fixed << std::setprecision(1) << avg_fps << std::endl;
                    }
                    
                    av_frame_unref(frame);
                    av_frame_unref(sw_frame);
                    frame_start_time = frame_end_time;
                }
            }
            av_packet_unref(packet);
        }
        
    cleanup:
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n\n=== Final Benchmark Results ===" << std::endl;
        std::cout << "Total frames processed: " << frame_count << std::endl;
        std::cout << "Hardware decoding: " << hw_frame_count << " frames" << std::endl;
        std::cout << "Software decoding: " << sw_frame_count << " frames" << std::endl;
        if (enable_loop) {
            std::cout << "Completed loops: " << loop_count << " times" << std::endl;
        }
        std::cout << "Total time: " << duration.count() << " ms" << std::endl;
        if (frame_count > 0 && duration.count() > 0) {
            double fps = (frame_count * 1000.0) / duration.count();
            double hw_percentage = (double)hw_frame_count / frame_count * 100.0;
            std::cout << "Average decoding speed: " << std::fixed << std::setprecision(2) << fps << " FPS" << std::endl;
            std::cout << "Hardware acceleration ratio: " << std::fixed << std::setprecision(1) << hw_percentage << "%" << std::endl;
        }
        std::cout << "=========================================" << std::endl;
        
        av_packet_free(&packet);
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
    }
    
private:
    void cleanup() {
        if (codec_ctx) {
            avcodec_free_context(&codec_ctx);
        }
        if (format_ctx) {
            avformat_close_input(&format_ctx);
        }
        if (hw_device_ctx) {
            av_buffer_unref(&hw_device_ctx);
        }
    }
    
    void print_error(const char* message, int error_code) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << message << ": " << error_buf << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cout << "[Apple] M1 Mac Hardware Accelerated Video Decoder" << std::endl;
        std::cout << "============================================" << std::endl;
        std::cout << "사용법: " << argv[0] << " <input_file> [loop]" << std::endl;
        std::cout << "\n예제:" << std::endl;
        std::cout << "  " << argv[0] << " media/samples/hevc_sample.mp4        # 단일 재생" << std::endl;
        std::cout << "  " << argv[0] << " media/samples/h264_sample.mp4 loop   # 루프 재생 (10초)" << std::endl;
        std::cout << "\n지원 코덱:" << std::endl;
        std::cout << "  [HW]  H.264, HEVC (VideoToolbox hardware acceleration)" << std::endl;
        std::cout << "  💻 기타 모든 코덱 (소프트웨어 디코딩)" << std::endl;
        return 1;
    }
    
    bool enable_loop = (argc == 3 && std::string(argv[2]) == "loop");
    
    std::cout << "[Apple] M1 Mac Hardware Accelerated Video Decoder" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "파일: " << argv[1] << std::endl;
    std::cout << "모드: " << (enable_loop ? "루프 재생 (10초)" : "단일 재생") << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    HardwareAcceleratedDecoder decoder;
    
    // Initialize hardware acceleration
    if (!decoder.initialize_hardware_acceleration()) {
        std::cerr << "[WARN] Hardware acceleration initialization failed, continuing with software decoding..." << std::endl;
    }
    
    // Open file and decode
    if (!decoder.open_file(argv[1])) {
        return 1;
    }
    
    decoder.benchmark_decoding(enable_loop);
    
    return 0;
}
