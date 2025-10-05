#pragma once

#include "./state_flags.h"
#include "./work_contract_id.h"
#include "./waitable_state.h"

#include <include/signal_tree.h>
#include <include/non_movable.h>
#include <include/non_copyable.h>
#include <include/synchronization_mode.h>

#include <array>
#include <cstdint>
#include <memory>


namespace bcpp::internal::work_contract
{

    template <synchronization_mode mode, std::size_t signal_tree_capacity>
    class shared_state_segment final :
        non_copyable,
        non_movable
    {
    public:

        using wc_id = work_contract_id<signal_tree_capacity>;

        shared_state_segment();

        ~shared_state_segment() = default;

        void schedule
        (
            contract_index
        );

        void release
        (
            contract_index
        );

        std::uint64_t select
        (
            std::uint64_t biasFlags
        ) noexcept;

        void clear_execute_flag
        (
            contract_index
        ) noexcept;

        state_flags set_execute_flag
        (
            contract_index
        ) noexcept;

        void clear_flags
        (
            contract_index
        ) noexcept;

        void set_waitable_state
        (
            std::shared_ptr<waitable_state> waitableState
        )
        {
            waitableState_ = waitableState;
        }

    private:

        void set_signal
        (
            contract_index
        ) noexcept;

        signal_tree<signal_tree_capacity>                           signalTree_;

        std::array<std::atomic<state_flags>, signal_tree_capacity>  contractFlags_;

        std::shared_ptr<waitable_state>                             waitableState_;
    };

} // namespace bcpp::internal::work_contract


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::shared_state_segment
(
)
{
    for (auto & f : contractFlags_)
        f.store(0, std::memory_order_relaxed); 
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::schedule
(
    contract_index contractIndex
)
{
    static auto constexpr flags_to_set = schedule_flag;
    auto previousFlags = contractFlags_[contractIndex].fetch_or(flags_to_set);
    if (auto notScheduledNorExecuting = ((previousFlags & (schedule_flag | execute_flag)) == 0); notScheduledNorExecuting)
        set_signal(contractIndex);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::release
(
    contract_index contractIndex
)
{
    static auto constexpr flags_to_set = (release_flag | schedule_flag);
    auto previousFlags = contractFlags_[contractIndex].fetch_or(flags_to_set);
    if (auto notScheduledNorExecuting = ((previousFlags & (schedule_flag | execute_flag)) == 0); notScheduledNorExecuting)
        set_signal(contractIndex);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
std::uint64_t bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::select
(
    std::uint64_t biasFlags
) noexcept
{
    if constexpr (mode == synchronization_mode::non_blocking)
    {
        auto [signalIndex, treeIsEmpty] = signalTree_.select(biasFlags);
        return signalIndex;
    }
    else
    {
        waitableState_->wait(); // blocking wait until at least one signal is available
        auto [signalIndex, treeIsEmpty] = signalTree_.select(biasFlags);
        if (treeIsEmpty)
            waitableState_->decrement_non_zero_counter();
        return signalIndex;
    }
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::set_signal
(
    contract_index contractIndex
) noexcept
{
    if constexpr (mode == synchronization_mode::non_blocking)
    {
        signalTree_.set(contractIndex);
    }
    else
    {
        auto [treeWasEmpty, _] = signalTree_.set(contractIndex);
        if (treeWasEmpty)
            waitableState_->increment_non_zero_counter();
    }
}

         
//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::clear_execute_flag
(
    contract_index contractIndex
) noexcept
{
    if (((contractFlags_[contractIndex] -= internal::work_contract::execute_flag) & internal::work_contract::schedule_flag) == internal::work_contract::schedule_flag)
        set_signal(contractIndex);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline auto bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::set_execute_flag
(
    // NOTE: this logic works because it is only ever called when the scheduled flag is set
    // and the executing flag is NOT set.  scheduled flag is 0x01 and executing flag is 0x02
    // Therefore incrementing sets the executing flag and clears the scheduling flag.
    contract_index contractIndex
) noexcept -> state_flags
{
    return ++contractFlags_[contractIndex];
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
void bcpp::internal::work_contract::shared_state_segment<mode, signal_tree_capacity>::clear_flags
(
    contract_index contractIndex
) noexcept
{
    contractFlags_[contractIndex] = 0;
}
