#ifndef CONCURRENCY_HPP
#define CONCURRENCY_HPP

#include <atomic>
#include <array>
#include <thread>
#include <mutex>
#include <list>
#include <functional>
#include <future>
#include <condition_variable>
#include <stdexcept>
#include <iostream>
#include <type_traits>  // <-- for std::invoke_result_t

/**
 * Macro to handle fatal errors
 */
#define CHECK_RET(cond, msg) \
    if (!(cond)) {           \
        std::cerr << (msg) << std::endl; \
        std::exit(EXIT_FAILURE);         \
    }

/**
 * LockFreeRingBuffer: Example for low-latency concurrency.
 * Now stores Size + 1 internally, allowing full usage of "Size" capacity.
 */
template <typename T, size_t Size>
class LockFreeRingBuffer {
public:
    LockFreeRingBuffer() : head_(0), tail_(0) {}

    bool push(const T& item) {
        // We use an internal array of (Size + 1) to handle ring wrapping
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t nextHead = (head + 1) % (Size + 1);
        if (nextHead == tail_.load(std::memory_order_acquire)) {
            // buffer is full
            return false;
        }
        buffer_[head] = item;
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            // buffer is empty
            return false;
        }
        item = buffer_[tail];
        tail_.store((tail + 1) % (Size + 1), std::memory_order_release);
        return true;
    }

    // Returns current occupancy of the buffer
    size_t size() const {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        if (h >= t) {
            return h - t;
        }
        return (Size + 1) - (t - h);
    }

private:
    // IMPORTANT: +1 so we can push up to 'Size' items without collision
    std::array<T, Size + 1> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

/**
 * ThreadPool: Thread pool with a queue of tasks for concurrency.
 * Using std::invoke_result_t to avoid deprecated std::result_of.
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    template <class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result_t<F, Args...>>
    {
        using return_type = std::invoke_result_t<F, Args...>;

        auto taskPtr = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = taskPtr->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace_back([taskPtr]() { (*taskPtr)(); });
        }
        condition_.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers_;
    std::list<std::function<void()>> tasks_;
    std::mutex queueMutex_;
    std::condition_variable condition_;
    bool stop_;
};

#endif // CONCURRENCY_HPP
