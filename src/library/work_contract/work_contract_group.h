#pragma once

#include "./internal/shared_state.h"
#include "./internal/this_contract.h"
#include "./work_contract.h"

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
#include <iostream>


namespace bcpp::implementation
{

    template <synchronization_mode> class work_contract;


    template <synchronization_mode T>
    class work_contract_group final :
        non_copyable,
        non_movable
    {
    public:

        static auto constexpr mode = T;
        using work_contract_type = work_contract<mode>;

        static auto constexpr default_capacity = 512;

        // internal signal tree capacity can be tuned for different performance needs
        static auto constexpr minimum_latency_signal_tree_capacity = 64;
        static auto constexpr general_purpose_signal_tree_capacity = 512;
        static auto constexpr default_signal_tree_capacity = minimum_latency_signal_tree_capacity;

        using signal_tree_type = bcpp::signal_tree<default_signal_tree_capacity>;
        static auto constexpr signal_tree_capacity = signal_tree_type::capacity;

        using shared_state_type = internal::work_contract::shared_state<mode, signal_tree_capacity>;
        using shared_segment_type = typename shared_state_type::segment_type;

        using wc_id = internal::work_contract::work_contract_id<signal_tree_capacity>;
        using segment_index = typename wc_id::segment_index;
        using contract_index = typename wc_id::contract_index;

        work_contract_group();

        work_contract_group
        (
            std::uint64_t
        );

        ~work_contract_group();

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
            std::invocable<std::exception_ptr> auto &&,
            work_contract_type::initial_state = work_contract_type::initial_state::unscheduled
        );

        std::uint64_t execute_next_contract();

        std::uint64_t execute_next_contract
        (
            std::uint64_t & 
        );
        
        template <typename rep, typename period>
        std::uint64_t execute_next_contract
        (
            std::chrono::duration<rep, period>
        ) requires (mode == synchronization_mode::blocking);

        template <typename rep, typename period>
        std::uint64_t execute_next_contract
        (
            std::chrono::duration<rep, period>,
            std::uint64_t &
        ) requires (mode == synchronization_mode::blocking);

        void stop();

    private:

        class auto_erase_contract;
        class auto_clear_execute_flag;

        void process_release(wc_id);

        void process_contract(wc_id);

        void process_exception(wc_id, std::exception_ptr);

        void erase_contract
        (
            wc_id
        ) noexcept;

        wc_id get_available_contract();

        std::shared_ptr<shared_state_type>                              sharedStates_;
        
        std::uint64_t                                                   subTreeCount_;

        std::uint64_t                                                   subTreeMask_;

        std::vector<signal_tree_type>                                   available_;

        std::vector<std::function<void()>>                              work_;
        std::vector<std::function<void()>>                              release_;
        std::vector<std::function<void(std::exception_ptr)>>            exception_;

        std::mutex                                                      mutex_;

        std::atomic<bool>                                               stopped_{false};

        std::atomic<std::uint64_t>                                      nextAvailableTreeIndex_{0};

        static thread_local std::uint64_t                               tls_biasFlags_;
/*
        std::atomic<std::int64_t>                                       nonZeroCounter_{0};


        void decrement_non_zero_counter() requires (mode == synchronization_mode::blocking);
        void increment_non_zero_counter() requires (mode == synchronization_mode::blocking);

        struct 
        {
            std::mutex mutable              mutex_;
            std::condition_variable mutable conditionVariable_;

            void notify_all()
            {
                std::lock_guard lockGuard(mutex_);
                conditionVariable_.notify_all();
            }

            bool wait(work_contract_group const * owner) const
            {
                if (owner->nonZeroCounter_ == 0)
                {
                    std::unique_lock uniqueLock(mutex_);
                    conditionVariable_.wait(uniqueLock, [owner](){return ((owner->nonZeroCounter_ != 0) || (owner->stopped_));});
                    return (not owner->stopped_);
                }
                return true;
            }

            bool wait_for
            (
                work_contract_group const * owner,
                std::chrono::nanoseconds duration
            ) const
            {                
                if (owner->nonZeroCounter_ == 0)
                {
                    std::unique_lock uniqueLock(mutex_);
                    auto waitSuccess = conditionVariable_.wait_for(uniqueLock, duration, [owner]() mutable{return ((owner->nonZeroCounter_ != 0) || (owner->stopped_));});
                    return ((!owner->stopped_) && (waitSuccess));
                }
                return true;
            }

        } waitableState_;
*/
        
        //=============================================================================
        class auto_clear_execute_flag
        {
        public:
            auto_clear_execute_flag(wc_id contractId, shared_segment_type * sharedStateSegment):contractId_(contractId),sharedStateSegment_(sharedStateSegment){}
            ~auto_clear_execute_flag(){sharedStateSegment_->clear_execute_flag(contractId_);}
        private:
            wc_id contractId_;
            shared_segment_type * sharedStateSegment_;
        };

    }; // class work_contract_group


    template <synchronization_mode T>
    std::uint64_t thread_local work_contract_group<T>::tls_biasFlags_ = 0;

} // namespace bcpp::implementation


namespace bcpp
{
    //=========================================================================
    using blocking_work_contract_group = implementation::work_contract_group<synchronization_mode::blocking>;
    using work_contract_group = implementation::work_contract_group<synchronization_mode::non_blocking>;
    using this_contract = internal::work_contract::this_contract;

} // namespace bcpp


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    return create_contract(std::forward<std::decay_t<decltype(workFunction)>>(workFunction), [](){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{    
    return create_contract(std::forward<std::decay_t<decltype(workFunction)>>(workFunction), 
            std::forward<decltype(releaseFunction)>(releaseFunction), [](auto){}, initialState);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction,
    std::invocable<std::exception_ptr> auto && exceptionFunction,
    work_contract_type::initial_state initialState
) -> work_contract_type
{
    if (auto workContractId = get_available_contract(); workContractId.is_valid())// != ~0ull)
    {
        release_[workContractId.get()] = std::forward<decltype(releaseFunction)>(releaseFunction); 
        work_[workContractId.get()] = std::forward<decltype(workFunction)>(workFunction); 
        exception_[workContractId.get()] = std::forward<decltype(exceptionFunction)>(exceptionFunction);
        return {sharedStates_, workContractId, initialState};
    }
    return {};
}
/*

//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_group<T>::increment_non_zero_counter
(
)  requires (mode == synchronization_mode::blocking)
{
    
    if (nonZeroCounter_++ == 0)
        waitableState_.notify_all();
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_group<T>::decrement_non_zero_counter
(
) requires (mode == synchronization_mode::blocking)
{
    --nonZeroCounter_;
}
*/

//=============================================================================
template <bcpp::synchronization_mode T>
inline std::uint64_t bcpp::implementation::work_contract_group<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
)
{
    return execute_next_contract(tls_biasFlags_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
template <typename rep, typename period>
inline std::uint64_t bcpp::implementation::work_contract_group<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
    std::chrono::duration<rep, period> duration
) requires (mode == synchronization_mode::blocking)
{
    return execute_next_contract(duration, tls_biasFlags_);
}

/*
//=============================================================================
template <bcpp::synchronization_mode T>
template <typename rep, typename period>
inline std::uint64_t bcpp::implementation::work_contract_group<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
    std::chrono::duration<rep, period> duration,
    std::uint64_t & biasFlags
) requires (mode == synchronization_mode::blocking)
{
//    if (waitableState_.wait_for(this, duration))
        return this->execute_next_contract(biasFlags);
    return ~0ull;
}
*/


//=============================================================================
template <bcpp::synchronization_mode T>
inline std::uint64_t bcpp::implementation::work_contract_group<T>::execute_next_contract
(
    // select a signal (a set signal) from the array of signal trees and, if found,
    // (which clears the signal) then process the pending action on that contract
    // based on the flags associated with that contract.
    std::uint64_t & biasFlags
) 
{
    /*
    if constexpr (mode == synchronization_mode::blocking)
    {
        if (!waitableState_.wait(this))// this should be done more graceful but for now ..
            return ~0ull;
    }        
    */
    auto subTreeIndex = (biasFlags / signal_tree_type::capacity);
    for (auto i = 0ull; i < subTreeCount_; ++i)
    {
        subTreeIndex &= subTreeMask_;
        if (auto signalIndex = sharedStates_->select(biasFlags); signalIndex != invalid_signal_index)
        {
            auto x = (signal_tree::select_bias_hint ^ biasFlags);
            if (auto b = (x & (~x + 1ull)) & (signal_tree_type::capacity - 1); b == 0)
            {
                biasFlags = ((subTreeIndex + 1) * signal_tree_type::capacity);
            }
            else
            {
                biasFlags |= b;
                biasFlags &= ~(b - 1);
            }
            process_contract(wc_id(segment_index(subTreeIndex), contract_index(signalIndex)));
            return signalIndex;
        }
        biasFlags = (++subTreeIndex * signal_tree_type::capacity);
    }
    return ~0ull;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract_group<T>::process_contract
(
    wc_id workContractId
)
{
    auto * sharedStateSegment = sharedStates_->get_segment(workContractId);
    auto flags = sharedStateSegment->set_execute_flag(workContractId);
    if (auto isReleased = ((flags & internal::work_contract::release_flag) == internal::work_contract::release_flag); isReleased)
    {
        process_release(workContractId); // release should be far less common path so ensure not inlined
    }
    else
    {
        // the expected case. invoke the work function
        auto_clear_execute_flag autoClearExecuteFlag(workContractId, sharedStateSegment);

        static constexpr void(*release)(std::uint64_t, void *) = [](auto workContractId, void * payload) noexcept
            {
                reinterpret_cast<shared_segment_type *>(payload)->release(wc_id(workContractId));
            };
        static constexpr void(*schedule)(std::uint64_t, void *) = [](auto workContractId, void * payload) noexcept
            {
                reinterpret_cast<shared_segment_type *>(payload)->schedule(wc_id(workContractId));
            };

        this_contract thisContract(workContractId.get(), sharedStateSegment, release, schedule);
        try
        {
            work_[workContractId.get()]();
        }
        catch (...)
        {
            process_exception(workContractId, std::current_exception());
        }
    }
}
