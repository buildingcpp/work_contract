#include "./work_contract_tree.h"


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_tree<T>::work_contract_tree
(
    std::uint64_t capacity
):
    subTreeCount_(minimum_power_of_two((capacity + (signal_tree_type::capacity - 1)) / signal_tree_type::capacity)),
    subTreeMask_(subTreeCount_ - 1),
    subTreeShift_(minimum_bit_count(subTreeCount_ - 1)),
    signalTree_(subTreeCount_),
    available_(subTreeCount_),
    contracts_(subTreeCount_ * signal_tree_type::capacity),
    release_(contracts_.size()),
    exception_(contracts_.size()),
    releaseToken_(subTreeCount_ * signal_tree_type::capacity)
{
    for (auto & subtree : available_)
        for (auto i = 0ull; i < signal_tree_type::capacity; ++i)
            subtree.set(i);
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_tree<T>::work_contract_tree
(
):
    work_contract_tree(default_capacity)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_tree<T>::~work_contract_tree
(
)
{
    stop();
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        for (auto & releaseToken : releaseToken_)
            if ((bool)releaseToken)
                releaseToken->orphan();
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
auto bcpp::implementation::work_contract_tree<T>::get_available_contract
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
void bcpp::implementation::work_contract_tree<T>::erase_contract
(
    // after contract's release function is invoked, clean up anything related to the contract
    work_contract_id contractId
)
{
    auto & contract = contracts_[contractId];
    contract.work_ = nullptr;
    release_[contractId] = nullptr;
    exception_[contractId] = nullptr;
    if (auto releaseToken = std::exchange(releaseToken_[contractId], nullptr); releaseToken)
        releaseToken->orphan(); // mark as invalid

    auto [treeIndex, signalIndex] = get_tree_and_signal_index(contractId);
    available_[treeIndex].set(signalIndex);
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::process_exception
(
    work_contract_id contractId,
    std::exception_ptr exception
)
{
    if (exception_[contractId])
    {
        exception_token token(contractId, exception, *this);
        exception_[contractId](token);
    }
    else
    {
        std::rethrow_exception(exception);
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::process_release
(
    // invoke the contract's release function.  use auto class to ensure
    // erasure of contract in the event of exceptions in the release function.
    work_contract_id contractId
)
{
    auto_erase_contract autoEraseContract(contractId, *this);
    try
    {
        release_[contractId]();
    }
    catch (std::exception const & exception)
    {
        process_exception(contractId, std::current_exception());
    }
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_tree<T>::release_token::release_token
(
    work_contract_tree * workContractTree
):
    workContractTree_(workContractTree)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
bool bcpp::implementation::work_contract_tree<T>::release_token::schedule
(
    work_contract_type const & workContract
)
{
    std::lock_guard lockGuard(mutex_);
    if (auto workContractTree = std::exchange(workContractTree_, nullptr); workContractTree != nullptr)
    {
        workContractTree->release(workContract.get_id());
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::release_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractTree_ = nullptr;
}


//=============================================================================
template <bcpp::synchronization_mode T>
bool bcpp::implementation::work_contract_tree<T>::release_token::is_valid
(
) const
{
    std::lock_guard lockGuard(mutex_);
    return ((bool)workContractTree_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_tree<T>::exception_token::exception_token 
(
    work_contract_id contractId,
    std::exception_ptr exception,
    work_contract_tree<T> & owner
):
    contractId_(contractId),
    exception_(exception),
    owner_(owner)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::exception_token::release
(
)
{
    owner_.release(contractId_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_tree<T>::exception_token::schedule
(
)
{
    owner_.schedule(contractId_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
auto bcpp::implementation::work_contract_tree<T>::exception_token::get_contract_id
(
) const -> work_contract_id
{
    return contractId_;
}


//=============================================================================
template <bcpp::synchronization_mode T>
auto bcpp::implementation::work_contract_tree<T>::exception_token::get_exception
(
) const -> std::exception_ptr
{
    return exception_;
}


//=============================================================================
template <bcpp::synchronization_mode T>
class bcpp::implementation::work_contract_tree<T>::auto_erase_contract
{
public:
    auto_erase_contract(std::uint64_t contractId, work_contract_tree<T> & owner):contractId_(contractId),owner_(owner){}
    ~auto_erase_contract(){owner_.erase_contract(contractId_);}
private:
    std::uint64_t                   contractId_;
    work_contract_tree<T> &         owner_;
};


//=============================================================================
namespace bcpp::implementation
{
    template class work_contract_tree<synchronization_mode::blocking>;
    template class work_contract_tree<synchronization_mode::non_blocking>;
}