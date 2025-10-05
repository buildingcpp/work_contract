#pragma once

#include <thread>

#if not defined(__cpp_lib_jthread) || __cpp_lib_jthread < 201911L
    #include <atomic>
    #include <functional>

namespace bcpp::detail {

// Simple stop_token implementation
class stop_token
{
    std::atomic<bool> * stop_flag_;

public:
    explicit stop_token(std::atomic<bool> * flag)
    : stop_flag_(flag)
    { }

    bool stop_requested() const noexcept { return stop_flag_->load(); }
};

// Simple jthread implementation with stop_token support
class jthread
{
    struct Impl
    {
        std::thread thread;
        std::atomic<bool> stop_flag{false};
    };

    std::unique_ptr<Impl> impl_;

public:
    jthread() = default;

    template <typename F, typename... Args>
    explicit jthread(F && f, Args &&... args)
    : impl_(std::make_unique<Impl>())
    {
        // Check if callable accepts stop_token as first argument
        if constexpr (std::is_invocable_v<F, stop_token, Args...>) {
            impl_->thread = std::thread(
                std::forward<F>(f),
                stop_token(&impl_->stop_flag),
                std::forward<Args>(args)...);
        } else {
            impl_->thread = std::thread(
                std::forward<F>(f),
                std::forward<Args>(args)...);
        }
    }

    jthread(jthread &&) = default;
    jthread & operator = (jthread &&) = default;

    ~jthread()
    {
        if (impl_ && impl_->thread.joinable()) {
            request_stop();
            join();
        }
    }

    void request_stop() noexcept
    {
        if (impl_) {
            impl_->stop_flag.store(true);
        }
    }

    void join()
    {
        if (impl_ && impl_->thread.joinable()) {
            impl_->thread.join();
        }
    }

    bool joinable() const noexcept { return impl_ && impl_->thread.joinable(); }
};

} // namespace bcpp::detail
#else
    #include <stop_token>

namespace bcpp::detail {
using jthread = std::jthread;
using stop_token = std::stop_token;
}
#endif
