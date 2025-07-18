
#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>

#include <iostream>
#include <thread>
#include <vector>


//=============================================================================
int main
(
    int, 
    char const **
)
{
    bcpp::spsc_fixed_queue<int> queue(1024);
    bcpp::work_contract_group wcg;

    static auto constexpr num_worker_threads = 16;
    std::vector<std::jthread> workerThreads(num_worker_threads);
    for (auto & workerThread : workerThreads)
        workerThread = std::jthread([&](auto token){while (!token.stop_requested()) wcg.execute_next_contract();});

    // async consume
    auto wc = wcg.create_contract([&]()
            {
                std::cout << "thread " << std::this_thread::get_id() << " consumed " << queue.pop() << "\n";
                if (!queue.empty())
                    bcpp::this_contract::schedule();
            });

    // produce
    for (auto i = 0; i < 16; ++i)
    {
        while (!queue.push(i))
            ;
        wc.schedule();
    }

    while (!queue.empty())
        ;
    return 0;
}
