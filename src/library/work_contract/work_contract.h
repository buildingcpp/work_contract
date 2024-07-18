#pragma once

#include <include/synchronization_mode.h>
#include <include/non_copyable.h>

#include <atomic>
#include <cstdint>
#include <utility>
#include <memory>


namespace bcpp
{

    template <synchronization_mode T>
    class work_contract_tree;

    
    template <synchronization_mode T>
    class alignas(64) work_contract :
        non_copyable
    {
    public:

        using id_type = std::uint64_t;

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

        friend class work_contract_tree<T>;
        using work_contract_tree_type = work_contract_tree<T>;

        work_contract
        (
            work_contract_tree_type *, 
            std::shared_ptr<typename work_contract_tree_type::release_token>,
            id_type,
            initial_state = initial_state::unscheduled
        );

        id_type get_id() const;

        work_contract_tree_type *   owner_{};

        std::shared_ptr<typename work_contract_tree_type::release_token> releaseToken_;

        id_type                 id_{};

    }; // class work_contract

} // namespace bcpp


#include "./work_contract_tree.h"


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract<T>::work_contract
(
    work_contract_tree_type * owner,
    std::shared_ptr<typename work_contract_tree_type::release_token> releaseToken, 
    id_type id,
    initial_state initialState
):
    owner_(owner),
    releaseToken_(releaseToken),
    id_(id)
{
    if (initialState == initial_state::scheduled)
        schedule();
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract<T>::work_contract
(
    work_contract && other
):
    owner_(other.owner_),
    releaseToken_(other.releaseToken_),
    id_(other.id_)
{
    other.owner_ = {};
    other.id_ = {};
    other.releaseToken_ = {};
}

    
//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract<T>::operator =
(
    work_contract && other
) -> work_contract &
{
    if (this != &other)
    {
        release();

        owner_ = other.owner_;
        id_ = other.id_;
        releaseToken_ = other.releaseToken_;
        
        other.owner_ = {};
        other.id_ = {};
        other.releaseToken_ = {};
    }
    return *this;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract<T>::~work_contract
(
)
{
    release();
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline auto bcpp::work_contract<T>::get_id
(
) const -> id_type
{
    return id_;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline void bcpp::work_contract<T>::schedule
(
)
{
    owner_->schedule(id_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::work_contract<T>::release
(
)
{
    if (auto releaseToken = std::exchange(releaseToken_, nullptr); releaseToken)
    {
        releaseToken->schedule(*this);
        owner_ = {};
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bool bcpp::work_contract<T>::is_valid
(
) const
{
    return ((releaseToken_) && (releaseToken_->is_valid()));
}


//=============================================================================
template <bcpp::synchronization_mode T>
inline bcpp::work_contract<T>::operator bool
(
) const
{
    return is_valid();
}
