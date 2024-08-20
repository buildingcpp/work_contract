
#include <library/work_contract.h>
#include <iostream>
#include <thread>


//=============================================================================
void example_exception
(
)
{
    std::cout << "===============================\example_exception:\n";
    // create work contract tree
    bcpp::work_contract_tree workContractTree;

    // create async worker thread to service scheduled contracts
    std::jthread workerThread([&](auto const & stopToken){while (!stopToken.stop_requested()) workContractTree.execute_next_contract();});

    // create a work contract
    auto workFunction = [&, n = 0]
            (
                // increment n by one with each invocation
                // throw exception when n is odd
                auto & self
            ) mutable 
            {
                ++n;
                std::cout << "n = " << n << "\n"; 
                if ((n % 2) == 1) 
                    throw std::runtime_error("n is odd");  
                self.schedule();
            };

    auto exceptionHandler = [&, exceptionCount = 0]
            (
                // handle the exception and less the contract re-schedule
                // upon the third exception, give up and release the contract instead
                auto & exceptionToken
            ) mutable
            {
                try
                {
                    if (exceptionToken.get_exception())
                        std::rethrow_exception(exceptionToken.get_exception());
                }
                catch (std::exception const & exception)
                {
                    std::cout << "work contract [id = " << exceptionToken.get_contract_id() << "] caught exception: " << exception.what() << "\n";
                }

                if (++exceptionCount >= 3)
                {
                    std::cout << "third exception.  releasing work contract\n";
                    exceptionToken.release();
                }
                exceptionToken.schedule();
            };
    auto workContract = workContractTree.create_contract(workFunction, [](){}, exceptionHandler, bcpp::work_contract::initial_state::scheduled);

    // wait until contract has been invoked
    while (workContract)
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
    example_exception();

    return 0;
}
