#include <iostream>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
}

void print_stream_info(AVFormatContext* format_ctx) {
    std::cout << "=== Media File Information ===" << std::endl;
    std::cout << "Duration: " << format_ctx->duration / AV_TIME_BASE << " seconds" << std::endl;
    std::cout << "Bitrate: " << format_ctx->bit_rate << " bps" << std::endl;
    std::cout << "Number of streams: " << format_ctx->nb_streams << std::endl;
    
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        AVStream* stream = format_ctx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;
        
        std::cout << "\n--- Stream " << i << " ---" << std::endl;
        std::cout << "Type: ";
        
        switch (codecpar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                std::cout << "Video" << std::endl;
                std::cout << "Resolution: " << codecpar->width << "x" << codecpar->height << std::endl;
                {
                    const char* pix_fmt_name = av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecpar->format));
                    std::cout << "Pixel format: " << (pix_fmt_name ? pix_fmt_name : "unknown") << std::endl;
                }
                if (stream->avg_frame_rate.den != 0) {
                    std::cout << "Frame rate: " << (double)stream->avg_frame_rate.num / stream->avg_frame_rate.den << " fps" << std::endl;
                }
                break;
                
            case AVMEDIA_TYPE_AUDIO:
                std::cout << "Audio" << std::endl;
                std::cout << "Sample rate: " << codecpar->sample_rate << " Hz" << std::endl;
                std::cout << "Channels: " << codecpar->ch_layout.nb_channels << std::endl;
                std::cout << "Sample format: " << av_get_sample_fmt_name(static_cast<AVSampleFormat>(codecpar->format)) << std::endl;
                break;
                
            case AVMEDIA_TYPE_SUBTITLE:
                std::cout << "Subtitle" << std::endl;
                break;
                
            default:
                std::cout << "Unknown" << std::endl;
                break;
        }
        
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (codec) {
            std::cout << "Codec: " << codec->name << " (" << codec->long_name << ")" << std::endl;
        }
        
        std::cout << "Bitrate: " << codecpar->bit_rate << " bps" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }
    
    const char* input_filename = argv[1];
    AVFormatContext* format_ctx = nullptr;
    
    std::cout << "FFmpeg version: " << av_version_info() << std::endl;
    std::cout << "Analyzing file: " << input_filename << std::endl << std::endl;
    
    // Open input file
    int ret = avformat_open_input(&format_ctx, input_filename, nullptr, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Error opening file: " << error_buf << std::endl;
        return 1;
    }
    
    // Retrieve stream information
    ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "Error finding stream info: " << error_buf << std::endl;
        avformat_close_input(&format_ctx);
        return 1;
    }
    
    // Print file information
    print_stream_info(format_ctx);
    
    // Cleanup
    avformat_close_input(&format_ctx);
    
    std::cout << "\nAnalysis complete!" << std::endl;
    return 0;
}
