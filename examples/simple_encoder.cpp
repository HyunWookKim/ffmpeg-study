#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <width> <height> <frame_count> <output_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 640 480 30 test_output.mp4" << std::endl;
        return 1;
    }
    
    int width = std::atoi(argv[1]);
    int height = std::atoi(argv[2]);
    int frame_count = std::atoi(argv[3]);
    const char* output_filename = argv[4];
    
    if (width <= 0 || height <= 0 || frame_count <= 0) {
        std::cerr << "Width, height, and frame count must be positive" << std::endl;
        return 1;
    }
    
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVStream* stream = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    
    // Allocate output format context
    int ret = avformat_alloc_output_context2(&format_ctx, nullptr, nullptr, output_filename);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Could not create output context: " << error_buf << std::endl;
        return 1;
    }
    
    // Find H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "H.264 encoder not found" << std::endl;
        goto cleanup;
    }
    
    // Create new stream
    stream = avformat_new_stream(format_ctx, nullptr);
    if (!stream) {
        std::cerr << "Could not create new stream" << std::endl;
        goto cleanup;
    }
    
    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        std::cerr << "Could not allocate codec context" << std::endl;
        goto cleanup;
    }
    
    // Set codec parameters
    codec_ctx->bit_rate = 400000;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = {1, 25}; // 25 FPS
    codec_ctx->framerate = {25, 1};
    codec_ctx->gop_size = 10;
    codec_ctx->max_b_frames = 1;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Some formats want stream headers to be separate
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    // Open codec
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Could not open codec: " << error_buf << std::endl;
        goto cleanup;
    }
    
    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        std::cerr << "Could not copy codec parameters" << std::endl;
        goto cleanup;
    }
    
    // Open output file
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&format_ctx->pb, output_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Could not open output file: " << error_buf << std::endl;
            goto cleanup;
        }
    }
    
    // Write file header
    ret = avformat_write_header(format_ctx, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Error writing header: " << error_buf << std::endl;
        goto cleanup;
    }
    
    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate frame" << std::endl;
        goto cleanup;
    }
    
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    
    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        std::cerr << "Could not allocate frame buffer" << std::endl;
        goto cleanup;
    }
    
    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Could not allocate packet" << std::endl;
        goto cleanup;
    }
    
    std::cout << "Creating " << frame_count << " frames of " << width << "x" << height << " video..." << std::endl;
    
    // Generate frames
    for (int i = 0; i < frame_count; i++) {
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            std::cerr << "Could not make frame writable" << std::endl;
            break;
        }
        
        // Generate a simple pattern (moving gradient)
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Create a moving pattern
                int value = (x + y + i * 5) % 256;
                frame->data[0][y * frame->linesize[0] + x] = value; // Y
            }
        }
        
        // Fill UV planes with neutral values (gray)
        for (int y = 0; y < height / 2; y++) {
            for (int x = 0; x < width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128; // U
                frame->data[2][y * frame->linesize[2] + x] = 128; // V
            }
        }
        
        frame->pts = i;
        
        // Encode frame
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0) {
            std::cerr << "Error sending frame to encoder" << std::endl;
            break;
        }
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                std::cerr << "Error during encoding" << std::endl;
                goto encode_cleanup;
            }
            
            // Rescale packet timestamp
            av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            
            // Write packet
            ret = av_interleaved_write_frame(format_ctx, packet);
            if (ret < 0) {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                std::cerr << "Error writing packet: " << error_buf << std::endl;
                goto encode_cleanup;
            }
            
            av_packet_unref(packet);
        }
        
        if ((i + 1) % 10 == 0) {
            std::cout << "Encoded " << (i + 1) << "/" << frame_count << " frames" << std::endl;
        }
    }
    
    // Flush encoder
    ret = avcodec_send_frame(codec_ctx, nullptr);
    if (ret >= 0) {
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }
            
            av_packet_rescale_ts(packet, codec_ctx->time_base, stream->time_base);
            packet->stream_index = stream->index;
            av_interleaved_write_frame(format_ctx, packet);
            av_packet_unref(packet);
        }
    }
    
    // Write file trailer
    av_write_trailer(format_ctx);
    
    std::cout << "Video created successfully: " << output_filename << std::endl;
    
encode_cleanup:
    if (packet) av_packet_free(&packet);
    if (frame) av_frame_free(&frame);
    
cleanup:
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) {
        if (!(format_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&format_ctx->pb);
        }
        avformat_free_context(format_ctx);
    }
    
    return 0;
}
