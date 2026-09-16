#include "main.h"
#include "game_input.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <sys/prctl.h>

static Main* g_main_instance = nullptr;

static void signalHandler(int sig) {
    if (g_main_instance) {
        g_main_instance->shutdown();
    }
}

Main& Main::instance() {
    static Main inst;
    return inst;
}

bool Main::initialize(int argc, char** argv) {
    running_ = false;
    initialized_ = false;
    g_main_instance = this;

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    MemoryPoolConfig mem_config;
    mem_config.total_budget_bytes = 768 * 1024 * 1024;
    mem_config.graphic_memory_bytes = 128 * 1024 * 1024;
    mem_config.audio_memory_bytes = 16 * 1024 * 1024;
    mem_config.streaming_memory_bytes = 64 * 1024 * 1024;
    mem_config.main_memory_bytes = 560 * 1024 * 1024;
    mem_config.enable_page_locking = true;
    mem_config.enable_jpeg_decoder = true;

    if (!MemManager::instance().initialize(mem_config)) {
        std::cerr << "[Main] Memory manager init failed" << std::endl;
        return false;
    }

    VideoRpi::Config video_config;
    video_config.target_width = 320;
    video_config.target_height = 240;
    video_config.window_width = 640;
    video_config.window_height = 480;
    video_config.fullscreen = true;
    video_config.integer_scaling = true;
    video_config.vsync_enabled = false;

    video_ = new VideoRpi(video_config);
    if (!video_->initialize()) {
        std::cerr << "[Main] Video initialization failed" << std::endl;
        return false;
    }

    AudioAlsa::Config audio_config;
    audio_config.device = "hw:0,0";
    audio_config.sample_rate = 48000;
    audio_config.channels = 2;
    audio_config.format = SND_PCM_FORMAT_S16_LE;
    audio_config.buffer_frames = 1024;
    audio_config.period_frames = 256;
    audio_config.software_resample = true;

    audio_ = new AudioAlsa(audio_config);
    if (!audio_->initialize()) {
        std::cerr << "[Main] Audio initialization failed" << std::endl;
        return false;
    }

    Threading::instance().initialize(0, 1, 2, 3);

    if (!GameInput::instance().initialize()) {
        std::cerr << "[Main] Game input initialization failed" << std::endl;
        return false;
    }

    Threading::instance().createThread("main_loop", CoreAffinity::MAIN_LOOP,
        [this]() { mainLoopThread(); }, 0);

    Threading::instance().createThread("audio_mixer", CoreAffinity::AUDIO_MIXER,
        [this]() { audioThreadThread(); }, 1);

    Threading::instance().createThread("gpu_submit", CoreAffinity::GPU_SUBMIT,
        [this]() { gpuSubmitThreadThread(); }, 2);

    Threading::instance().createThread("streaming", CoreAffinity::STREAMING,
        [this]() { streamingThreadThread(); }, 3);

    audio_->start();
    initialized_ = true;

    std::cout << "[Main] All subsystems initialized successfully" << std::endl;
    return true;
}

void Main::run() {
    if (!initialized_) return;
    running_ = true;
    std::cout << "[Main] Starting main loop" << std::endl;

    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Main::shutdown() {
    if (!running_) return;
    std::cout << "[Main] Shutting down..." << std::endl;
    running_ = false;

    audio_->drain();
    Threading::instance().joinAll();

    delete audio_;
    delete video_;

    MemManager::instance().shutdown();

    GameInput::instance().shutdown();

    std::cout << "[Main] Shutdown complete" << std::endl;
}

void Main::mainLoopThread() {
    Threading::setCoreAffinity(0);
    prctl(PR_SET_NAME, "main_loop", 0, 0, 0);

    auto last_frame = std::chrono::steady_clock::now();

    while (running_) {
        GameInput::instance().poll();

        if (GameInput::instance().hasController()) {
            auto ctrl = GameInput::instance().getState();
            auto cs = GameInput::instance().getControlStick();
            auto cb = GameInput::instance().getCButtons();
            auto dp = GameInput::instance().getDPad();
            static int frame_count = 0;
            frame_count++;
            if (frame_count % 300 == 0) {
                std::cout << "[Input] CTRL: L" << cs.x << "," << cs.y
                          << " R" << ctrl.c_stick_raw.x << "," << ctrl.c_stick_raw.y
                          << " BTN: " << std::hex << ctrl.buttons << std::dec
                          << " D-Pad: " << (dp.up ? "U" : "-") << (dp.down ? "D" : "-")
                          << (dp.left ? "L" : "-") << (dp.right ? "R" : "-")
                          << " C-btn: " << (cb.up ? "U" : "-") << (cb.down ? "D" : "-")
                          << (cb.left ? "L" : "-") << (cb.right ? "R" : "-")
                          << std::endl;
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame).count();
        uint32_t n64_buttons = GameInput::instance().getN64Buttons();

        gameLoopIteration();

        last_frame = now;
    }
}

void Main::audioThreadThread() {
    Threading::setCoreAffinity(1);
    prctl(PR_SET_NAME, "audio_mixer", 0, 0, 0);

    static const int BUFFER_FRAMES = 256;
    static const int BYTES_PER_FRAME = 4;
    uint8_t audio_buffer[BUFFER_FRAMES * BYTES_PER_FRAME];

    while (running_) {
        memset(audio_buffer, 0, sizeof(audio_buffer));

        audio_->writeInterleaved(audio_buffer, BUFFER_FRAMES);
    }
}

void Main::gpuSubmitThreadThread() {
    Threading::setCoreAffinity(2);
    prctl(PR_SET_NAME, "gpu_submit", 0, 0, 0);

    while (running_) {
        if (video_->beginFrame()) {
            video_->endFrame();
        }
        std::this_thread::yield();
    }
}

void Main::streamingThreadThread() {
    Threading::setCoreAffinity(3);
    prctl(PR_SET_NAME, "streaming", 0, 0, 0);

    while (running_) {
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void Main::gameLoopIteration() {
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, 320, 240);

    video_->endFrame();
}

int main(int argc, char** argv) {
    Main& main = Main::instance();
    if (!main.initialize(argc, argv)) {
        return 1;
    }
    main.run();
    main.shutdown();
    return 0;
}
