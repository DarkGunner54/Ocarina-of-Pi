#include "threading.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <sched.h>
#include <sys/prctl.h>
#include <unistd.h>

Threading& Threading::instance() {
    static Threading inst;
    return inst;
}

bool Threading::initialize(int main_core, int audio_core, int gpu_core, int stream_core) {
    for (int i = 0; i < 4; i++) {
        threads_[i].joinable = false;
    }
    thread_count_ = 0;
    initialized_ = true;

    setCoreAffinity(main_core);
    std::cout << "[Threading] Main thread pinned to core " << main_core << std::endl;

    return true;
}

void Threading::shutdown() {
    joinAll();
    initialized_ = false;
}

bool Threading::createThread(const std::string& name, CoreAffinity affinity,
                              std::function<void()> entry_point, int priority) {
    if (thread_count_ >= 4) {
        std::cerr << "[Threading] Maximum thread count reached" << std::endl;
        return false;
    }

    ThreadHandle& t = threads_[thread_count_];
    t.config.name = name;
    t.config.affinity = affinity;
    t.config.scheduling_priority = priority;
    t.config.enabled = true;

    pthread_attr_init(&t.attributes);

    int core = static_cast<int>(affinity);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    pthread_attr_setaffinity_np(&t.attributes, sizeof(cpu_set_t), &cpuset);

    struct sched_param sched;
    memset(&sched, 0, sizeof(sched));
    sched.sched_priority = 1 + priority;
    pthread_attr_setschedparam(&t.attributes, &sched);
    pthread_attr_setschedpolicy(&t.attributes, SCHED_OTHER);

    int err = pthread_create(&t.thread, &t.attributes, threadEntryPoint,
                             new std::function<void()>(std::move(entry_point)));
    if (err != 0) {
        std::cerr << "[Threading] Failed to create thread " << name
                  << " on core " << core << ": " << strerror(err) << std::endl;
        pthread_attr_destroy(&t.attributes);
        return false;
    }

    t.joinable = true;
    thread_count_++;
    prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);

    std::cout << "[Threading] Created thread " << name
              << " on core " << core << std::endl;
    return true;
}

void* Threading::threadEntryPoint(void* arg) {
    auto* func = static_cast<std::function<void()>*>(arg);
    setCoreAffinity(static_cast<int>(CoreAffinity::MAIN_LOOP));
    (*func)();
    delete func;
    return nullptr;
}

void Threading::setCoreAffinity(int core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "[Threading] Failed to set affinity to core " << core << std::endl;
    }
}

int Threading::getCurrentCore() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    for (int i = 0; i < 8; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            return i;
        }
    }
    return -1;
}

void Threading::setRealtimeScheduling(int core, int priority) {
    struct sched_param sched;
    sched.sched_priority = priority;
    pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
}

void Threading::joinAll() {
    for (int i = 0; i < thread_count_; i++) {
        if (threads_[i].joinable) {
            pthread_join(threads_[i].thread, nullptr);
            pthread_attr_destroy(&threads_[i].attributes);
            threads_[i].joinable = false;
        }
    }
}
