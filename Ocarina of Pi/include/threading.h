#ifndef THREADING_H
#define THREADING_H

#include <pthread.h>
#include <cstdint>
#include <functional>
#include <string>

enum class CoreAffinity {
    MAIN_LOOP = 0,
    AUDIO_MIXER = 1,
    GPU_SUBMIT = 2,
    STREAMING = 3
};

struct ThreadConfig {
    CoreAffinity affinity;
    std::string name;
    int scheduling_priority;
    bool enabled;
};

class Threading {
public:
    static Threading& instance();

    bool initialize(int main_core = 0, int audio_core = 1, int gpu_core = 2, int stream_core = 3);
    void shutdown();

    bool createThread(const std::string& name, CoreAffinity affinity,
                      std::function<void()> entry_point, int priority = 0);

    static void setCoreAffinity(int core);
    static int getCurrentCore();

    static void setRealtimeScheduling(int core, int priority);

    void joinAll();

private:
    Threading() = default;
    ~Threading() = default;

    struct ThreadHandle {
        pthread_t thread;
        pthread_attr_t attributes;
        ThreadConfig config;
        bool joinable;
    };

    ThreadHandle threads_[4];
    int thread_count_;
    bool initialized_;

    static void* threadEntryPoint(void* arg);
};

#endif
