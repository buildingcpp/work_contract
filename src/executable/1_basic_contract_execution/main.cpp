#include <library/work_contract.h>
#include <include/jthread.h>
#include <iostream>

int main() 
{
    // Create a work contract group
    bcpp::work_contract_group group;

    // Start worker thread to process scheduled work contracts
    bcpp::detail::jthread worker([&group](auto stopToken)
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    // Create a work contract from the work contract group
    auto contract = group.create_contract([]() 
            {
                std::cout << "Hello, World! from Work Contract\n";
                bcpp::this_contract::release(); // Release to async destroy this contract
            });

    // Schedule the contract
    contract.schedule();

    // Main thread waits for contract to be released
    while (contract.is_valid()) {}

    // Signal worker to stop
    worker.request_stop();
    worker.join();

    return 0;
}
