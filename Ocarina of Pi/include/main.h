#ifndef MAIN_H
#define MAIN_H

#include "video_rpi.h"
#include "audio_alsa.h"
#include "threading.h"
#include "mem_manager.h"

class Main {
public:
    static Main& instance();

    bool initialize(int argc, char** argv);
    void run();
    void shutdown();

private:
    Main() = default;
    ~Main() = default;

    void mainLoopThread();
    void audioThreadThread();
    void gpuSubmitThreadThread();
    void streamingThreadThread();

    bool initSubsystems();
    void gameLoopIteration();

    VideoRpi* video_;
    AudioAlsa* audio_;
    bool running_;
    bool initialized_;
};

#endif
