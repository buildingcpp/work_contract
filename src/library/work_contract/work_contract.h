#pragma once

#include "./internal/work_contract_id.h"
#include <include/synchronization_mode.h>
#include <include/non_copyable.h>

#include <atomic>
#include <cstdint>
#include <utility>
#include <memory>


namespace bcpp::implementation
{

    template <synchronization_mode T>
    class work_contract_group;

    
    template <synchronization_mode T>
    class alignas(64) work_contract :
        non_copyable
    {
    public:

        using id_type = internal::work_contract::contract_index;

        enum class initial_state 
        {
            unscheduled = 0,
            scheduled = 1
        };

        work_contract() = default;

        ~work_contract();

        work_contract(work_contract &&);
        work_contract & operator = (work_contract &&);

        void schedule();

        bool release();

        bool is_valid() const;

        explicit operator bool() const;

    private:

        using work_contract_group_type = work_contract_group<T>;

        template <synchronization_mode> friend class work_contract_group;

        using shared_state = typename work_contract_group_type::shared_state_type;
        using shared_state_segment = typename shared_state::segment_type;

        work_contract
        (
            std::shared_ptr<shared_state>,
            internal::work_contract::work_contract_id_concept auto,
            initial_state = initial_state::unscheduled
        );

        id_type get_id() const;

        id_type                         id_{};
        std::shared_ptr<shared_state>   sharedStates_;
        shared_state_segment *          sharedStateSegment_;

    }; // class work_contract

} // namespace bcpp::implementation


namespace bcpp
{

    using work_contract = implementation::work_contract<synchronization_mode::non_blocking>;
    using blocking_work_contract = implementation::work_contract<synchronization_mode::blocking>;
}


#include "./work_contract_group.h"


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract<T>::work_contract
(
    std::shared_ptr<shared_state> sharedStates,
    internal::work_contract::work_contract_id_concept auto workContractId,
    initial_state initialState
):
    id_(workContractId),
    sharedStates_(sharedStates),
    sharedStateSegment_(sharedStates_->get_segment(workContractId))
{
    if (initialState == initial_state::scheduled)
        schedule();
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract<T>::work_contract
(
    work_contract && other
):
    id_(other.id_),
    sharedStates_(std::move(other.sharedStates_)),
    sharedStateSegment_(other.sharedStateSegment_)
{
    other.id_ = {};
    other.sharedStateSegment_ = {};
    other.sharedStates_ = {};
}

    
//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract<T>::operator =
(
    work_contract && other
) -> work_contract &
{
    if (this != &other)
    {
        release();

        id_ = other.id_;
        sharedStates_ = std::move(other.sharedStates_);
        sharedStateSegment_ = other.sharedStateSegment_;

        other.id_ = {};
        other.sharedStateSegment_ = {};
        other.sharedStates_ = {};
    }
    return *this;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract<T>::~work_contract
(
)
{
    release();
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::implementation::work_contract<T>::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::implementation::work_contract<T>::schedule
(
)
{
    sharedStateSegment_->schedule(id_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::implementation::work_contract<T>::release
(
)
{
    if (is_valid())
    {
        sharedStateSegment_->release(id_);
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::implementation::work_contract<T>::is_valid
(
) const
{
    return (sharedStateSegment_ != nullptr);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::implementation::work_contract<T>::operator bool
(
) const
{
    return is_valid();
}
