#pragma once

#include "./work_contract_id.h"
#include <include/synchronization_mode.h>
#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <concepts>
#include <type_traits>


namespace bcpp::implementation
{

    template <synchronization_mode T> class work_contract_group;
  

    //=========================================================================
    template <synchronization_mode T>
    class work_contract_token final :
        non_movable,
        non_copyable
    {
    public:

        void schedule() noexcept;

        void release() noexcept;

        work_contract_id get_contract_id() const noexcept;

    private:

        friend work_contract_group<T>;

        work_contract_token() = delete;

        work_contract_token
        (
            work_contract_id, 
            work_contract_group<T> &
        ) noexcept;

        ~work_contract_token() = default;

        work_contract_id            contractId_{~0ull};
        
        work_contract_group<T> &    owner_;
        
        bool                        released_{false};

    }; // class work_contract_group<>::work_contract_token


    using asynchronous_work_contract_token = work_contract_token<synchronization_mode::asynchronous>;
    using synchronous_work_contract_token = work_contract_token<synchronization_mode::synchronous>;


    template <typename T>
    concept work_contract_token_callable = ((std::is_invocable_v<std::decay_t<T>, asynchronous_work_contract_token &>) ||
                                            (std::is_invocable_v<std::decay_t<T>, synchronous_work_contract_token &>));

    template <typename T>
    concept work_contract_no_token_callable = (std::is_invocable_v<std::decay_t<T>>);


    template <typename T>
    concept work_contract_callable = (work_contract_token_callable<T> || work_contract_no_token_callable<T>);

} // namespace bcpp::implementation


//=========================================================================
template <bcpp::synchronization_mode T>
bcpp::implementation::work_contract_token<T>::work_contract_token
(
    work_contract_id contractId, 
    work_contract_group<T> & owner
) noexcept :
    contractId_(contractId),
    owner_(owner)
{
}


//=========================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_token<T>::schedule
(
) noexcept
{
    if (!released_) 
        owner_.schedule(contractId_);
}


//=========================================================================
template <bcpp::synchronization_mode T>
void bcpp::implementation::work_contract_token<T>::release
(
) noexcept
{
    if (!released_) 
        owner_.release(contractId_); 
    released_ = true;
}


//=========================================================================
template <bcpp::synchronization_mode T>
auto bcpp::implementation::work_contract_token<T>::get_contract_id
(
) const noexcept -> work_contract_id
{
    return contractId_;
}
