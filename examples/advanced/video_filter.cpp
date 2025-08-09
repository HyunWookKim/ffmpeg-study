#include <iostream>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoFilterProcessor {
private:
    AVFormatContext* input_fmt_ctx = nullptr;
    AVFormatContext* output_fmt_ctx = nullptr;
    AVCodecContext* decoder_ctx = nullptr;
    AVCodecContext* encoder_ctx = nullptr;
    AVFilterGraph* filter_graph = nullptr;
    AVFilterContext* buffersrc_ctx = nullptr;
    AVFilterContext* buffersink_ctx = nullptr;
    int video_stream_index = -1;
    
public:
    ~VideoFilterProcessor() {
        cleanup();
    }
    
    bool setup_input(const char* filename) {
        int ret = avformat_open_input(&input_fmt_ctx, filename, nullptr, nullptr);
        if (ret < 0) {
            print_error("Could not open input file", ret);
            return false;
        }
        
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
        
        return true;
    }
    
    bool setup_output(const char* filename) {
        int ret = avformat_alloc_output_context2(&output_fmt_ctx, nullptr, nullptr, filename);
        if (ret < 0) {
            print_error("Could not create output context", ret);
            return false;
        }
        
        // Create output stream
        AVStream* out_stream = avformat_new_stream(output_fmt_ctx, nullptr);
        if (!out_stream) {
            std::cerr << "Could not create output stream" << std::endl;
            return false;
        }
        
        // Setup encoder (using H.264)
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
        
        // Set encoder parameters (will be updated after filter setup)
        encoder_ctx->codec_id = AV_CODEC_ID_H264;
        encoder_ctx->bit_rate = 2000000;
        encoder_ctx->width = decoder_ctx->width;
        encoder_ctx->height = decoder_ctx->height;
        encoder_ctx->time_base = {1, 25};
        encoder_ctx->framerate = {25, 1};
        encoder_ctx->gop_size = 10;
        encoder_ctx->max_b_frames = 1;
        encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        
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
        
        // Open output file
        if (!(output_fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open(&output_fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
            if (ret < 0) {
                print_error("Could not open output file", ret);
                return false;
            }
        }
        
        ret = avformat_write_header(output_fmt_ctx, nullptr);
        if (ret < 0) {
            print_error("Error writing header", ret);
            return false;
        }
        
        return true;
    }
    
    bool setup_filters(const std::string& filter_desc) {
        const AVFilter* buffersrc = avfilter_get_by_name("buffer");
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        AVFilterInOut* outputs = avfilter_inout_alloc();
        AVFilterInOut* inputs = avfilter_inout_alloc();
        
        filter_graph = avfilter_graph_alloc();
        if (!outputs || !inputs || !filter_graph) {
            std::cerr << "Could not allocate filter graph" << std::endl;
            return false;
        }
        
        // Create buffer source
        char args[512];
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt,
                 input_fmt_ctx->streams[video_stream_index]->time_base.num,
                 input_fmt_ctx->streams[video_stream_index]->time_base.den,
                 decoder_ctx->sample_aspect_ratio.num,
                 decoder_ctx->sample_aspect_ratio.den);
        
        int ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                               args, nullptr, filter_graph);
        if (ret < 0) {
            print_error("Could not create buffer source", ret);
            return false;
        }
        
        // Create buffer sink
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           nullptr, nullptr, filter_graph);
        if (ret < 0) {
            print_error("Could not create buffer sink", ret);
            return false;
        }
        
        // Set pixel format for sink
        enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
        ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            print_error("Could not set output pixel format", ret);
            return false;
        }
        
        // Configure filter graph
        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;
        
        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;
        
        ret = avfilter_graph_parse_ptr(filter_graph, filter_desc.c_str(),
                                       &inputs, &outputs, nullptr);
        if (ret < 0) {
            print_error("Could not parse filter graph", ret);
            return false;
        }
        
        ret = avfilter_graph_config(filter_graph, nullptr);
        if (ret < 0) {
            print_error("Could not configure filter graph", ret);
            return false;
        }
        
        // Update encoder dimensions based on filter output
        AVFilterLink* outlink = buffersink_ctx->inputs[0];
        encoder_ctx->width = outlink->w;
        encoder_ctx->height = outlink->h;
        
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        
        std::cout << "🎬 Filter setup complete: " << filter_desc << std::endl;
        std::cout << "   Input: " << decoder_ctx->width << "x" << decoder_ctx->height << std::endl;
        std::cout << "   Output: " << encoder_ctx->width << "x" << encoder_ctx->height << std::endl;
        
        return true;
    }
    
    void process_video() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* filtered_frame = av_frame_alloc();
        AVPacket* out_packet = av_packet_alloc();
        
        if (!packet || !frame || !filtered_frame || !out_packet) {
            std::cerr << "Could not allocate packets or frames" << std::endl;
            return;
        }
        
        int frame_count = 0;
        std::cout << "\n🎥 Starting video processing..." << std::endl;
        
        while (av_read_frame(input_fmt_ctx, packet) >= 0) {
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
                    
                    // Filter
                    ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
                    if (ret < 0) {
                        print_error("Error adding frame to filter", ret);
                        goto cleanup;
                    }
                    
                    while (1) {
                        ret = av_buffersink_get_frame(buffersink_ctx, filtered_frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        }
                        if (ret < 0) {
                            print_error("Error getting filtered frame", ret);
                            goto cleanup;
                        }
                        
                        // Encode
                        filtered_frame->pts = frame_count;
                        ret = avcodec_send_frame(encoder_ctx, filtered_frame);
                        if (ret < 0) {
                            print_error("Error sending frame to encoder", ret);
                            av_frame_unref(filtered_frame);
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
                            
                            // Write packet
                            av_packet_rescale_ts(out_packet, encoder_ctx->time_base, 
                                                output_fmt_ctx->streams[0]->time_base);
                            out_packet->stream_index = 0;
                            
                            ret = av_interleaved_write_frame(output_fmt_ctx, out_packet);
                            if (ret < 0) {
                                print_error("Error writing packet", ret);
                                goto cleanup;
                            }
                            
                            av_packet_unref(out_packet);
                        }
                        
                        frame_count++;
                        if (frame_count % 30 == 0) {
                            std::cout << "📹 Processed " << frame_count << " frames" << std::endl;
                        }
                        
                        av_frame_unref(filtered_frame);
                    }
                    
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(packet);
        }
        
        // Flush filters and encoder
        {
            int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, nullptr, 0);
            if (ret < 0) {
                print_error("Error flushing filter", ret);
                goto cleanup;
            }
        }
        
        while (1) {
            int ret = av_buffersink_get_frame(buffersink_ctx, filtered_frame);
            if (ret == AVERROR_EOF) break;
            if (ret < 0) break;
            
            filtered_frame->pts = frame_count++;
            avcodec_send_frame(encoder_ctx, filtered_frame);
            
            while (avcodec_receive_packet(encoder_ctx, out_packet) >= 0) {
                av_packet_rescale_ts(out_packet, encoder_ctx->time_base, 
                                   output_fmt_ctx->streams[0]->time_base);
                out_packet->stream_index = 0;
                av_interleaved_write_frame(output_fmt_ctx, out_packet);
                av_packet_unref(out_packet);
            }
            
            av_frame_unref(filtered_frame);
        }
        
        // Flush encoder
        avcodec_send_frame(encoder_ctx, nullptr);
        while (avcodec_receive_packet(encoder_ctx, out_packet) >= 0) {
            av_packet_rescale_ts(out_packet, encoder_ctx->time_base, 
                               output_fmt_ctx->streams[0]->time_base);
            out_packet->stream_index = 0;
            av_interleaved_write_frame(output_fmt_ctx, out_packet);
            av_packet_unref(out_packet);
        }
        
        av_write_trailer(output_fmt_ctx);
        
        std::cout << "✅ Processing complete! Total frames: " << frame_count << std::endl;
        
    cleanup:
        av_packet_free(&packet);
        av_frame_free(&frame);
        av_frame_free(&filtered_frame);
        av_packet_free(&out_packet);
    }
    
private:
    void cleanup() {
        if (decoder_ctx) avcodec_free_context(&decoder_ctx);
        if (encoder_ctx) avcodec_free_context(&encoder_ctx);
        if (input_fmt_ctx) avformat_close_input(&input_fmt_ctx);
        if (output_fmt_ctx) {
            if (!(output_fmt_ctx->oformat->flags & AVFMT_NOFILE))
                avio_closep(&output_fmt_ctx->pb);
            avformat_free_context(output_fmt_ctx);
        }
        if (filter_graph) avfilter_graph_free(&filter_graph);
    }
    
    void print_error(const char* message, int error_code) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << message << ": " << error_buf << std::endl;
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <input_file> <output_file> [filter]" << std::endl;
    std::cout << "\nAvailable filters:" << std::endl;
    std::cout << "  blur           - Apply Gaussian blur" << std::endl;
    std::cout << "  scale_half     - Scale down to 50%" << std::endl;
    std::cout << "  brightness     - Increase brightness" << std::endl;
    std::cout << "  rotate         - Rotate 90 degrees" << std::endl;
    std::cout << "  edge_detect    - Edge detection filter" << std::endl;
    std::cout << "  vintage        - Vintage color effect" << std::endl;
    std::cout << "  custom         - Custom filter (you can modify the code)" << std::endl;
    std::cout << "\nExample: " << program_name << " input.mp4 output.mp4 blur" << std::endl;
}

std::string get_filter_description(const std::string& filter_name) {
    if (filter_name == "blur") {
        return "gblur=sigma=2";
    } else if (filter_name == "scale_half") {
        return "scale=iw*0.5:ih*0.5";
    } else if (filter_name == "brightness") {
        return "eq=brightness=0.2";
    } else if (filter_name == "rotate") {
        return "transpose=1";
    } else if (filter_name == "edge_detect") {
        return "edgedetect=low=0.1:high=0.4";
    } else if (filter_name == "vintage") {
        return "colorchannelmixer=.3:.4:.3:0:.3:.4:.3:0:.3:.4:.3";
    } else if (filter_name == "custom") {
        // 복합 필터: 블러 + 밝기 조정 + 색상 조정
        return "gblur=sigma=1,eq=brightness=0.1:contrast=1.2,hue=h=10";
    } else {
        return "null"; // No filter
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    const char* output_file = argv[2];
    std::string filter_name = (argc > 3) ? argv[3] : "null";
    
    std::cout << "🎬 Advanced Video Filter Processor" << std::endl;
    std::cout << "===================================" << std::endl;
    std::cout << "Input: " << input_file << std::endl;
    std::cout << "Output: " << output_file << std::endl;
    std::cout << "Filter: " << filter_name << std::endl << std::endl;
    
    VideoFilterProcessor processor;
    
    if (!processor.setup_input(input_file)) {
        return 1;
    }
    
    if (!processor.setup_output(output_file)) {
        return 1;
    }
    
    std::string filter_desc = get_filter_description(filter_name);
    if (!processor.setup_filters(filter_desc)) {
        return 1;
    }
    
    processor.process_video();
    
    return 0;
}
