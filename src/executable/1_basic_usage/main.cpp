
#include <library/work_contract.h>
#include <iostream>
#include <thread>


//=============================================================================
void example_work
(
    // 1. create work contract group
    // 2. create work contract
    // 3. schedule work contract
    // 4. wait until work contract has been invoked
)
{
    std::cout << "===============================\nexample_work:\n";
    // create work contract group
    bcpp::work_contract_group workContractGroup;

    // create async worker thread to service scheduled contracts
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    // create a work contract
    auto workContract = workContractGroup.create_contract([&](auto & token){std::cout << "work invoked\n"; token.release();});
    workContract.schedule(); // schedule the contract

    // wait until contract has been invoked
    while (workContract)
        ;
}


//=============================================================================
void example_blocking_work
(
    // 1. create work contract group
    // 2. create work contract
    // 3. schedule work contract
    // 4. wait until work contract has been invoked
)
{
    std::cout << "===============================\nexample_work:\n";
    std::atomic<bool> contractInvoked{false};

    // create work contract group
    bcpp::blocking_work_contract_group workContractGroup;

    // create async worker thread to service scheduled contracts
    std::jthread workerThread([&](){workContractGroup.execute_next_contract();});

    // create a work contract
    auto workContract = workContractGroup.create_contract([&](auto & token){std::cout << "work invoked\n"; contractInvoked = true;});

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "scheduling contract\n";
    workContract.schedule(); // schedule the contract

    // wait until contract has been invoked
    while (!contractInvoked)
        ;
}


//=============================================================================
void example_release
(
    // 1. create work contract group
    // 2. create work contract
    // 3. schedule work contract
    // 4. work function will release contract
    // 5. wait until work contract has been released
)
{
    std::cout << "===============================\nexample_release:\n";

    // create work contract group
    bcpp::work_contract_group workContractGroup;

    // create async worker thread
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    // create a work contract and set initial state to scheduled in the same call
    auto workFunction = [](auto & contractToken){std::cout << "work invoked\n"; contractToken.release();};
    auto releaseFunction = [](){std::cout << "release invoked\n";};
    auto workContract = workContractGroup.create_contract(workFunction, releaseFunction, bcpp::work_contract::initial_state::scheduled);

    // wait until contract has been invoked and released
    while (workContract.is_valid())
        ;

    // stop the worker thread
    workerThread.request_stop();
    workerThread.join();
}


//=============================================================================
void example_redundant_schedule_is_ignored
(
    // 1. create work contract group
    // 2. create work contract
    // 3. schedule work contract
    // 4. work function will release contract
    // 5. wait until work contract has been released
)
{
    std::cout << "===============================\nexample_redundant_schedule_is_ignored:\n";

    // create work contract group
    bcpp::work_contract_group workContractGroup;

    // create a work contract and set initial state to scheduled in the same call
    auto workFunction = [n=0](auto & contractToken) mutable{std::cout << "work invocation count = " << ++n << "\n"; contractToken.release();};
    auto workContract = workContractGroup.create_contract(workFunction);
    workContract.schedule();
    workContract.schedule(); // scheduling an already scheduled work contract does nothing

    // create async worker thread. do it *after* we have already scheduled the WC multiple times
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    // wait until contract has been invoked and released
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
    example_work();
    example_blocking_work();
    example_release();
    example_redundant_schedule_is_ignored();

    return 0;
}
