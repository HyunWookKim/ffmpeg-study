#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <signal.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// 플랫폼별 하드웨어 가속 헤더 (Windows는 일단 제외)
#ifdef __APPLE__
#include <libavutil/hwcontext_videotoolbox.h>
#endif
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

struct FrameInfo {
    AVFrame* frame;
    int64_t pts;
    double timestamp;
    bool is_hardware;
    
    FrameInfo() : frame(nullptr), pts(0), timestamp(0.0), is_hardware(false) {}
    FrameInfo(AVFrame* f, int64_t p, double ts, bool hw) 
        : frame(f), pts(p), timestamp(ts), is_hardware(hw) {}
};

class HardwareVideoPlayer {
private:
    // FFmpeg components
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    AVCodecContext* audio_codec_ctx = nullptr;
    AVBufferRef* hw_device_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    
    // Stream info
    int video_stream_index = -1;
    int audio_stream_index = -1;
    AVRational time_base;
    
    // Threading
    std::thread decoder_thread;
    std::thread display_thread;
    std::thread audio_thread;
    
    // Frame queues
    std::queue<FrameInfo> video_queue;
    std::queue<FrameInfo> audio_queue;
    std::mutex video_mutex, audio_mutex;
    std::condition_variable video_cv, audio_cv;
    
    // Playback control
    std::atomic<bool> should_stop{false};
    std::atomic<bool> is_paused{false};
    std::atomic<bool> is_seeking{false};
    std::atomic<double> current_time{0.0};
    std::atomic<double> duration{0.0};
    std::atomic<int> frame_count{0};        // Successfully displayed frames
    std::atomic<int> decoded_frames{0};     // Total decoded frames
    std::atomic<int> dropped_frames{0};
    
    // Display
    AVFrame* display_frame = nullptr;
    const size_t MAX_QUEUE_SIZE = 10;
    
    // Timing
    int64_t playback_start_time = 0;
    double video_clock = 0.0;
    
public:
    ~HardwareVideoPlayer() {
        cleanup();
    }
    
    bool initialize_hardware_acceleration() {
        std::cout << "[INIT] Initializing VideoToolbox hardware acceleration..." << std::endl;
        
        int ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
        if (ret < 0) {
            print_error("Failed to create VideoToolbox device context", ret);
            return false;
        }
        
        std::cout << "[OK] VideoToolbox hardware acceleration initialized successfully!" << std::endl;
        return true;
    }
    
    bool open_media(const char* filename) {
        std::cout << "[OPEN] Opening media file: " << filename << std::endl;
        
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
        
        // Get duration
        if (format_ctx->duration != AV_NOPTS_VALUE) {
            duration = format_ctx->duration / (double)AV_TIME_BASE;
        }
        
        // Find video and audio streams
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0) {
                video_stream_index = i;
            } else if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index < 0) {
                audio_stream_index = i;
            }
        }
        
        if (video_stream_index == -1) {
            std::cerr << "[ERROR] Could not find video stream" << std::endl;
            return false;
        }
        
        // Setup video codec
        if (!setup_video_codec()) {
            return false;
        }
        
        // Setup audio codec (optional)
        if (audio_stream_index >= 0) {
            setup_audio_codec();
        }
        
        // Print media information
        print_media_info();
        
        return true;
    }
    
    bool setup_video_codec() {
        AVCodecParameters* codecpar = format_ctx->streams[video_stream_index]->codecpar;
        time_base = format_ctx->streams[video_stream_index]->time_base;
        
        // Find decoder
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cerr << "[ERROR] Video decoder not found" << std::endl;
            return false;
        }
        
        // Check for hardware acceleration support
        bool is_hardware_supported = (codecpar->codec_id == AV_CODEC_ID_H264 || 
                                     codecpar->codec_id == AV_CODEC_ID_HEVC);
        
        if (is_hardware_supported) {
            std::cout << "[INFO] Using " << avcodec_get_name(codecpar->codec_id) 
                     << " decoder with VideoToolbox hardware acceleration" << std::endl;
        } else {
            std::cout << "[INFO] Using software decoder for " << avcodec_get_name(codecpar->codec_id) << std::endl;
        }
        
        // Allocate codec context
        video_codec_ctx = avcodec_alloc_context3(codec);
        if (!video_codec_ctx) {
            std::cerr << "[ERROR] Could not allocate video codec context" << std::endl;
            return false;
        }
        
        // Copy codec parameters
        int ret = avcodec_parameters_to_context(video_codec_ctx, codecpar);
        if (ret < 0) {
            print_error("Could not copy video codec parameters", ret);
            return false;
        }
        
        // Set hardware device context if supported
        if (hw_device_ctx && is_hardware_supported) {
            video_codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            video_codec_ctx->get_format = get_hw_format;
        }
        
        // Open codec
        ret = avcodec_open2(video_codec_ctx, codec, nullptr);
        if (ret < 0) {
            print_error("Could not open video codec", ret);
            return false;
        }
        
        return true;
    }
    
    bool setup_audio_codec() {
        AVCodecParameters* codecpar = format_ctx->streams[audio_stream_index]->codecpar;
        
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) {
            std::cout << "[WARN] Audio decoder not found, skipping audio" << std::endl;
            audio_stream_index = -1;
            return false;
        }
        
        audio_codec_ctx = avcodec_alloc_context3(codec);
        if (!audio_codec_ctx) {
            std::cout << "[WARN] Could not allocate audio codec context" << std::endl;
            audio_stream_index = -1;
            return false;
        }
        
        int ret = avcodec_parameters_to_context(audio_codec_ctx, codecpar);
        if (ret < 0) {
            print_error("Could not copy audio codec parameters", ret);
            audio_stream_index = -1;
            return false;
        }
        
        ret = avcodec_open2(audio_codec_ctx, codec, nullptr);
        if (ret < 0) {
            print_error("Could not open audio codec", ret);
            audio_stream_index = -1;
            return false;
        }
        
        std::cout << "[AUDIO] Audio stream found: " << avcodec_get_name(codecpar->codec_id) 
                 << " @ " << codecpar->sample_rate << "Hz" << std::endl;
        return true;
    }
    
    void print_media_info() {
        std::cout << "\n[INFO] Media Information" << std::endl;
        std::cout << "===================" << std::endl;
        std::cout << "Duration: " << std::fixed << std::setprecision(2) << duration.load() << " seconds" << std::endl;
        std::cout << "Video: " << video_codec_ctx->width << "x" << video_codec_ctx->height 
                 << " @ " << av_q2d(format_ctx->streams[video_stream_index]->r_frame_rate) << " FPS" << std::endl;
        std::cout << "Codec: " << video_codec_ctx->codec->name << std::endl;
        std::cout << "Hardware acceleration: " << (video_codec_ctx->hw_device_ctx ? "YES (VideoToolbox)" : "NO") << std::endl;
        if (audio_stream_index >= 0) {
            std::cout << "Audio: " << audio_codec_ctx->sample_rate << "Hz, " 
                     << audio_codec_ctx->ch_layout.nb_channels << " channels" << std::endl;
        }
        std::cout << std::endl;
    }
    
    void start_playback() {
        std::cout << "[START] Starting playback..." << std::endl;
        
        playback_start_time = av_gettime();
        
        // Start threads
        decoder_thread = std::thread(&HardwareVideoPlayer::decode_worker, this);
        display_thread = std::thread(&HardwareVideoPlayer::display_worker, this);
        
        if (audio_stream_index >= 0) {
            audio_thread = std::thread(&HardwareVideoPlayer::audio_worker, this);
        }
        
        // Control loop
        control_loop();
        
        // Wait for threads to finish
        if (decoder_thread.joinable()) decoder_thread.join();
        if (display_thread.joinable()) display_thread.join();
        if (audio_thread.joinable()) audio_thread.join();
        
        std::cout << "\n[OK] Playback finished!" << std::endl;
        std::cout << "[STATS] Statistics:" << std::endl;
        std::cout << "   Decoded frames: " << decoded_frames.load() << std::endl;
        std::cout << "   Displayed frames: " << frame_count.load() << std::endl;
        std::cout << "   Dropped frames: " << dropped_frames.load() << std::endl;
        std::cout << "   Final queue size: " << video_queue.size() << std::endl;
        std::cout << "   Max queue size: " << MAX_QUEUE_SIZE << std::endl;
    }
    
    void decode_worker() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        
        while (!should_stop && av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                decode_video_packet(packet, frame);
            } else if (packet->stream_index == audio_stream_index) {
                decode_audio_packet(packet, frame);
            }
            av_packet_unref(packet);
        }
        
        // Check if we reached end of file
        if (!should_stop) {
            should_stop = true; // Signal all threads to stop
            std::cout << "\n[EOF] Reached end of file, playback completed!" << std::endl;
            show_final_progress(); // Show 100% completion
        }
        
        av_packet_free(&packet);
        av_frame_free(&frame);
    }
    
    void decode_video_packet(AVPacket* packet, AVFrame* frame) {
        int ret = avcodec_send_packet(video_codec_ctx, packet);
        if (ret < 0) return;
        
        while (ret >= 0) {
            ret = avcodec_receive_frame(video_codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }
            
            decoded_frames++; // Count decoded frames
            
            // Check queue size
            {
                std::unique_lock<std::mutex> lock(video_mutex);
                if (video_queue.size() >= MAX_QUEUE_SIZE) {
                    // Drop oldest frame
                    if (!video_queue.empty()) {
                        FrameInfo old_frame = video_queue.front();
                        video_queue.pop();
                        av_frame_free(&old_frame.frame);
                        dropped_frames++;
                    }
                }
                
                // Add new frame
                AVFrame* new_frame = av_frame_alloc();
                av_frame_ref(new_frame, frame);
                
                double timestamp = frame->pts * av_q2d(time_base);
                bool is_hw = (frame->format == AV_PIX_FMT_VIDEOTOOLBOX);
                
                video_queue.emplace(new_frame, frame->pts, timestamp, is_hw);
            }
            video_cv.notify_one();
            
            av_frame_unref(frame);
        }
    }
    
    void decode_audio_packet(AVPacket* packet, AVFrame* frame) {
        if (!audio_codec_ctx) return;
        
        int ret = avcodec_send_packet(audio_codec_ctx, packet);
        if (ret < 0) return;
        
        while (ret >= 0) {
            ret = avcodec_receive_frame(audio_codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                break;
            }
            
            // Add to audio queue (simplified - no actual audio output in this example)
            av_frame_unref(frame);
        }
    }
    
    void display_worker() {
        while (!should_stop) {
            FrameInfo frame_info;
            
            // Get frame from queue
            {
                std::unique_lock<std::mutex> lock(video_mutex);
                video_cv.wait(lock, [this] { return !video_queue.empty() || should_stop; });
                
                if (should_stop && video_queue.empty()) break;
                
                frame_info = video_queue.front();
                video_queue.pop();
            }
            
            // Process and display frame
            display_frame_info(frame_info);
            
            // Update timing
            video_clock = frame_info.timestamp;
            current_time = video_clock;
            frame_count++;
            
            // Frame rate control
            control_frame_rate(frame_info.timestamp);
            
            av_frame_free(&frame_info.frame);
        }
    }
    
    void display_frame_info(const FrameInfo& frame_info) {
        if (frame_count % 30 == 0) {  // Print every 30 frames
            std::cout << "[PLAY] Frame " << frame_count.load() 
                     << " | Time: " << std::fixed << std::setprecision(2) << frame_info.timestamp << "s"
                     << " | " << (frame_info.is_hardware ? "HW" : "SW")
                     << " | Queue: " << video_queue.size() << "/" << MAX_QUEUE_SIZE
                     << " | Decoded: " << decoded_frames.load()
                     << " | Dropped: " << dropped_frames.load() << std::endl;
        }
        
        // Here you would typically render the frame to a window
        // For this console example, we just process the frame data
        process_frame_for_display(frame_info.frame);
    }
    
    void process_frame_for_display(AVFrame* frame) {
        // Convert hardware frame to software if needed
        AVFrame* sw_frame = frame;
        AVFrame* temp_frame = nullptr;
        
        if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
            // Transfer hardware frame to software
            temp_frame = av_frame_alloc();
            if (av_hwframe_transfer_data(temp_frame, frame, 0) >= 0) {
                sw_frame = temp_frame;
            }
        }
        
        // Here you would render sw_frame to your display
        // For now, we just validate we can access the frame data
        if (sw_frame && sw_frame->data[0]) {
            // Frame is ready for display
        }
        
        if (temp_frame) {
            av_frame_free(&temp_frame);
        }
    }
    
    void control_frame_rate(double timestamp) {
        if (is_paused) {
            while (is_paused && !should_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            playback_start_time = av_gettime() - (int64_t)(timestamp * 1000000);
        }
        
        // Calculate expected time
        int64_t expected_time = playback_start_time + (int64_t)(timestamp * 1000000);
        int64_t current_time_us = av_gettime();
        
        if (expected_time > current_time_us) {
            std::this_thread::sleep_for(
                std::chrono::microseconds(expected_time - current_time_us)
            );
        }
    }
    
    void audio_worker() {
        // Audio playback would be implemented here
        // For this example, we just track that audio is available
        std::cout << "[AUDIO] Audio thread started (output not implemented in this example)" << std::endl;
        
        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void control_loop() {
        std::cout << "\n[CTRL] Playback Controls:" << std::endl;
        std::cout << "   SPACE: Pause/Resume" << std::endl;
        std::cout << "   Q: Quit" << std::endl;
        std::cout << "   I: Show info" << std::endl;
        std::cout << "   (This is a console demo - controls are not interactive)" << std::endl;
        std::cout << "\n[INFO] Progress Display Format:" << std::endl;
        std::cout << "   Q: Queue usage (current/max)" << std::endl;
        std::cout << "   D: Total decoded frames" << std::endl;
        std::cout << "   Drop: Frames dropped due to queue overflow" << std::endl;
        
        auto last_info_time = std::chrono::steady_clock::now();
        
        while (!should_stop) {
            auto now = std::chrono::steady_clock::now();
            
            // Show progress every 2 seconds
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_info_time).count() >= 2) {
                show_playback_progress();
                last_info_time = now;
            }
            
            // Check if playback finished
            if ((current_time.load() >= duration.load() && duration.load() > 0) || should_stop) {
                if (!should_stop) {
                    std::cout << "\n[EOF] Reached end of media" << std::endl;
                    should_stop = true;
                }
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void show_playback_progress() {
        double current = current_time.load();
        double total = duration.load();
        
        if (total > 0) {
            double progress = (current / total) * 100.0;
            int bar_width = 30; // Reduced to make room for queue info
            int filled = (int)(progress * bar_width / 100.0);
            
            std::cout << "\r[PROG] [";
            for (int i = 0; i < bar_width; i++) {
                if (i < filled) std::cout << "#";
                else std::cout << "-";
            }
            std::cout << "] " << std::fixed << std::setprecision(1) << progress << "% "
                     << "(" << (int)current << "s/" << (int)total << "s)"
                     << " | Q:" << video_queue.size() << "/" << MAX_QUEUE_SIZE
                     << " | D:" << decoded_frames.load()
                     << " | Drop:" << dropped_frames.load() << std::flush;
        }
    }
    
    void show_final_progress() {
        double total = duration.load();
        int bar_width = 30; // Consistent with show_playback_progress
        
        std::cout << "\r[PROG] [";
        for (int i = 0; i < bar_width; i++) {
            std::cout << "#";
        }
        std::cout << "] 100.0% (" << (int)total << "s/" << (int)total << "s)"
                 << " | Q:" << video_queue.size() << "/" << MAX_QUEUE_SIZE
                 << " | D:" << decoded_frames.load()
                 << " | Drop:" << dropped_frames.load() << std::endl;
    }
    
    void pause_resume() {
        is_paused = !is_paused;
        std::cout << (is_paused ? "\n[PAUSE] Paused" : "\n[RESUME] Resumed") << std::endl;
    }
    
    void stop_playback() {
        should_stop = true;
        video_cv.notify_all();
        audio_cv.notify_all();
    }
    
private:
    void cleanup() {
        stop_playback();
        
        if (video_codec_ctx) avcodec_free_context(&video_codec_ctx);
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
        if (format_ctx) avformat_close_input(&format_ctx);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        if (sws_ctx) sws_freeContext(sws_ctx);
        
        // Clear queues
        while (!video_queue.empty()) {
            FrameInfo frame_info = video_queue.front();
            video_queue.pop();
            av_frame_free(&frame_info.frame);
        }
    }
    
    void print_error(const char* message, int error_code) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(error_code, error_buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << "[ERROR] " << message << ": " << error_buf << std::endl;
    }
};

void print_usage(const char* program_name) {
    std::cout << "[PLAYER] Hardware Accelerated Video Player" << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Usage: " << program_name << " <video_file>" << std::endl << std::endl;
    
    std::cout << "Features:" << std::endl;
    std::cout << "• M1 Mac VideoToolbox hardware acceleration" << std::endl;
    std::cout << "• Real-time frame rate control" << std::endl;
    std::cout << "• Multi-threaded decoding and display" << std::endl;
    std::cout << "• Support for H.264, HEVC, and other formats" << std::endl;
    std::cout << "• Performance monitoring and statistics" << std::endl << std::endl;
    
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " media/samples/h264_sample.mp4" << std::endl;
    std::cout << "  " << program_name << " media/samples/hevc_sample.mp4" << std::endl;
    std::cout << "  " << program_name << " /path/to/your/video.mp4" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* video_file = argv[1];
    
    std::cout << "[PLAYER] Hardware Accelerated Video Player" << std::endl;
    std::cout << "==========================================" << std::endl;
    
    HardwareVideoPlayer player;
    
    // Initialize hardware acceleration
    if (!player.initialize_hardware_acceleration()) {
        std::cerr << "[WARN] Continuing with software decoding only" << std::endl;
    }
    
    // Open media file
    if (!player.open_media(video_file)) {
        std::cerr << "[ERROR] Failed to open media file: " << video_file << std::endl;
        return 1;
    }
    
    // Signal handling for graceful shutdown
    signal(SIGINT, [](int) {
        std::cout << "\n[STOP] Stopping playback..." << std::endl;
        exit(0);
    });
    
    // Start playback
    player.start_playback();
    
    return 0;
}
