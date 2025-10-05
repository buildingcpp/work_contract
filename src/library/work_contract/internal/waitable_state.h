#pragma once

#include <mutex>
#include <condition_variable>


namespace bcpp::internal::work_contract
{

    class waitable_state 
    {
    public:
    
        bool wait() const
        {
            if (nonZeroCounter_ == 0)
            {
                std::unique_lock uniqueLock(mutex_);
                conditionVariable_.wait(uniqueLock, [this](){return (nonZeroCounter_ != 0);});
            }
            return true;
        }

        bool wait_for
        (
            std::chrono::nanoseconds duration
        ) const
        {                
            if (nonZeroCounter_ == 0)
            {
                std::unique_lock uniqueLock(mutex_);
                auto waitSuccess = conditionVariable_.wait_for(uniqueLock, duration, [this]() mutable{return (nonZeroCounter_ != 0);});
                return waitSuccess;
            }
            return true;
        }

        void increment_non_zero_counter
        (
        )
        {
            if (nonZeroCounter_++ == 0)
            {
                std::lock_guard lockGuard(mutex_);
                conditionVariable_.notify_all();
            }
        }


        void decrement_non_zero_counter
        (
        )
        {
            --nonZeroCounter_;
        }
        
    private:

        std::mutex mutable              mutex_;
        std::condition_variable mutable conditionVariable_;
        std::atomic<std::int64_t>       nonZeroCounter_{0};

    }; // waitable_state

} // namespace bcpp::internal::work_contract



