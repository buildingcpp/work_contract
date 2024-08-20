#pragma once

#include "./work_contract_id.h"

#include <include/signal_tree.h>
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
#include <bit>


namespace bcpp::implementation
{

    template <synchronization_mode> class work_contract;


    template <synchronization_mode T>
    class work_contract_tree final :
        non_copyable,
        non_movable
    {
    public:

        //============================================================================= 
        template <std::uint64_t total_counters, std::uint64_t bits_per_counter>
        struct largest_child_selector
        {
            inline auto operator()
            (
                std::uint64_t biasFlags,
                std::uint64_t counters
            ) const noexcept -> signal_index
            {
                if constexpr (bits_per_counter == 1)
                {
                    return (counters > 0) ? std::countl_zero(counters) : ~0ull;
                }
                else
                {
                    // this routine is only called to select new contract ids.  but we could improve speed here
                    // with a expression fold as total counters is never more than 8 and often 4 or 2.
                    auto selected = ~0ull;
                    auto max = 0ull;
                    static auto constexpr counter_mask = ((1ull << bits_per_counter) - 1);
                    for (auto i = 0ull; i < total_counters; ++i)
                    {
                        if ((counters & counter_mask) > max)
                        {
                            max = (counters & counter_mask);
                            selected = i;
                        }
                        counters >>= bits_per_counter;
                    }
                    return selected;
                }
            }
        };

        static auto constexpr mode = T;
        using work_contract_type = work_contract<mode>;

        static auto constexpr default_capacity = 512;

        class release_token;
        class work_contract_token;
        class exception_token;

        work_contract_tree();

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
            std::invocable<work_contract_token &> auto &&,
            std::invocable auto &&,
            std::invocable<exception_token &> auto &&,
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

        work_contract_type create_contract
        (
            std::invocable auto &&,
            std::invocable auto &&,
            std::invocable<exception_token &> auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        bool execute_next_contract();
        
        template <typename rep, typename period>
        bool execute_next_contract
        (
            std::chrono::duration<rep, period> duration
        ) requires (mode == synchronization_mode::blocking);

        void stop();

    private:

        class auto_erase_contract;
        
        friend class work_contract<mode>;
        friend class release_token;
        friend class exception_token;
        friend class work_contract_token;
        friend class auto_erase_contract;

        using state_flags = std::int8_t;

        struct /*alignas(64)*/ contract
        {
            static auto constexpr release_flag      = 0x00000004;
            static auto constexpr execute_flag      = 0x00000002;
            static auto constexpr schedule_flag     = 0x00000001;
        
            std::atomic<state_flags>                    flags_;
            std::function<void(work_contract_token &)>  work_;
        };

        void schedule
        (
            work_contract_id 
        );

        void release
        (
            work_contract_id 
        );        
        
        void set_contract_signal
        (
            work_contract_id
        );

        void process_release(work_contract_id);

        void process_contract(work_contract_id);

        void process_exception(work_contract_id, std::exception_ptr);

        void clear_execute_flag
        (
            work_contract_id
        );

        void clear_invocation_count
        (
            work_contract_id,
            bool
        );

        void erase_contract
        (
            work_contract_id
        );

        work_contract_id get_available_contract();

        std::tuple<std::uint64_t, std::uint64_t> get_tree_and_signal_index
        (
            work_contract_id
        ) const;

        using signal_tree_type = bcpp::signal_tree<512>;
        static auto constexpr signal_tree_capacity = signal_tree_type::capacity;
        static auto constexpr bias_shift = (64 - minimum_bit_count(signal_tree_capacity - 1)); 

        std::uint64_t                                                   subTreeCount_;

        std::uint64_t                                                   subTreeMask_;

        std::uint64_t                                                   subTreeShift_;

        std::vector<contract>                                           contracts_;

        std::vector<std::function<void()>>                              release_;

        std::vector<std::function<void(exception_token &)>>             exception_;

        std::vector<std::shared_ptr<release_token>>                     releaseToken_;

        std::mutex                                                      mutex_;

        std::atomic<bool>                                               stopped_{false};

        std::vector<signal_tree_type>                                   signalTree_;

        std::vector<signal_tree_type>                                   available_;


        static thread_local std::uint64_t tls_biasFlags;

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
                return false;  // TODO: restore 
                /*
                if (owner->signalTree_.empty())
                {
                    std::unique_lock uniqueLock(mutex_);
                    conditionVariable_.wait(uniqueLock, [owner](){return ((!owner->signalTree_.empty()) || (owner->stopped_));});
                    return (!owner->stopped_);
                }
                return true;
                */
            }

            bool wait_for
            (
                work_contract_tree const * owner,
                std::chrono::nanoseconds duration
            ) const
            {                
                return false;  // TODO: restore 
                /*
                if (owner->signalTree_.empty())
                {
                    std::unique_lock uniqueLock(mutex_);
                    auto waitSuccess = conditionVariable_.wait_for(uniqueLock, duration, [owner]() mutable{return ((!owner->signalTree_.empty()) || (owner->stopped_));});
                    return ((!owner->stopped_) && (waitSuccess));
                }
                return true;
                */
            }

        } waitableState_;

    }; // class work_contract_tree


    template <synchronization_mode T>
    thread_local std::uint64_t work_contract_tree<T>::tls_biasFlags{0};


    //=========================================================================
    template <synchronization_mode T>
    class work_contract_tree<T>::release_token final :
        non_copyable,
        non_movable
    {
    public:
        release_token() = delete;
        release_token(work_contract_tree *);
        bool schedule(work_contract_type const &);
        void orphan();
        bool is_valid() const;
        std::mutex mutable      mutex_;
        work_contract_tree *    workContractTree_{};
    }; // class work_contract_tree<>::release_token


    //=========================================================================
    template <synchronization_mode T>
    class work_contract_tree<T>::work_contract_token final :
        non_movable,
        non_copyable
    {
    public:
        void schedule();
        void release();
    private:
        friend work_contract_tree<T>;
        work_contract_token() = delete;
        work_contract_token(work_contract_id, work_contract_tree &);
        ~work_contract_token();        
        void clear_execute_flag();
        work_contract_id            contractId_{~0ull};
        work_contract_tree<T> &     owner_;
        bool                        released_{false};
    }; // class work_contract_tree<>::work_contract_token


    //=========================================================================
    template <synchronization_mode T>
    class work_contract_tree<T>::exception_token final :
        non_movable,
        non_copyable
    {
    public:
        ~exception_token() = default;
        void release();
        void schedule();
        work_contract_id get_contract_id() const;
        std::exception_ptr get_exception() const;
    private:
        friend work_contract_tree<T>;
        exception_token() = delete;
        exception_token(work_contract_id, std::exception_ptr, work_contract_tree &);
        work_contract_id            contractId_{~0ull};
        std::exception_ptr          exception_;
        work_contract_tree<T> &     owner_;
    }; // class work_contract_tree<>::exception_token


} // namespace bcpp::implementation


namespace bcpp
{
    //=========================================================================
    using blocking_work_contract_tree = implementation::work_contract_tree<synchronization_mode::blocking>;
    using work_contract_tree = implementation::work_contract_tree<synchronization_mode::non_blocking>;

} // namespace bcpp


#include "./work_contract.h"


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable<work_contract_token &> auto && workFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), [](){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable<work_contract_token &> auto && workFunction,
    std::invocable auto && releaseFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{    
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = std::forward<decltype(workFunction)>(workFunction); 
        release_[workContractId] = std::forward<decltype(releaseFunction)>(releaseFunction); 
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable<work_contract_token &> auto && workFunction,
    std::invocable auto && releaseFunction,
    std::invocable<exception_token &> auto && exceptionFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{    
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = std::forward<decltype(workFunction)>(workFunction); 
        release_[workContractId] = std::forward<decltype(releaseFunction)>(releaseFunction); 
        exception_[workContractId] = std::forward<decltype(exceptionFunction)>(exceptionFunction);
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable auto && workFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), [](){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = [work = std::forward<decltype(workFunction)>(workFunction)](auto &) mutable{work();}; 
        release_[workContractId] = std::forward<decltype(releaseFunction)>(releaseFunction);
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction,
    std::invocable<exception_token &> auto && exceptionFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = [work = std::forward<decltype(workFunction)>(workFunction)](auto &) mutable{work();}; 
        release_[workContractId] = std::forward<decltype(releaseFunction)>(releaseFunction);
        exception_[workContractId] = std::forward<decltype(exceptionFunction)>(exceptionFunction);
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_tree<T>::get_tree_and_signal_index
(
    work_contract_id workContractId
) const -> std::tuple<std::uint64_t, std::uint64_t> 
{
    return {workContractId / signal_tree_capacity, workContractId % signal_tree_capacity};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::release
(
    work_contract_id contractId
)
{
    static auto constexpr flags_to_set = (contract::release_flag | contract::schedule_flag);
    auto previousFlags = contracts_[contractId].flags_.fetch_or(flags_to_set);
    auto notScheduledNorExecuting = ((previousFlags & (contract::schedule_flag | contract::execute_flag)) == 0);
    if (notScheduledNorExecuting)
        set_contract_signal(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::schedule
(
    // set the schedule flag.  if not previously set, and not currently executing
    // then also set the signal associated with the contract.
    work_contract_id contractId
)
{
    static auto constexpr flags_to_set = contract::schedule_flag;
    auto previousFlags = contracts_[contractId].flags_.fetch_or(flags_to_set);
    auto notScheduledNorExecuting = ((previousFlags & (contract::schedule_flag | contract::execute_flag)) == 0);
    if (notScheduledNorExecuting)
        set_contract_signal(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::set_contract_signal
(
    // set the signal that is associated with the specified contract
    work_contract_id contractId
)
{
    auto [treeIndex, signalIndex] = get_tree_and_signal_index(contractId);
    signalTree_[treeIndex].set(signalIndex);
    if constexpr (mode == synchronization_mode::blocking)
    {
        // this should be done more graceful but for now ...
         waitableState_.notify_all();
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::implementation::work_contract_tree<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
) 
{
    if constexpr (mode == synchronization_mode::blocking)
    {
        if (!waitableState_.wait(this))// this should be done more graceful but for now ..
            return false;
    }

    auto biasFlags = tls_biasFlags++;
    auto subTreeIndex = (biasFlags & subTreeMask_);
    biasFlags >>= subTreeShift_; // these two shifts should be combined
    biasFlags <<= bias_shift;

    for (auto i = 0; i < signalTree_.size(); ++i)
    {
        if (auto signalIndex = signalTree_[subTreeIndex].select(biasFlags); signalIndex != invalid_signal_index)
        {
            work_contract_id workContractId((subTreeIndex) * signal_tree_capacity);
            workContractId |= signalIndex;
         //   tls_biasFlags = ((signalIndex) << subTreeShift_) | ((subTreeIndex + 1) & subTreeMask_);//(workContractId + 1);
            process_contract(work_contract_id(workContractId));
            return true;
        }
        ++subTreeIndex;
        subTreeIndex &= subTreeMask_;
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::process_contract
(
    work_contract_id contractId
)
{
    auto & contract = contracts_[contractId];
    auto flags = ++contract.flags_;

    if (auto isReleased = ((flags & contract::release_flag) == contract::release_flag); isReleased)
    {
        // release should be far less common path so ensure not inlined 
        process_release(contractId);
        return;
    }
    
    // the expected case. invoke the work function
    work_contract_token token(contractId, *this); // RAII to clean up in case of exception
    try
    {
        contract.work_(token);
    }
    catch (std::exception const & exception)
    {
        process_exception(contractId, std::current_exception());
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
template <typename rep, typename period>
inline bool bcpp::implementation::work_contract_tree<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
    std::chrono::duration<rep, period> duration
) requires (mode == synchronization_mode::blocking)
{
    if (waitableState_.wait_for(this, duration))    // this should be done more graceful but for now ..
    {
        auto biasFlags = tls_biasFlags++;
        auto subTreeIndex = (biasFlags & subTreeMask_);
        biasFlags >>= subTreeShift_; // these two shifts should be combined
        biasFlags <<= bias_shift;

        for (auto i = 0; i < signalTree_.size(); ++i)
        {
            if (auto signalIndex = signalTree_[subTreeIndex & subTreeMask_].select(biasFlags); signalIndex != invalid_signal_index)
            {
                work_contract_id workContractId((subTreeIndex & subTreeMask_) * signal_tree_capacity);
                workContractId |= signalIndex;
            //    tls_biasFlags = (workContractId + 1);
                process_contract(workContractId);
                return true;
            }
            ++subTreeIndex;
        }
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::clear_execute_flag
(
    work_contract_id contractId
)
{
    if (((contracts_[contractId].flags_ -= contract::execute_flag) & contract::schedule_flag) == contract::schedule_flag)
        set_contract_signal(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract_tree<T>::work_contract_token::work_contract_token 
(
    work_contract_id contractId,
    work_contract_tree<T> & owner
):
    contractId_(contractId),
    owner_(owner),
    released_(false)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract_tree<T>::work_contract_token::~work_contract_token
(
)
{
    owner_.clear_execute_flag(contractId_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::work_contract_token::schedule
(
)
{
    if (!released_)
        owner_.schedule(contractId_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_tree<T>::work_contract_token::release
(
)
{
    if (!released_)
        owner_.release(contractId_);
    released_ = true;
}
