
#include <library/work_contract.h>
#include <iostream>
#include <thread>


//=============================================================================
void example_multi_invocation
(
    // 1. create work contract tree
    // 2. create work contract
    // 3. schedule work contract
    // 4. reschedule the work contract 'invocation_count' times
    // 5. release work contract when 'invocation_count' = 0
)
{
    static auto constexpr invocation_count = 6;

    std::cout << "===============================\nexample_multi_invocation:\n";
    // create work contract tree
    bcpp::work_contract_group workContractGroup;

    // create async worker thread to service scheduled contracts
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    // create a work contract
    auto workFunction = [n = invocation_count](auto & contractToken) mutable
            {
                std::cout << "n = " << n << "\n"; 
                if (--n == 0) 
                    contractToken.release();
                contractToken.schedule();
            };
    auto workContract = workContractGroup.create_contract(workFunction, bcpp::work_contract::initial_state::scheduled);

    // wait until contract has been invoked
    while (workContract.is_valid())
        ;

    // stop the worker thread
    workerThread.request_stop();
    workerThread.join();
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    example_multi_invocation();

    return 0;
}
