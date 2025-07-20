#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    // Create a lock-free SPSC queue for ingress data (capacity: 10 integers)
    bcpp::spsc_fixed_queue<int> dataQueue(10);

    auto contract = group.create_contract(
        [&dataQueue]() mutable 
        {
            int value;
            if (dataQueue.pop(value))
            {
                if (value == -1)
                    bcpp::this_contract::release(); // Release on termination flag
                else
                    std::cout << "Processed value: " << value << "\n";
            }
            if (not dataQueue.empty())
                bcpp::this_contract::schedule(); // Reschedule if more data
        },
        []() { std::cout << "Contract cleanup completed\n"; }
    );

    // Push data to queue and schedule contract
    for (int i = 1; i <= 5; ++i)
    {
        dataQueue.push(i);
        contract.schedule();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    dataQueue.push(-1); // Push termination flag
    contract.schedule();

    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}