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

    auto contract = group.create_contract(
        []() 
        {
            throw std::runtime_error("Error in contract");
        },
        []() { std::cout << "Contract cleanup completed\n"; },
        [](std::exception_ptr e) 
        {
            try { std::rethrow_exception(e); }
            catch (const std::exception& ex) 
            {
                std::cout << "Exception caught: " << ex.what() << "\n";
            }
            bcpp::this_contract::release();
        }
    );

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
