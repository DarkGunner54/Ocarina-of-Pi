#include "mem_manager.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>

MemManager& MemManager::instance() {
    static MemManager inst;
    return inst;
}

bool MemManager::initialize(const MemoryPoolConfig& config) {
    config_ = config;
    used_bytes_ = 0;

    std::cout << "[MemManager] Total budget: " << (config_.total_budget_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "[MemManager] Graphics: " << (config_.graphic_memory_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "[MemManager] Audio: " << (config_.audio_memory_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "[MemManager] Streaming: " << (config_.streaming_memory_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "[MemManager] Main: " << (config_.main_memory_bytes / 1024 / 1024) << " MB" << std::endl;

    if (config_.enable_page_locking) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
            std::cerr << "[MemManager] mlockall failed - memory may be swapped" << std::endl;
        }
    }

    return true;
}

void MemManager::shutdown() {
    if (memory_pool_) {
        free(memory_pool_);
        memory_pool_ = nullptr;
    }
    munlockall();
}

void* MemManager::allocate(size_t size, const std::string& tag) {
    if (size == 0) return nullptr;
    if (size > MAX_ALLOCATION) {
        std::cerr << "[MemManager] Allocation " << tag << " exceeds max: " << size << " > " << MAX_ALLOCATION << std::endl;
        return nullptr;
    }

    if (used_bytes_ + size > config_.total_budget_bytes) {
        std::cerr << "[MemManager] Memory pool exhausted for " << tag
                  << " (used=" << used_bytes_ / 1024 / 1024 << "MB, requested=" << size / 1024 / 1024 << "MB)" << std::endl;
        return nullptr;
    }

    void* ptr = malloc(size);
    if (!ptr) {
        std::cerr << "[MemManager] malloc failed for " << tag << std::endl;
        return nullptr;
    }

    used_bytes_ += size;
    return ptr;
}

void MemManager::deallocate(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

size_t MemManager::getAvailableMemory() const {
    return config_.total_budget_bytes - used_bytes_;
}

size_t MemManager::getUsedMemory() const {
    return used_bytes_;
}

size_t MemManager::getTotalBudget() const {
    return config_.total_budget_bytes;
}

void MemManager::reportMemoryUsage() const {
    std::cout << "[MemManager] Used: " << (used_bytes_ / 1024 / 1024) << " / "
              << (config_.total_budget_bytes / 1024 / 1024) << " MB" << std::endl;
}
