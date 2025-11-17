#pragma once

#include <atomic>
#include <memory>

#if !defined(__cpp_lib_atomic_shared_ptr) || __cpp_lib_atomic_shared_ptr < 201711L
#include <mutex>

namespace bcpp::detail {

    template<typename T>
    class atomic_shared_ptr {
        std::shared_ptr<T> ptr_;
        mutable std::mutex mtx_;
    public:
        atomic_shared_ptr() = default;
        atomic_shared_ptr(std::shared_ptr<T> p) : ptr_(std::move(p)) {}

        std::shared_ptr<T> load() const {
            std::lock_guard<std::mutex> lock(mtx_);
            return ptr_;
        }

        std::shared_ptr<T> exchange(std::shared_ptr<T> desired) {
            std::lock_guard<std::mutex> lock(mtx_);
            std::shared_ptr<T> old = std::move(ptr_);
            ptr_ = std::move(desired);
            return old;
        }

        void store(std::shared_ptr<T> desired) {
            std::lock_guard<std::mutex> lock(mtx_);
            ptr_ = std::move(desired);
        }
    };

} // namespace bcpp::detail
#else

namespace bcpp::detail {
    template<typename T>
    using atomic_shared_ptr = std::atomic<std::shared_ptr<T>>;
}
#endif
