#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// Function to save frame as PPM (simple image format)
void save_frame_as_ppm(AVFrame* frame, int width, int height, int frame_number) {
    std::ostringstream filename;
    filename << "frame_" << std::setfill('0') << std::setw(6) << frame_number << ".ppm";
    
    FILE* file = fopen(filename.str().c_str(), "wb");
    if (!file) {
        std::cerr << "Could not open file " << filename.str() << std::endl;
        return;
    }
    
    // Write PPM header
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    
    // Write pixel data
    for (int y = 0; y < height; y++) {
        fwrite(frame->data[0] + y * frame->linesize[0], 1, width * 3, file);
    }
    
    fclose(file);
    std::cout << "Saved frame " << frame_number << " as " << filename.str() << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <frame_interval>" << std::endl;
        std::cerr << "Example: " << argv[0] << " video.mp4 30" << std::endl;
        std::cerr << "This will extract every 30th frame" << std::endl;
        return 1;
    }
    
    const char* input_filename = argv[1];
    int frame_interval = std::atoi(argv[2]);
    
    if (frame_interval <= 0) {
        std::cerr << "Frame interval must be positive" << std::endl;
        return 1;
    }
    
    // Initialize variables at the beginning
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgb_frame = nullptr;
    uint8_t* rgb_buffer = nullptr;
    int video_stream_index = -1;
    int frame_count = 0;
    int saved_count = 0;
    int ret = 0;
    
    // Open input file
    ret = avformat_open_input(&format_ctx, input_filename, nullptr, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Error opening file: " << error_buf << std::endl;
        return 1;
    }
    
    // Find stream information
    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Could not find stream information" << std::endl;
        goto cleanup;
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
        goto cleanup;
    }
    
    // Get codec parameters and setup decoder
    {
        AVCodecParameters* codecpar = format_ctx->streams[video_stream_index]->codecpar;
        
        // Find decoder
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cerr << "Unsupported codec" << std::endl;
            goto cleanup;
        }
        
        // Allocate codec context
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            std::cerr << "Could not allocate codec context" << std::endl;
            goto cleanup;
        }
        
        // Copy codec parameters to context
        ret = avcodec_parameters_to_context(codec_ctx, codecpar);
        if (ret < 0) {
            std::cerr << "Could not copy codec parameters" << std::endl;
            goto cleanup;
        }
        
        // Open codec
        ret = avcodec_open2(codec_ctx, codec, nullptr);
        if (ret < 0) {
            std::cerr << "Could not open codec" << std::endl;
            goto cleanup;
        }
    }
    
    // Initialize scaling context for RGB conversion
    sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    if (!sws_ctx) {
        std::cerr << "Could not initialize scaling context" << std::endl;
        goto cleanup;
    }
    
    // Allocate frames
    packet = av_packet_alloc();
    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    
    if (!packet || !frame || !rgb_frame) {
        std::cerr << "Could not allocate packet or frames" << std::endl;
        goto cleanup;
    }
    
    // Allocate buffer for RGB frame
    {
        int rgb_buffer_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
        rgb_buffer = (uint8_t*)av_malloc(rgb_buffer_size);
        
        if (!rgb_buffer) {
            std::cerr << "Could not allocate RGB buffer" << std::endl;
            goto cleanup;
        }
        
        av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer, 
                            AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
    }
    
    std::cout << "Extracting frames from: " << input_filename << std::endl;
    std::cout << "Video resolution: " << codec_ctx->width << "x" << codec_ctx->height << std::endl;
    std::cout << "Frame interval: " << frame_interval << std::endl << std::endl;
    
    // Read frames
    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                std::cerr << "Error sending packet to decoder" << std::endl;
                break;
            }
            
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Error during decoding" << std::endl;
                    goto cleanup;
                }
                
                frame_count++;
                
                // Save frame if it matches our interval
                if (frame_count % frame_interval == 0) {
                    // Convert to RGB
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
                             rgb_frame->data, rgb_frame->linesize);
                    
                    // Save frame
                    save_frame_as_ppm(rgb_frame, codec_ctx->width, codec_ctx->height, frame_count);
                    saved_count++;
                }
                
                av_frame_unref(frame);
            }
        }
        av_packet_unref(packet);
    }
    
    std::cout << "\nExtraction complete!" << std::endl;
    std::cout << "Total frames processed: " << frame_count << std::endl;
    std::cout << "Frames saved: " << saved_count << std::endl;
    
cleanup:
    if (rgb_buffer) av_free(rgb_buffer);
    if (packet) av_packet_free(&packet);
    if (frame) av_frame_free(&frame);
    if (rgb_frame) av_frame_free(&rgb_frame);
    if (sws_ctx) sws_freeContext(sws_ctx);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) avformat_close_input(&format_ctx);
    
    return 0;
}
