#include <iostream>
#include <iomanip>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

class VideoAnalyzer {
private:
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    int video_stream_index = -1;
    
public:
    ~VideoAnalyzer() {
        cleanup();
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
        
        // Find decoder
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cerr << "Unsupported codec" << std::endl;
            return false;
        }
        
        // Allocate codec context
        video_codec_ctx = avcodec_alloc_context3(codec);
        if (!video_codec_ctx) {
            std::cerr << "Could not allocate codec context" << std::endl;
            return false;
        }
        
        // Copy codec parameters to context
        ret = avcodec_parameters_to_context(video_codec_ctx, codecpar);
        if (ret < 0) {
            print_error("Could not copy codec parameters", ret);
            return false;
        }
        
        // Open codec
        ret = avcodec_open2(video_codec_ctx, codec, nullptr);
        if (ret < 0) {
            print_error("Could not open codec", ret);
            return false;
        }
        
        return true;
    }
    
    void analyze_frames() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        
        if (!packet || !frame) {
            std::cerr << "Could not allocate packet or frame" << std::endl;
            return;
        }
        
        int frame_count = 0;
        int64_t total_size = 0;
        double total_duration = 0.0;
        
        std::cout << "=== Frame Analysis ===" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        
        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                int ret = avcodec_send_packet(video_codec_ctx, packet);
                if (ret < 0) {
                    print_error("Error sending packet to decoder", ret);
                    break;
                }
                
                while (ret >= 0) {
                    ret = avcodec_receive_frame(video_codec_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        print_error("Error during decoding", ret);
                        goto cleanup;
                    }
                    
                    frame_count++;
                    total_size += packet->size;
                    
                    // Calculate timestamp
                    double timestamp = frame->pts * av_q2d(format_ctx->streams[video_stream_index]->time_base);
                    total_duration = timestamp;
                    
                    // Print frame info (every 30th frame to avoid spam)
                    if (frame_count % 30 == 0) {
                        std::cout << "Frame " << std::setw(6) << frame_count 
                                 << " | Time: " << std::setw(8) << timestamp << "s"
                                 << " | Type: " << (frame->pict_type == AV_PICTURE_TYPE_I ? "I" : 
                                                   frame->pict_type == AV_PICTURE_TYPE_P ? "P" : 
                                                   frame->pict_type == AV_PICTURE_TYPE_B ? "B" : "?")
                                 << " | Size: " << std::setw(8) << packet->size << " bytes" << std::endl;
                    }
                    
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(packet);
        }
        
    cleanup:
        std::cout << "\n=== Analysis Summary ===" << std::endl;
        std::cout << "Total frames: " << frame_count << std::endl;
        std::cout << "Total duration: " << total_duration << " seconds" << std::endl;
        if (total_duration > 0) {
            std::cout << "Average FPS: " << frame_count / total_duration << std::endl;
        }
        std::cout << "Total video data: " << total_size << " bytes" << std::endl;
        if (frame_count > 0) {
            std::cout << "Average frame size: " << total_size / frame_count << " bytes" << std::endl;
        }
        
        av_packet_free(&packet);
        av_frame_free(&frame);
    }
    
private:
    void cleanup() {
        if (video_codec_ctx) {
            avcodec_free_context(&video_codec_ctx);
        }
        if (format_ctx) {
            avformat_close_input(&format_ctx);
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
        return 1;
    }
    
    VideoAnalyzer analyzer;
    
    if (!analyzer.open_file(argv[1])) {
        return 1;
    }
    
    analyzer.analyze_frames();
    
    return 0;
}
