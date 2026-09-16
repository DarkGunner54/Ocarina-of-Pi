#ifndef MEM_MANAGER_H
#define MEM_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <functional>

struct MemoryPoolConfig {
    size_t total_budget_bytes;
    size_t graphic_memory_bytes;
    size_t audio_memory_bytes;
    size_t streaming_memory_bytes;
    size_t main_memory_bytes;
    bool enable_page_locking;
    bool enable_jpeg_decoder;
};

class MemManager {
public:
    static MemManager& instance();

    bool initialize(const MemoryPoolConfig& config);
    void shutdown();

    void* allocate(size_t size, const std::string& tag);
    void deallocate(void* ptr);

    size_t getAvailableMemory() const;
    size_t getUsedMemory() const;
    size_t getTotalBudget() const;

    void reportMemoryUsage() const;

    static MemManager& get() { return instance(); }

private:
    MemManager() = default;
    ~MemManager() = default;

    MemoryPoolConfig config_;
    size_t used_bytes_;
    void* memory_pool_;

    static constexpr size_t MAX_ALLOCATION = 16 * 1024 * 1024;
};

#endif
