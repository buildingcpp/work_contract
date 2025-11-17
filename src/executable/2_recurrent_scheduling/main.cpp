#include <library/work_contract.h>
#include <include/jthread.h>
#include <iostream>

int main() 
{
    bcpp::work_contract_group group;
    bcpp::detail::jthread worker([&group](auto stopToken)
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract([count = 0]() mutable 
            {
                std::cout << "Execution #" << ++count << "\n";
                if (count < 3)
                    bcpp::this_contract::schedule(); // Reschedule for up to 3 executions
                else
                    bcpp::this_contract::release(); // Release after 3 executions
            });

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
