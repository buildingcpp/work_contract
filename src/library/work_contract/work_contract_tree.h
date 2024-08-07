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

#include <iostream>


namespace bcpp 
{

    template <synchronization_mode> class work_contract;


    template <synchronization_mode T>
    class work_contract_tree :
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

    private:

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
            work_contract_id 
        );

        void release
        (
            work_contract_id 
        );        
        
        template <std::size_t>
        void set_contract_flag
        (
            work_contract_id 
        );

        std::size_t process_contract();

        void process_release(work_contract_id);

        void process_contract(work_contract_id);

        void clear_execute_flag
        (
            work_contract_id
        );

        void erase_contract
        (
            work_contract_id
        );

        work_contract_id get_available_contract();

        using signal_tree_type = signal_tree<512>;
        static auto constexpr signal_tree_capacity = signal_tree_type::capacity;
        static auto constexpr bias_shift = (64 - minimum_bit_count(signal_tree_capacity - 1)); 

        std::uint64_t                                                   subTreeCount_;

        std::uint64_t                                                   subTreeMask_;

        std::uint64_t                                                   subTreeShift_;

        std::vector<contract>                                           contracts_;

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
        work_contract_token(work_contract_id, work_contract_tree &);
        ~work_contract_token();
        void schedule();
        void release();
    private:
        work_contract_id            contractId_{~0ull};
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
inline auto bcpp::work_contract_tree<T>::get_available_contract
(
) -> work_contract_id
{
    auto biasFlags = tls_biasFlags++;
    auto subTreeIndex = biasFlags;
    biasFlags >>= subTreeShift_;
    biasFlags <<= bias_shift;

    for (auto i = 0; i < available_.size(); ++i)
    {
        if (auto signalId = available_[subTreeIndex & subTreeMask_].select<largest_child_selector>(biasFlags); signalId != ~0ull)
        {
            auto workContractId = (subTreeIndex & subTreeMask_) * signal_tree_capacity;
            workContractId += signalId;
            return workContractId;
        }
        ++subTreeIndex;
    }
    return {}; 
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
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = std::forward<decltype(workFunction)>(workFunction); 
        contract.release_ = std::forward<decltype(releaseFunction)>(releaseFunction); 
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
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
    if (auto workContractId = get_available_contract(); workContractId != ~0ull)
    {
        auto & contract = contracts_[workContractId];
        contract.flags_ = 0;
        contract.work_ = [work = std::forward<decltype(workFunction)>(workFunction)](auto &) mutable{work();}; 
        contract.release_ = std::forward<decltype(releaseFunction)>(releaseFunction);
        return {this, releaseToken_[workContractId] = std::make_shared<release_token>(this), workContractId, initialState};
    }
    return {};
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::release
(
    work_contract_id contractId
)
{
    set_contract_flag<contract::release_flag | contract::schedule_flag>(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::schedule
(
    work_contract_id contractId
)
{
    set_contract_flag<contract::schedule_flag>(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
template <std::size_t flags_to_set>
inline void bcpp::work_contract_tree<T>::set_contract_flag
(
    work_contract_id contractId
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::schedule_flag);
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
    {
        auto signalIndex = contractId % signal_tree_capacity;
        auto treeIndex = contractId / signal_tree_capacity;
        signalTree_[treeIndex].set(signalIndex);
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

    auto biasFlags = tls_biasFlags++;
    auto subTreeIndex = (biasFlags & subTreeMask_);
    biasFlags >>= subTreeShift_;
    biasFlags <<= bias_shift;

    for (auto i = 0; i < signalTree_.size(); ++i)
    {
        if (auto signalIndex = signalTree_[subTreeIndex & subTreeMask_].select(biasFlags); signalIndex != invalid_signal_index)
        {
            work_contract_id workContractId((subTreeIndex & subTreeMask_) * signal_tree_capacity);
            workContractId |= signalIndex;
         //   tls_biasFlags = ((signalIndex) << subTreeShift_) | ((subTreeIndex + 1) & subTreeMask_);//(workContractId + 1);
            process_contract(work_contract_id(workContractId));
            return true;
        }
        ++subTreeIndex;
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
        auto biasFlags = tls_biasFlags++;
        auto subTreeIndex = (biasFlags & subTreeMask_);
        biasFlags >>= subTreeShift_;
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
inline void bcpp::work_contract_tree<T>::clear_execute_flag
(
    work_contract_id contractId
)
{
    if (((contracts_[contractId].flags_ -= contract::execute_flag) & contract::schedule_flag) == contract::schedule_flag)
    {
        auto signalIndex = (contractId % signal_tree_capacity);
        auto treeIndex = (contractId / signal_tree_capacity);
        signalTree_[treeIndex].set(signalIndex);
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::process_contract
(
    work_contract_id contractId
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
    work_contract_id contractId,
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


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract_tree<T>::work_contract_token::release
(
)
{
    owner_.release(contractId_);
}
