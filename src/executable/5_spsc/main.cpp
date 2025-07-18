
#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>

#include <iostream>
#include <thread>


//=============================================================================
int main()
{
    bcpp::work_contract_group wcg;
    std::jthread workerThread([&](auto st){while (!st.stop_requested()) wcg.execute_next_contract();});

    bcpp::spsc_fixed_queue<int> queue(1024);
    auto consume = [&](){std::cout << queue.pop() << '\n'; if (!queue.empty()) bcpp::this_contract::schedule();};
    auto wc = wcg.create_contract(consume);    
    
    auto produce = [&](auto n){queue.push(n); wc.schedule();};
    for (auto i = 0; i < 16; ++i)
        produce(i);

    while (!queue.empty())
        ;
    return 0;
}
