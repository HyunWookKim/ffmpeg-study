#include <iostream>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_videotoolbox.h>
#include <libavutil/pixdesc.h>
}

// 하드웨어 픽셀 포맷 선택 함수
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;
    
    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_VIDEOTOOLBOX) {
            return *p;
        }
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
        // M1 Mac의 VideoToolbox 하드웨어 가속 초기화
        int ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
        if (ret < 0) {
            print_error("Failed to create VideoToolbox device context", ret);
            return false;
        }
        
        std::cout << "✅ VideoToolbox hardware acceleration initialized successfully!" << std::endl;
        return true;
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
        codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cerr << "Decoder not found for codec: " << avcodec_get_name(codecpar->codec_id) << std::endl;
            return false;
        }
        
        // VideoToolbox 하드웨어 가속 지원 코덱인지 확인
        if (codecpar->codec_id == AV_CODEC_ID_H264 || codecpar->codec_id == AV_CODEC_ID_HEVC) {
            // 하드웨어 가속이 가능한 코덱
            is_hardware_decoder = true;
            std::cout << "🚀 Found " << avcodec_get_name(codecpar->codec_id) << " decoder with VideoToolbox support" << std::endl;
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
            std::cout << "✅ Hardware acceleration confirmed and active" << std::endl;
        }
        
        return true;
    }
    
    void benchmark_decoding() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        
        if (!packet || !frame) {
            std::cerr << "Could not allocate packet or frame" << std::endl;
            return;
        }
        
        int frame_count = 0;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "\n=== Hardware Accelerated Decoding Benchmark ===" << std::endl;
        std::cout << "Codec: " << codec_ctx->codec->name << std::endl;
        std::cout << "Hardware acceleration: " << (codec_ctx->hw_device_ctx ? "YES (VideoToolbox)" : "NO (Software)") << std::endl;
        std::cout << "Resolution: " << codec_ctx->width << "x" << codec_ctx->height << std::endl;
        
        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                int ret = avcodec_send_packet(codec_ctx, packet);
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
                    
                    // 하드웨어 프레임인지 확인
                    bool is_hw_frame = (frame->format == AV_PIX_FMT_VIDEOTOOLBOX);
                    
                    if (is_hw_frame) {
                        if (frame_count % 30 == 0) {
                            std::cout << "🖥️  Frame " << frame_count << " decoded using VideoToolbox hardware acceleration" << std::endl;
                        }
                    } else {
                        if (frame_count % 30 == 0) {
                            std::cout << "💻 Frame " << frame_count << " decoded using software" << std::endl;
                        }
                    }
                    
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(packet);
        }
        
    cleanup:
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n=== Benchmark Results ===" << std::endl;
        std::cout << "Total frames decoded: " << frame_count << std::endl;
        std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
        if (frame_count > 0 && duration.count() > 0) {
            double fps = (frame_count * 1000.0) / duration.count();
            std::cout << "Average decoding speed: " << fps << " FPS" << std::endl;
        }
        
        av_packet_free(&packet);
        av_frame_free(&frame);
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
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " media/samples/hevc_sample.mp4" << std::endl;
        return 1;
    }
    
    std::cout << "🍎 M1 Mac Hardware Accelerated Video Decoder" << std::endl;
    std::cout << "============================================" << std::endl;
    
    HardwareAcceleratedDecoder decoder;
    
    // Initialize hardware acceleration
    if (!decoder.initialize_hardware_acceleration()) {
        std::cerr << "Failed to initialize hardware acceleration, continuing with software decoding..." << std::endl;
    }
    
    // Open file and decode
    if (!decoder.open_file(argv[1])) {
        return 1;
    }
    
    decoder.benchmark_decoding();
    
    return 0;
}
