#pragma once

#include <include/signal_tree.h>
#include "./signal_map.h"
#include <include/synchronization_mode.h>
#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <concepts>


namespace bcpp 
{

    template <synchronization_mode> class work_contract;


    template <synchronization_mode T>
    class work_contract_tree :
        non_copyable,
        non_movable
    {
    public:

        static auto constexpr mode = T;
        using work_contract_type = work_contract<mode>;

        class release_token;
        class work_contract_token;

        work_contract_tree
        (
            std::uint64_t
        );

        ~work_contract_tree();

        work_contract_type create_contract
        (
            std::invocable<work_contract_token &> auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        work_contract_type create_contract
        (
            std::invocable<work_contract_token &> auto &&,
            std::invocable auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        work_contract_type create_contract
        (
            std::invocable auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        work_contract_type create_contract
        (
            std::invocable auto &&,
            std::invocable auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        bool execute_next_contract();
        
        template <typename rep, typename period>
        bool execute_next_contract
        (
            std::chrono::duration<rep, period> duration
        ) requires (mode == synchronization_mode::blocking);

        void stop();

 //   private:

        class auto_erase_contract;
        
        friend class work_contract<mode>;
        friend class release_token;
        friend class work_contract_token;
        friend class auto_erase_contract;

        using state_flags = std::int8_t;

        struct alignas(64) contract
        {
            static auto constexpr release_flag  = 0x00000004;
            static auto constexpr execute_flag  = 0x00000002;
            static auto constexpr schedule_flag = 0x00000001;
        
            std::atomic<state_flags>                    flags_;
            std::function<void(work_contract_token &)>  work_;
            std::function<void()>                       release_;
        };

        void schedule
        (
            std::uint64_t 
        );

        void release
        (
            std::uint64_t 
        );        
        
        template <std::size_t>
        void set_contract_flag
        (
            std::uint64_t 
        );

        std::size_t process_contract();

        void process_release(std::uint64_t);

        void process_contract(std::uint64_t);

        void clear_execute_flag
        (
            std::uint64_t
        );

        void erase_contract
        (
            std::uint64_t
        );

        std::vector<contract>                           contracts_;

        std::vector<std::shared_ptr<release_token>>     releaseToken_;

        std::mutex                                      mutex_;

        std::atomic<bool>                               stopped_{false};

        static auto constexpr subtree_count = 512;
        static auto constexpr signal_capacity = 32768;

        signal_tree<signal_capacity, subtree_count>                      tree_;

        signal_tree<signal_capacity, subtree_count>                      available_;

        struct 
        {
            std::mutex mutable              mutex_;
            std::condition_variable mutable conditionVariable_;

            void notify_all()
            {
                std::lock_guard lockGuard(mutex_);
                conditionVariable_.notify_all();
            }

            bool wait(work_contract_tree const * owner) const
            {
                if (owner->tree_.empty())
                {
                    std::unique_lock uniqueLock(mutex_);
                    conditionVariable_.wait(uniqueLock, [owner](){return ((!owner->tree_.empty()) || (owner->stopped_));});
                    return (!owner->stopped_);
                }
                return true;
            }

            bool wait_for
            (
                work_contract_tree const * owner,
                std::chrono::nanoseconds duration
            ) const
            {
                if (owner->tree_.empty())
                {
                    std::unique_lock uniqueLock(mutex_);
                    auto waitSuccess = conditionVariable_.wait_for(uniqueLock, duration, [owner]() mutable{return ((!owner->tree_.empty()) || (owner->stopped_));});
                    return ((!owner->stopped_) && (waitSuccess));
                }
                return true;
            }

        } waitableState_;

    }; // class work_contract_tree


    template <synchronization_mode T>
    class work_contract_tree<T>::release_token
    {
    public:

        release_token(work_contract_tree *);
        bool schedule(work_contract_type const &);
        void orphan();
        bool is_valid() const;
        std::mutex mutable      mutex_;
        work_contract_tree *    workContractTree_{};
    };


    template <synchronization_mode T>
    class work_contract_tree<T>::work_contract_token :
        non_movable,
        non_copyable
    {
    public:
        work_contract_token(std::uint64_t, work_contract_tree &);
        ~work_contract_token();
        void schedule();
    private:
        std::uint64_t               contractId_;
        work_contract_tree<T> &     owner_;
    };

} // namespace bcpp


#include "./work_contract.h"


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract_tree<T>::create_contract
(
    std::invocable<work_contract_token &> auto && workFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), [](){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract_tree<T>::create_contract
(
    std::invocable<work_contract_token &> auto && workFunction,
    std::invocable auto && releaseFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    if (auto contractId = available_.select(); contractId != ~0)
    {
        auto & contract = contracts_[contractId];
        contract.flags_ = 0;
        contract.work_ = std::forward<decltype(workFunction)>(workFunction); 
        contract.release_ = std::forward<decltype(releaseFunction)>(releaseFunction); 
        return {this, releaseToken_[contractId] = std::make_shared<release_token>(this), contractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract_tree<T>::create_contract
(
    std::invocable auto && workFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), [](){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract_tree<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    if (auto contractId = available_.select(); contractId != ~0)
    {
        auto & contract = contracts_[contractId];
        contract.flags_ = 0;
        contract.work_ = [work = std::forward<decltype(workFunction)>(workFunction)](auto &) mutable{work();}; 
        contract.release_ = std::forward<decltype(releaseFunction)>(releaseFunction);
        return {this, releaseToken_[contractId] = std::make_shared<release_token>(this), contractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::release
(
    std::uint64_t contractId
)
{
    set_contract_flag<contract::release_flag | contract::schedule_flag>(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::schedule
(
    std::uint64_t contractId
)
{
    set_contract_flag<contract::schedule_flag>(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
template <std::size_t flags_to_set>
inline void bcpp::work_contract_tree<T>::set_contract_flag
(
    std::uint64_t contractId
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::schedule_flag);
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
    {
        tree_.set(contractId);
        if constexpr (mode == synchronization_mode::blocking)
        {
            // this should be done more graceful but for now ...
            waitableState_.notify_all();
        }
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::work_contract_tree<T>::execute_next_contract
(
) 
{
    if constexpr (mode == synchronization_mode::blocking)
    {
        if (!waitableState_.wait(this))// this should be done more graceful but for now ..
            return false;
    }
    if (auto contractId = tree_.select(); contractId != ~0)
    {
        process_contract(contractId);
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
template <typename rep, typename period>
inline bool bcpp::work_contract_tree<T>::execute_next_contract
(
    std::chrono::duration<rep, period> duration
) requires (mode == synchronization_mode::blocking)
{
    if (waitableState_.wait_for(this, duration))    // this should be done more graceful but for now ..
    {
        if (auto contractId = tree_.select(); contractId != ~0)
        {
            process_contract(contractId);
            return true;
        }
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::clear_execute_flag
(
    std::uint64_t contractId
)
{
    if (((contracts_[contractId].flags_ -= contract::execute_flag) & contract::schedule_flag) == contract::schedule_flag)
        tree_.set(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::process_contract
(
    std::uint64_t contractId
)
{
    auto & contract = contracts_[contractId];
    if ((++contract.flags_ & contract::release_flag) != contract::release_flag)
    {
        work_contract_token token(contractId, *this);
        contract.work_(token);
    }
    else
    {
        process_release(contractId);
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract_tree<T>::work_contract_token::work_contract_token 
(
    std::uint64_t contractId,
    work_contract_tree<T> & owner
):
    contractId_(contractId),
    owner_(owner)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract_tree<T>::work_contract_token::~work_contract_token
(
)
{
    owner_.clear_execute_flag(contractId_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::work_contract_token::schedule
(
)
{
    owner_.schedule(contractId_);
}
