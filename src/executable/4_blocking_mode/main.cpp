#include <library/work_contract.h>
#include <include/jthread.h>
#include <iostream>
#include <chrono>

int main() 
{
    bcpp::blocking_work_contract_group group;
    bcpp::detail::jthread worker([&group](auto stopToken)
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract(/*std::chrono::seconds(1)*/); // use infinite wait - note: we could use wait with timeout here as well
            });

    auto contract = group.create_contract(
        [count = 0]() mutable 
        {
            std::cout << "Execution #" << ++count << "\n";
            if (count >= 3)
                bcpp::this_contract::release(); // Self-release after 3 executions
        },
        []() { std::cout << "Contract cleanup completed\n"; }
    );

    while (contract.is_valid())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        contract.schedule();
    }

    // in blocking mode, it is possible for worker thread to be waiting inside the call
    // to group.execute_next_contract() while there are no furter contracts to be scheduled.
    // since this wait is infinite (we did not pass a timeout value to group.exeucte_next_contract)
    // we need to explicitly stop the group with group.stop() in order to notify the waiting
    // worker thread (causing it to exit the infinite wait) prior to joining that thread below.
    // See DESIGN.md files for more details.
    group.stop();

    worker.request_stop();
    worker.join();
    return 0;
}
