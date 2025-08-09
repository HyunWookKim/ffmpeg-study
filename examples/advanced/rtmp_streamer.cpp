#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <signal.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class RTMPStreamer {
private:
    AVFormatContext* input_fmt_ctx = nullptr;
    AVFormatContext* output_fmt_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    int video_stream_index = -1;
    std::atomic<bool> should_stop{false};
    
public:
    ~RTMPStreamer() {
        cleanup();
    }
    
    bool setup_input(const char* input_source, bool is_webcam = false) {
        const AVInputFormat* input_format = nullptr;
        AVDictionary* options = nullptr;
        
        if (is_webcam) {
            // macOS에서 웹캠 사용 (AVFoundation)
            input_format = av_find_input_format("avfoundation");
            av_dict_set(&options, "video_size", "1280x720", 0);
            av_dict_set(&options, "framerate", "30", 0);
            av_dict_set(&options, "pixel_format", "uyvy422", 0);
        }
        
        int ret = avformat_open_input(&input_fmt_ctx, input_source, input_format, &options);
        if (ret < 0) {
            print_error("Could not open input", ret);
            av_dict_free(&options);
            return false;
        }
        av_dict_free(&options);
        
        ret = avformat_find_stream_info(input_fmt_ctx, nullptr);
        if (ret < 0) {
            print_error("Could not find stream information", ret);
            return false;
        }
        
        // Find video stream
        for (unsigned int i = 0; i < input_fmt_ctx->nb_streams; i++) {
            if (input_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        
        if (video_stream_index == -1) {
            std::cerr << "Could not find video stream" << std::endl;
            return false;
        }
        
        // Setup decoder
        AVCodecParameters* codecpar = input_fmt_ctx->streams[video_stream_index]->codecpar;
        const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
        if (!decoder) {
            std::cerr << "Decoder not found" << std::endl;
            return false;
        }
        
        decoder_ctx = avcodec_alloc_context3(decoder);
        if (!decoder_ctx) {
            std::cerr << "Could not allocate decoder context" << std::endl;
            return false;
        }
        
        ret = avcodec_parameters_to_context(decoder_ctx, codecpar);
        if (ret < 0) {
            print_error("Could not copy codec parameters", ret);
            return false;
        }
        
        ret = avcodec_open2(decoder_ctx, decoder, nullptr);
        if (ret < 0) {
            print_error("Could not open decoder", ret);
            return false;
        }
        
        std::cout << "📹 Input setup complete:" << std::endl;
        std::cout << "   Resolution: " << decoder_ctx->width << "x" << decoder_ctx->height << std::endl;
        std::cout << "   Codec: " << avcodec_get_name(decoder_ctx->codec_id) << std::endl;
        
        return true;
    }
    
    bool setup_rtmp_output(const char* rtmp_url, int bitrate = 2500000) {
        int ret = avformat_alloc_output_context2(&output_fmt_ctx, nullptr, "flv", rtmp_url);
        if (ret < 0) {
            print_error("Could not create output context", ret);
            return false;
        }
        
        // Create video stream
        AVStream* out_stream = avformat_new_stream(output_fmt_ctx, nullptr);
        if (!out_stream) {
            std::cerr << "Could not create output stream" << std::endl;
            return false;
        }
        
        // Setup encoder (H.264 for RTMP)
        const AVCodec* encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder) {
            std::cerr << "H.264 encoder not found" << std::endl;
            return false;
        }
        
        encoder_ctx = avcodec_alloc_context3(encoder);
        if (!encoder_ctx) {
            std::cerr << "Could not allocate encoder context" << std::endl;
            return false;
        }
        
        // Set encoder parameters for streaming
        encoder_ctx->codec_id = AV_CODEC_ID_H264;
        encoder_ctx->bit_rate = bitrate;
        encoder_ctx->width = decoder_ctx->width;
        encoder_ctx->height = decoder_ctx->height;
        encoder_ctx->time_base = {1, 30}; // 30 FPS
        encoder_ctx->framerate = {30, 1};
        encoder_ctx->gop_size = 60; // GOP size for streaming
        encoder_ctx->max_b_frames = 0; // No B-frames for low latency
        encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        
        // Streaming optimizations
        av_opt_set(encoder_ctx->priv_data, "preset", "veryfast", 0);
        av_opt_set(encoder_ctx->priv_data, "tune", "zerolatency", 0);
        av_opt_set(encoder_ctx->priv_data, "profile", "baseline", 0);
        
        if (output_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            encoder_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        
        ret = avcodec_open2(encoder_ctx, encoder, nullptr);
        if (ret < 0) {
            print_error("Could not open encoder", ret);
            return false;
        }
        
        ret = avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);
        if (ret < 0) {
            print_error("Could not copy encoder parameters", ret);
            return false;
        }
        
        out_stream->time_base = encoder_ctx->time_base;
        
        // Open RTMP connection
        ret = avio_open(&output_fmt_ctx->pb, rtmp_url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            print_error("Could not open RTMP URL", ret);
            return false;
        }
        
        ret = avformat_write_header(output_fmt_ctx, nullptr);
        if (ret < 0) {
            print_error("Error writing header", ret);
            return false;
        }
        
        std::cout << "📡 RTMP output setup complete:" << std::endl;
        std::cout << "   URL: " << rtmp_url << std::endl;
        std::cout << "   Bitrate: " << bitrate / 1000 << " kbps" << std::endl;
        std::cout << "   Encoder: " << encoder->name << std::endl;
        
        return true;
    }
    
    void start_streaming() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVPacket* out_packet = av_packet_alloc();
        SwsContext* sws_ctx = nullptr;
        AVFrame* scaled_frame = nullptr;
        
        if (!packet || !frame || !out_packet) {
            std::cerr << "Could not allocate packets or frames" << std::endl;
            return;
        }
        
        int64_t frame_count = 0;
        int64_t start_time = av_gettime();
        
        // Setup scaling if needed
        if (decoder_ctx->pix_fmt != AV_PIX_FMT_YUV420P) {
            sws_ctx = sws_getContext(
                decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
                encoder_ctx->width, encoder_ctx->height, encoder_ctx->pix_fmt,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            
            if (!sws_ctx) {
                std::cerr << "Could not initialize scaling context" << std::endl;
                goto cleanup;
            }
            
            scaled_frame = av_frame_alloc();
            if (!scaled_frame) {
                std::cerr << "Could not allocate scaled frame" << std::endl;
                goto cleanup;
            }
            
            scaled_frame->format = encoder_ctx->pix_fmt;
            scaled_frame->width = encoder_ctx->width;
            scaled_frame->height = encoder_ctx->height;
            
            int ret = av_frame_get_buffer(scaled_frame, 0);
            if (ret < 0) {
                print_error("Could not allocate scaled frame buffer", ret);
                goto cleanup;
            }
        }
        
        std::cout << "\n🔴 Starting live stream..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        while (!should_stop && av_read_frame(input_fmt_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                // Decode
                int ret = avcodec_send_packet(decoder_ctx, packet);
                if (ret < 0) {
                    print_error("Error sending packet to decoder", ret);
                    break;
                }
                
                while (ret >= 0) {
                    ret = avcodec_receive_frame(decoder_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        print_error("Error during decoding", ret);
                        goto cleanup;
                    }
                    
                    AVFrame* encode_frame = frame;
                    
                    // Scale if needed
                    if (sws_ctx) {
                        sws_scale(sws_ctx, frame->data, frame->linesize, 0,
                                decoder_ctx->height, scaled_frame->data, scaled_frame->linesize);
                        scaled_frame->pts = frame->pts;
                        encode_frame = scaled_frame;
                    }
                    
                    // Set proper timestamp
                    encode_frame->pts = frame_count;
                    
                    // Encode
                    ret = avcodec_send_frame(encoder_ctx, encode_frame);
                    if (ret < 0) {
                        print_error("Error sending frame to encoder", ret);
                        goto cleanup;
                    }
                    
                    while (ret >= 0) {
                        ret = avcodec_receive_packet(encoder_ctx, out_packet);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        } else if (ret < 0) {
                            print_error("Error during encoding", ret);
                            goto cleanup;
                        }
                        
                        // Timestamp for streaming
                        av_packet_rescale_ts(out_packet, encoder_ctx->time_base,
                                           output_fmt_ctx->streams[0]->time_base);
                        out_packet->stream_index = 0;
                        
                        // Send to RTMP server
                        ret = av_interleaved_write_frame(output_fmt_ctx, out_packet);
                        if (ret < 0) {
                            print_error("Error writing packet to stream", ret);
                            goto cleanup;
                        }
                        
                        av_packet_unref(out_packet);
                    }
                    
                    frame_count++;
                    
                    // Print statistics every 30 frames
                    if (frame_count % 30 == 0) {
                        int64_t current_time = av_gettime();
                        double fps = frame_count * 1000000.0 / (current_time - start_time);
                        std::cout << "📊 Streaming: " << frame_count 
                                 << " frames, FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
                    }
                    
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(packet);
            
            // Frame rate control for file input
            if (strcmp(input_fmt_ctx->iformat->name, "avfoundation") != 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
            }
        }
        
        av_write_trailer(output_fmt_ctx);
        std::cout << "\n✅ Streaming stopped. Total frames: " << frame_count << std::endl;
        
    cleanup:
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (scaled_frame) av_frame_free(&scaled_frame);
        av_packet_free(&packet);
        av_frame_free(&frame);
        av_packet_free(&out_packet);
    }
    
    void stop() {
        should_stop = true;
    }
    
private:
    void cleanup() {
        if (decoder_ctx) avcodec_free_context(&decoder_ctx);
        if (encoder_ctx) avcodec_free_context(&encoder_ctx);
        if (input_fmt_ctx) avformat_close_input(&input_fmt_ctx);
        if (output_fmt_ctx) {
            if (output_fmt_ctx->pb) avio_closep(&output_fmt_ctx->pb);
            avformat_free_context(output_fmt_ctx);
        }
    }
    
    void print_error(const char* message, int error_code) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << message << ": " << error_buf << std::endl;
    }
};

// RTMP 테스트 서버 클래스
class RTMPTestServer {
public:
    static void start_test_server() {
        std::cout << "\n🔧 Setting up RTMP test server..." << std::endl;
        std::cout << "Installing nginx with rtmp module via Homebrew:" << std::endl;
        std::cout << "  brew install nginx-full --with-rtmp-module" << std::endl;
        std::cout << "\nOr use Docker:" << std::endl;
        std::cout << "  docker run -d -p 1935:1935 tiangolo/nginx-rtmp" << std::endl;
        std::cout << "\nAlternatively, use OBS Studio or YouTube Live:" << std::endl;
        std::cout << "  YouTube: rtmp://a.rtmp.youtube.com/live2/YOUR_STREAM_KEY" << std::endl;
        std::cout << "  Twitch: rtmp://live.twitch.tv/live/YOUR_STREAM_KEY" << std::endl;
    }
};

void print_usage(const char* program_name) {
    std::cout << "Advanced RTMP Live Streamer" << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Usage: " << program_name << " <mode> [options]" << std::endl << std::endl;
    
    std::cout << "Modes:" << std::endl;
    std::cout << "  webcam <rtmp_url>           - Stream from webcam" << std::endl;
    std::cout << "  file <input_file> <rtmp_url> - Stream from video file" << std::endl;
    std::cout << "  test-server                 - Show RTMP server setup instructions" << std::endl << std::endl;
    
    std::cout << "Examples:" << std::endl;
    std::cout << "  # Stream webcam to local RTMP server" << std::endl;
    std::cout << "  " << program_name << " webcam rtmp://localhost/live/test" << std::endl << std::endl;
    
    std::cout << "  # Stream video file to YouTube Live" << std::endl;
    std::cout << "  " << program_name << " file video.mp4 rtmp://a.rtmp.youtube.com/live2/YOUR_KEY" << std::endl << std::endl;
    
    std::cout << "  # Setup test server" << std::endl;
    std::cout << "  " << program_name << " test-server" << std::endl << std::endl;
    
    std::cout << "Note: For webcam streaming on macOS, make sure to grant camera permission." << std::endl;
}

#include <iomanip>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "test-server") {
        RTMPTestServer::start_test_server();
        return 0;
    }
    
    if ((mode == "webcam" && argc < 3) || (mode == "file" && argc < 4)) {
        print_usage(argv[0]);
        return 1;
    }
    
    RTMPStreamer streamer;
    
    if (mode == "webcam") {
        const char* rtmp_url = argv[2];
        std::cout << "🎥 Starting webcam streaming to: " << rtmp_url << std::endl;
        
        // macOS 웹캠 디바이스 (기본 카메라는 "0")
        if (!streamer.setup_input("0", true)) {
            std::cerr << "❌ Failed to setup webcam input" << std::endl;
            std::cerr << "💡 Make sure camera permission is granted and no other app is using the camera" << std::endl;
            return 1;
        }
        
        if (!streamer.setup_rtmp_output(rtmp_url)) {
            std::cerr << "❌ Failed to setup RTMP output" << std::endl;
            return 1;
        }
        
    } else if (mode == "file") {
        const char* input_file = argv[2];
        const char* rtmp_url = argv[3];
        
        std::cout << "📁 Starting file streaming:" << std::endl;
        std::cout << "   Input: " << input_file << std::endl;
        std::cout << "   Output: " << rtmp_url << std::endl;
        
        if (!streamer.setup_input(input_file, false)) {
            std::cerr << "❌ Failed to setup file input" << std::endl;
            return 1;
        }
        
        if (!streamer.setup_rtmp_output(rtmp_url)) {
            std::cerr << "❌ Failed to setup RTMP output" << std::endl;
            return 1;
        }
    } else {
        std::cerr << "❌ Unknown mode: " << mode << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    // Signal handling for graceful shutdown
    signal(SIGINT, [](int) {
        std::cout << "\n🛑 Stopping stream..." << std::endl;
        exit(0);
    });
    
    streamer.start_streaming();
    
    return 0;
}
