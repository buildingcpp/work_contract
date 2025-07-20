#include <library/work_contract.h>
#include <iostream>
#include <thread>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract(
        [count = 0]() mutable 
        {
            std::cout << "Execution #" << ++count << "\n";
            if (count < 3)
                bcpp::this_contract::schedule();
            else
                bcpp::this_contract::release();
        },
        []() { std::cout << "Contract cleanup completed\n"; } // Release callback
    );

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}