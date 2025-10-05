#include "./work_contract_group.h"


namespace bcpp
{
    
    template <std::uint64_t total_counters, std::uint64_t bits_per_counter>
    struct largest_child_selector
    {
        inline auto operator()
        (
            std::uint64_t,
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
                return (total_counters - selected - 1);
            }
        }
    };

} // namespace bcpp


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_group<T>::work_contract_group
(
    std::uint64_t capacity
):
    sharedStates_(std::make_shared<shared_state_type>(capacity)),
    subTreeCount_(sharedStates_->capacity() / signal_tree_capacity),
    subTreeMask_(subTreeCount_ - 1),
    available_(subTreeCount_),
    work_(sharedStates_->capacity()),
    release_(sharedStates_->capacity()),
    exception_(sharedStates_->capacity())
{
    for (auto & subtree : available_)
        for (auto i = 0ull; i < signal_tree_capacity; ++i)
            subtree.set(i);
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_group<T>::work_contract_group
(
):
    work_contract_group(default_capacity)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_group<T>::~work_contract_group
(
)
{
    stop();
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_group<T>::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        // TODO: add generation count to contract ids and invalidate them all
//        for (auto & releaseToken : releaseToken_)
//            if ((bool)releaseToken)
//                releaseToken->orphan();
        if constexpr (mode == synchronization_mode::blocking)
        {
            // this addresses the problem of stopping the group while worker threads
            // are waiting indefinitely for a contract to be scheduled.  Since
            // the group is stopped no such scheduling will ever happen so we
            // give any waiting worker threads a chance to give up the wait now.
       //     std::unique_lock uniqueLock(mutex_);
       //     waitableState_.notify_all();
        }
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
auto bcpp::implementation::work_contract_group<T>::get_available_contract
(
) -> wc_id
{
    for (auto i = 0ull; i < available_.size(); ++i)
    {
        auto subTreeIndex (nextAvailableTreeIndex_++ & subTreeMask_);
        if (!available_[subTreeIndex].empty())
        {
            if (auto [signalIndex, _] = available_[subTreeIndex].select<largest_child_selector>(0); signalIndex != ~0ull)
            {
                wc_id workContractId((subTreeIndex * signal_tree_capacity) + signalIndex);
                return workContractId;
            }
        }
    }
    return ~0ull; 
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_group<T>::erase_contract
(
    // after contract's release function is invoked, clean up anything related to the contract
    wc_id contractId
) noexcept
{
    work_[contractId.get()] = nullptr;
    release_[contractId.get()] = nullptr;
    exception_[contractId.get()] = nullptr;
    sharedStates_->clear_flags(contractId);// TODO: invalidate the slot's geneation count
    available_[contractId.get() / signal_tree_capacity].set(contractId.get() % signal_tree_capacity);
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_group<T>::process_exception
(
    wc_id contractId,
    std::exception_ptr exception
)
{
    if (exception_[contractId.get()])
    {
        exception_[contractId.get()](exception);
    }
    else
    {
        std::rethrow_exception(exception);
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_group<T>::process_release
(
    // invoke the contract's release function.  use auto class to ensure
    // erasure of contract in the event of exceptions in the release function.
    wc_id contractId
)
{
    auto_erase_contract autoEraseContract(contractId, *this);
    try
    {
        release_[contractId.get()]();
    }
    catch (std::exception const & exception)
    {
        process_exception(contractId.get(), std::current_exception());
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
class bcpp::implementation::work_contract_group<T>::auto_erase_contract
{
public:
    auto_erase_contract(wc_id contractId, work_contract_group<T> & owner):contractId_(contractId),owner_(owner){}
    ~auto_erase_contract(){owner_.erase_contract(contractId_);}
private:
    wc_id                   contractId_;
    work_contract_group<T> &         owner_;
};


//=============================================================================
namespace bcpp::implementation
{
    template class work_contract_group<synchronization_mode::blocking>;
    template class work_contract_group<synchronization_mode::non_blocking>;
}