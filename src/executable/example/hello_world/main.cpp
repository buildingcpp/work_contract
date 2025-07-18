#include <library/work_contract.h>
#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>


using namespace std::chrono;
using namespace bcpp;


work_contract_group workContractGroup;


//=============================================================================
void example_1
(
    // basic example. create contract, schedule it, sleep to allow time for async completion
)
{
    auto hello_world = [](){std::cout << "hello world. ";};

    std::cout << "example_1: ";
    auto workContract = workContractGroup.create_contract(hello_world);
    workContract.schedule();
    std::this_thread::sleep_for(1s);
} // example_1


//=============================================================================
void example_2
(
    // same as example_1 but contract is created in the scheduled state
)
{    
    auto hello_world = [](){std::cout << "hello world. ";};

    std::cout << "example_2: ";
    auto workContract = workContractGroup.create_contract(hello_world, work_contract::initial_state::scheduled);
    std::this_thread::sleep_for(1s);
} // example_2


//=============================================================================
void example_3
(
    // same as example_2 but add release handler when creating the contract
)
{
    auto hello_world = [](){std::cout << "hello world. ";};
    auto goodbye_world = [](){std::cout << "goodbye world. ";};

    std::cout << "example_3: ";
    auto workContract = workContractGroup.create_contract(hello_world, goodbye_world, work_contract::initial_state::scheduled);
    std::this_thread::sleep_for(1s);
} // example_3


//=============================================================================
void example_4
(
)
{
    std::atomic<bool> done{false};

    auto hello_world = [&](){throw std::runtime_error("runtime error");};
    auto goodbye_world = [&](){std::cout << "goodbye world. "; done = true;};
    auto exception_handler = [](auto & workContractToken, auto currentException)
            {
                try
                {
                    if (currentException)
                        std::rethrow_exception(currentException);
                }
                catch (std::exception const & exception)
                {
                    std::cout << "caught exception: " << exception.what() << "work contract id = " << workContractToken.get_contract_id() << ". ";
                }
                workContractToken.release();
            };

    std::cout << "example_4: ";
    auto workContract = workContractGroup.create_contract(hello_world,
            goodbye_world, exception_handler, work_contract::initial_state::scheduled);
    while (not done)
        ;

} // example_4


//=============================================================================
void example_5
(
)
{
    std::atomic<bool> done{false};

    auto hello_world = [](auto & c){std::cout << "hello world. " << std::flush; c.release();};
    auto goodbye_world = [&](){throw std::runtime_error("runtime error"); done = true;};
    auto exception_handler = [&](auto & workContractToken, auto currentException)
            {
                try
                {
                    if (currentException)
                        std::rethrow_exception(currentException);
                }
                catch (std::exception const & exception)
                {
                    std::cout << "caught exception: " << exception.what() << "work contract id = " << workContractToken.get_contract_id() << ". " << std::flush;
                }
                done = true;
            };

    std::cout << "example_5: ";
    auto workContract = workContractGroup.create_contract(hello_world,
            goodbye_world, exception_handler, work_contract::initial_state::scheduled);
    while (not done)
        ;
} // example_5


//=============================================================================
void run_example
(
    // run the specified example and clean up output with \n
    void (*function)()
)
{
    function();
    std::cout << "\n";
}


//=============================================================================
int main(int, char **)
{

    std::jthread worker([&](std::stop_token stopToken)
            {
                while (not stopToken.stop_requested()) 
                    workContractGroup.execute_next_contract();
            });

    run_example(example_1);
    run_example(example_2);
    run_example(example_3);
    run_example(example_4);
    run_example(example_5);

    return 0;
}