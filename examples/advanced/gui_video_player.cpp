#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <SDL2/SDL.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_videotoolbox.h>
#include <libswscale/swscale.h>
}

class GUIVideoPlayer {
private:
    // FFmpeg 구조체
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    const AVCodec* video_codec = nullptr;
    AVBufferRef* hw_device_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    
    // SDL 구조체
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    
    // 비디오 정보
    int video_stream_index = -1;
    int video_width = 0;
    int video_height = 0;
    double frame_rate = 0.0;
    int64_t total_frames = 0;
    double duration = 0.0;
    
    // 재생 제어
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> should_quit{false};
    std::atomic<double> playback_speed{1.0};
    std::atomic<int64_t> current_frame{0};
    
    // 스레드 동기화
    std::queue<AVFrame*> frame_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    static constexpr size_t MAX_QUEUE_SIZE = 10;
    
    std::thread decoder_thread;
    std::thread render_thread;

public:
    GUIVideoPlayer() = default;
    
    ~GUIVideoPlayer() {
        cleanup();
    }
    
    bool initialize(const std::string& filename) {
        // SDL 초기화
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "❌ SDL 초기화 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // FFmpeg 비디오 파일 열기
        if (avformat_open_input(&format_ctx, filename.c_str(), nullptr, nullptr) != 0) {
            std::cerr << "❌ 비디오 파일을 열 수 없습니다: " << filename << std::endl;
            return false;
        }
        
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            std::cerr << "❌ 스트림 정보를 찾을 수 없습니다" << std::endl;
            return false;
        }
        
        // 비디오 스트림 찾기
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        
        if (video_stream_index == -1) {
            std::cerr << "❌ 비디오 스트림을 찾을 수 없습니다" << std::endl;
            return false;
        }
        
        // 비디오 정보 추출
        AVStream* video_stream = format_ctx->streams[video_stream_index];
        video_width = video_stream->codecpar->width;
        video_height = video_stream->codecpar->height;
        
        if (video_stream->r_frame_rate.den > 0) {
            frame_rate = av_q2d(video_stream->r_frame_rate);
        } else {
            frame_rate = 25.0; // 기본값
        }
        
        if (format_ctx->duration != AV_NOPTS_VALUE) {
            duration = static_cast<double>(format_ctx->duration) / AV_TIME_BASE;
            total_frames = static_cast<int64_t>(duration * frame_rate);
        }
        
        // 코덱 찾기 및 설정
        video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!video_codec) {
            std::cerr << "❌ 비디오 코덱을 찾을 수 없습니다" << std::endl;
            return false;
        }
        
        video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (!video_codec_ctx) {
            std::cerr << "❌ 코덱 컨텍스트 할당 실패" << std::endl;
            return false;
        }
        
        if (avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar) < 0) {
            std::cerr << "❌ 코덱 매개변수 복사 실패" << std::endl;
            return false;
        }
        
        // 하드웨어 가속 설정 시도
        setupHardwareAcceleration();
        
        if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
            std::cerr << "❌ 코덱 열기 실패" << std::endl;
            return false;
        }
        
        // SDL 윈도우 생성
        window = SDL_CreateWindow(
            ("FFmpeg GUI Player - " + filename).c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            video_width, video_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
        
        if (!window) {
            std::cerr << "❌ SDL 윈도우 생성 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            std::cerr << "❌ SDL 렌더러 생성 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_YV12,
            SDL_TEXTUREACCESS_STREAMING,
            video_width, video_height
        );
        
        if (!texture) {
            std::cerr << "❌ SDL 텍스처 생성 실패: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // SwsContext 생성 (하드웨어 프레임 → YUV420P 변환용)
        sws_ctx = sws_getContext(
            video_width, video_height, AV_PIX_FMT_YUV420P,
            video_width, video_height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        std::cout << "🎬 GUI 비디오 플레이어 초기화 완료!" << std::endl;
        std::cout << "📹 " << video_width << "x" << video_height 
                  << " @ " << frame_rate << " FPS" << std::endl;
        std::cout << "⏱️  재생 시간: " << duration << "초 (" << total_frames << " 프레임)" << std::endl;
        
        if (hw_device_ctx) {
            std::cout << "🖥️  VideoToolbox 하드웨어 가속 활성화!" << std::endl;
        }
        
        return true;
    }
    
    void play() {
        if (playing) return;
        
        playing = true;
        should_quit = false;
        
        // 디코더 스레드 시작
        decoder_thread = std::thread(&GUIVideoPlayer::decoderWorker, this);
        
        // 렌더링 스레드 시작  
        render_thread = std::thread(&GUIVideoPlayer::renderWorker, this);
        
        // 메인 이벤트 루프
        eventLoop();
        
        // 스레드 정리
        should_quit = true;
        queue_condition.notify_all();
        
        if (decoder_thread.joinable()) {
            decoder_thread.join();
        }
        if (render_thread.joinable()) {
            render_thread.join();
        }
    }
    
private:
    void setupHardwareAcceleration() {
        // VideoToolbox 하드웨어 가속 설정
        if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0) == 0) {
            video_codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            video_codec_ctx->get_format = getHardwareFormat;
            std::cout << "✅ VideoToolbox 하드웨어 가속 초기화 성공!" << std::endl;
        }
    }
    
    static enum AVPixelFormat getHardwareFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
        const enum AVPixelFormat *p;
        for (p = pix_fmts; *p != -1; p++) {
            if (*p == AV_PIX_FMT_VIDEOTOOLBOX) {
                return *p;
            }
        }
        return AV_PIX_FMT_YUV420P; // fallback
    }
    
    void decoderWorker() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* sw_frame = av_frame_alloc();
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (!should_quit && av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                if (avcodec_send_packet(video_codec_ctx, packet) == 0) {
                    while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                        // 큐가 가득 찬 경우 대기
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        queue_condition.wait(lock, [this] { 
                            return frame_queue.size() < MAX_QUEUE_SIZE || should_quit; 
                        });
                        
                        if (should_quit) break;
                        
                        AVFrame* display_frame = av_frame_alloc();
                        
                        // 하드웨어 프레임인 경우 소프트웨어로 전송
                        if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX) {
                            if (av_hwframe_transfer_data(sw_frame, frame, 0) == 0) {
                                av_frame_copy_props(sw_frame, frame);
                                av_frame_ref(display_frame, sw_frame);
                            } else {
                                av_frame_ref(display_frame, frame);
                            }
                        } else {
                            av_frame_ref(display_frame, frame);
                        }
                        
                        frame_queue.push(display_frame);
                        current_frame++;
                        
                        av_frame_unref(frame);
                        av_frame_unref(sw_frame);
                    }
                }
            }
            av_packet_unref(packet);
            
            // 일시정지 처리
            while (paused && !should_quit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        av_frame_free(&frame);
        av_frame_free(&sw_frame);
        av_packet_free(&packet);
    }
    
    void renderWorker() {
        const auto frame_duration = std::chrono::duration<double>(1.0 / frame_rate);
        auto last_frame_time = std::chrono::steady_clock::now();
        
        while (!should_quit) {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            if (frame_queue.empty()) {
                queue_condition.wait_for(lock, std::chrono::milliseconds(10));
                continue;
            }
            
            AVFrame* frame = frame_queue.front();
            frame_queue.pop();
            lock.unlock();
            
            // 프레임 타이밍 조절
            auto current_time = std::chrono::steady_clock::now();
            auto adjusted_duration = std::chrono::duration<double>(frame_duration.count() / playback_speed.load());
            auto time_since_last = current_time - last_frame_time;
            
            if (time_since_last < adjusted_duration) {
                std::this_thread::sleep_for(adjusted_duration - time_since_last);
            }
            
            renderFrame(frame);
            av_frame_free(&frame);
            
            last_frame_time = std::chrono::steady_clock::now();
            
            // 일시정지 처리
            while (paused && !should_quit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    void renderFrame(AVFrame* frame) {
        if (!frame || frame->format != AV_PIX_FMT_YUV420P) {
            return;
        }
        
        // YUV 데이터를 SDL 텍스처에 업데이트
        SDL_UpdateYUVTexture(
            texture, nullptr,
            frame->data[0], frame->linesize[0],  // Y plane
            frame->data[1], frame->linesize[1],  // U plane  
            frame->data[2], frame->linesize[2]   // V plane
        );
        
        // 렌더링
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        
        // 재생 정보 오버레이 (간단한 텍스트는 생략하고 타이틀바에만 표시)
        std::string title = "FFmpeg GUI Player - Frame: " + std::to_string(current_frame.load()) + 
                           "/" + std::to_string(total_frames) + 
                           (paused ? " [PAUSED]" : " [PLAYING]") +
                           " Speed: " + std::to_string(playback_speed.load()) + "x";
        SDL_SetWindowTitle(window, title.c_str());
        
        SDL_RenderPresent(renderer);
    }
    
    void eventLoop() {
        SDL_Event event;
        
        std::cout << "\n🎮 조작법:" << std::endl;
        std::cout << "  SPACE: 재생/일시정지" << std::endl;
        std::cout << "  ↑/↓: 재생 속도 조절" << std::endl;
        std::cout << "  ESC/Q: 종료" << std::endl;
        std::cout << "  클릭: 윈도우 닫기로 종료\n" << std::endl;
        
        while (!should_quit) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        should_quit = true;
                        break;
                        
                    case SDL_KEYDOWN:
                        handleKeyPress(event.key.keysym.sym);
                        break;
                        
                    case SDL_WINDOWEVENT:
                        if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                            // 윈도우 크기 변경 처리는 SDL이 자동으로 처리
                        }
                        break;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }
    
    void handleKeyPress(SDL_Keycode key) {
        switch (key) {
            case SDLK_SPACE:
                paused = !paused;
                std::cout << (paused ? "⏸️  일시정지" : "▶️  재생") << std::endl;
                break;
                
            case SDLK_UP:
                playback_speed = std::min(4.0, playback_speed.load() + 0.25);
                std::cout << "⚡ 재생 속도: " << playback_speed.load() << "x" << std::endl;
                break;
                
            case SDLK_DOWN:
                playback_speed = std::max(0.25, playback_speed.load() - 0.25);
                std::cout << "🐌 재생 속도: " << playback_speed.load() << "x" << std::endl;
                break;
                
            case SDLK_ESCAPE:
            case SDLK_q:
                should_quit = true;
                break;
        }
    }
    
    void cleanup() {
        // 스레드 정리
        should_quit = true;
        queue_condition.notify_all();
        
        if (decoder_thread.joinable()) {
            decoder_thread.join();
        }
        if (render_thread.joinable()) {
            render_thread.join();
        }
        
        // 큐의 남은 프레임들 정리
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!frame_queue.empty()) {
            AVFrame* frame = frame_queue.front();
            frame_queue.pop();
            av_frame_free(&frame);
        }
        
        // SDL 정리
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_Quit();
        
        // SwsContext 정리
        if (sws_ctx) {
            sws_freeContext(sws_ctx);
            sws_ctx = nullptr;
        }
        
        // FFmpeg 정리
        if (video_codec_ctx) {
            avcodec_free_context(&video_codec_ctx);
        }
        if (format_ctx) {
            avformat_close_input(&format_ctx);
        }
        if (hw_device_ctx) {
            av_buffer_unref(&hw_device_ctx);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "🎬 FFmpeg GUI 비디오 플레이어" << std::endl;
        std::cout << "사용법: " << argv[0] << " <비디오_파일>" << std::endl;
        std::cout << "\n예제:" << std::endl;
        std::cout << "  " << argv[0] << " media/samples/h264_sample.mp4" << std::endl;
        return 1;
    }
    
    GUIVideoPlayer player;
    
    if (!player.initialize(argv[1])) {
        std::cerr << "❌ 플레이어 초기화 실패" << std::endl;
        return 1;
    }
    
    player.play();
    
    std::cout << "👋 플레이어 종료" << std::endl;
    return 0;
}
