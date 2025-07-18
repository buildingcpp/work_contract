
#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>

#include <iostream>
#include <thread>


//=============================================================================
void example_data
(
)
{
    std::cout << "===============================\nexample_data:\n";
    // create work contract group
    bcpp::work_contract_group workContractGroup;

    // create async worker thread to service scheduled contracts
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    // create a work contract
    bcpp::spsc_fixed_queue<std::pair<int, int>> dataQueue(1024);

    auto workContract = workContractGroup.create_contract(
                [&]()
                {
                    std::pair<int, int> data;
                    if (dataQueue.try_pop(data))
                    {
                        auto [a, b] = data;
                        std::cout << a << " * " << b << " = " << (a * b) << std::endl;
                    }
                    if (!dataQueue.empty())
                        bcpp::this_contract::schedule();
                    else
                        bcpp::this_contract::release();
                });

    for (auto i = 0; i < 32; ++i)
    {
        dataQueue.push(std::make_pair(i, i + 1));
        workContract.schedule(); // schedule the contract
    }

    // wait until contract has been invoked
    while (workContract)
        ;
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    example_data();

    return 0;
}
