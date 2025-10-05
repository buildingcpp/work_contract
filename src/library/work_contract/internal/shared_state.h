#pragma once

#include "./shared_state_segment.h"
#include "./work_contract_id.h"
#include "./waitable_state.h"

#include <include/signal_tree.h>
#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <vector>
#include <cstdint>


namespace bcpp::internal::work_contract
{

    template <synchronization_mode mode, std::size_t signal_tree_capacity>
    class shared_state final :
        non_copyable,
        non_movable
    {
    public:

        using segment_type = shared_state_segment<mode, signal_tree_capacity>;
        using wc_id = work_contract_id<signal_tree_capacity>;

        shared_state
        (
            std::uint32_t
        );
    
        ~shared_state() = default;

        void schedule
        (
            wc_id
        );

        void release
        (
            wc_id
        );

        std::uint64_t select
        (
            std::uint64_t biasFlags
        ) noexcept;

        void clear_execute_flag
        (
            wc_id
        ) noexcept;

        state_flags set_execute_flag
        (
            wc_id
        ) noexcept;

        void clear_flags
        (
            wc_id
        ) noexcept;

        std::uint64_t capacity() const noexcept{return (sharedStateSegments_.size() * signal_tree_capacity);}

        segment_type * get_segment
        (
            segment_index
        ) noexcept;

    private:

        std::vector<segment_type>       sharedStateSegments_;

        std::uint64_t                   biasFlagsMask_{0};
    };

} // namespace bcpp::internal::work_contract


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::shared_state
(
    std::uint32_t capacity
):
    sharedStateSegments_(minimum_power_of_two((capacity + signal_tree_capacity - 1) / signal_tree_capacity)),
    biasFlagsMask_(sharedStateSegments_.size() - 1)
{
    if constexpr (mode == synchronization_mode::blocking)
    {
        auto waitableState = std::make_shared<waitable_state>();
        for (auto & sharedStateSegment : sharedStateSegments_)
            sharedStateSegment.set_waitable_state(waitableState);
    }
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::schedule
(
    wc_id workContractId
)
{
    sharedStateSegments_[segment_index(workContractId)].schedule(workContractId);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::release
(
    wc_id workContractId
)
{
    sharedStateSegments_[segment_index(workContractId)].release(workContractId);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
std::uint64_t bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::select
(
    std::uint64_t biasFlags
) noexcept
{
    return sharedStateSegments_[(biasFlags / signal_tree_capacity) & biasFlagsMask_].select(biasFlags);
}

         
//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline void bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::clear_execute_flag
(
    wc_id workContractId
) noexcept
{
    sharedStateSegments_[segment_index(workContractId)].clear_execute_flag(workContractId);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
inline auto bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::set_execute_flag
(
    // NOTE: this logic works because it is only ever called when the scheduled flag is set
    // and the executing flag is NOT set.  scheduled flag is 0x01 and executing flag is 0x02
    // Therefore incrementing sets the executing flag and clears the scheduling flag.
    wc_id workContractId
) noexcept -> state_flags
{
    return sharedStateSegments_[segment_index(workContractId)].set_execute_flag(workContractId);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
void bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::clear_flags
(
    wc_id workContractId
) noexcept
{
    sharedStateSegments_[segment_index(workContractId)].clear_flags(workContractId);
}


//=============================================================================
template <bcpp::synchronization_mode mode, std::size_t signal_tree_capacity>
auto bcpp::internal::work_contract::shared_state<mode, signal_tree_capacity>::get_segment
(
    segment_index segmentIndex
) noexcept -> segment_type *
{
    return sharedStateSegments_.data() + segmentIndex;
}
