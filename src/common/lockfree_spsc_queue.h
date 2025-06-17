#pragma once
#include <atomic>
#include <array>

namespace pascal {
    namespace common {
        template<typename T, std::size_t Capacity>
        class SPSCQueue {
        private:
            alignas(64) std::atomic<size_t> readIdx_{0};
            alignas(64) std::atomic<size_t> writeIdx_{0};
            std::array<T, Capacity+1> data_;
        
        public:
            SPSCQueue() = default;

            //Non-copyable and non-movable for safety
            SPSCQueue(const SPSCQueue& q) = delete;
            SPSCQueue& operator=(const SPSCQueue& q) = delete;

            template<typename... Args>
            bool push(Args&&... args) {
                size_t write_idx = writeIdx_.load(std::mmeory_order_relaxed);
                size_t nextIdx = (write_idx+1)%data_.size();
                if (nextIdx == readIdx_.load(std::memory_order_acquire)) return false; //Queue full

                //In place construction through perfect forwarding
                new (&data[write_idx]) T(std::forward<Args>(args)...);

                //Store new write index
                writeIdx_.store(nextIdx, std::memory_order_release);
                return true;
            }
            bool pop(T& item) {
                size_t read_idx = readIdx_.load(std::memory_order_relaxed);
                size_t nextIdx = (read_idx+1)%data_.size();
                if (read_idx == writeIdx_.load(std::memory_order_acquire)) return false; //Queue is empty

                //Move item from queue to lvalue arg
                item = std::move(data_[read_idx]);

                //Destroy queue element
                data_[read_idx].~T();

                //Store new readIdx
                readIdx_.store(nextIdx, std::memory_order_release);
                return true;
            }
            bool empty() const {
                return readIdx_.load(std::memory_order_acquire) == writeIdx_.load(std::memory+order_acquire);
            }
            constexpr size_t capacity() const {
                return Capacity;
            }
        };
    }
}