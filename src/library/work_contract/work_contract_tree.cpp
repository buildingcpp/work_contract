#include "./work_contract_tree.h"


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::work_contract_tree<T>::work_contract_tree
(
    std::uint64_t capacity
):
    contracts_(available_.capacity()),
    releaseToken_(available_.capacity())
{
    for (auto i = 0; i < available_.capacity(); ++i)
        available_.set(i);
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::work_contract_tree<T>::~work_contract_tree
(
)
{
    stop();
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::work_contract_tree<T>::stop
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
void bcpp::work_contract_tree<T>::erase_contract
(
    // after contract's release function is invoked, clean up anything related to the contract
    std::uint64_t contractId
)
{
    auto & contract = contracts_[contractId];
    contract.work_ = nullptr;
    contract.release_ = nullptr;
    releaseToken_[contractId] = {};
    available_.set(contractId);
}


//=============================================================================
template <bcpp::synchronization_mode T>
void bcpp::work_contract_tree<T>::process_release
(
    // invoke the contract's release function.  use auto class to ensure
    // erasure of contract in the event of exceptions in the release function.
    std::uint64_t contractId
)
{
    auto_erase_contract autoEraseContract(contractId, *this);
    contracts_[contractId].release_();
}


//=============================================================================
template <bcpp::synchronization_mode T>
bcpp::work_contract_tree<T>::release_token::release_token
(
    work_contract_tree * workContractTree
):
    workContractTree_(workContractTree)
{
}


//=============================================================================
template <bcpp::synchronization_mode T>
bool bcpp::work_contract_tree<T>::release_token::schedule
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
void bcpp::work_contract_tree<T>::release_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractTree_ = nullptr;
}


//=============================================================================
template <bcpp::synchronization_mode T>
bool bcpp::work_contract_tree<T>::release_token::is_valid
(
) const
{
    std::lock_guard lockGuard(mutex_);
    return ((bool)workContractTree_);
}


//=============================================================================
template <bcpp::synchronization_mode T>
class bcpp::work_contract_tree<T>::auto_erase_contract
{
public:
    auto_erase_contract(std::uint64_t contractId, work_contract_tree<T> & owner):contractId_(contractId),owner_(owner){}
    ~auto_erase_contract(){owner_.erase_contract(contractId_);}
private:
    std::uint64_t                   contractId_;
    bcpp::work_contract_tree<T> &   owner_;
};


//=============================================================================
namespace bcpp
{
    template class work_contract_tree<synchronization_mode::blocking>;
    template class work_contract_tree<synchronization_mode::non_blocking>;
}