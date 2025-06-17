#pragma once
#include <atomic>
#include <array>

namespace pascal {
    namespace common {
        template<typename T, std::size_t Size>
        class SPSCQueue {
        private:
            alignas(64) std::atomic<size_t> readIdx_{0};
            alignas(64) std::atomic<size_t> writeIdx_{0};
            std::array<T, Size> data_;
        
        public:
            SPSCQueue(size_t size) {
                data_ = std::array<T, size>();
            }
            void push(const T& val) {
                size_t wIdx_ = writeIdx_.load(std::memory_order_relaxed);
                size_t nextIdx = wIdx_+1;
                if (nextIdx == data_.size()) nextIdx = 0;
                //Writer is non-blocking, always succeeds
                data_[nextIdx] = std::move(val);
                writeIdx_.store(nextIdx, std::memory_order_release);
            }
            bool pop(T& val) {
                size_t rIdx_ = read_idx_.load(std::memory_order_relaxed);
                size_t nextIdx = rIdx_+1;
                if (nextIdx_ == data.size()) nextIdx = 0;
                if (nextIdx == writeIdx_.load(std::memory_order_acquire)) {
                    return false;
                }
                val = std::move(data[nextIdx]);
                readIdx_.store(nextIdx, std::memory_order_release);
                return true;
            }
        };
    }
}