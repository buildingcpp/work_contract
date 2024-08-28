
#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>

#include <iostream>
#include <cstdint>
#include <thread>


//=============================================================================
void example_lock_free
(
    // the body of a work_contract is thread safe.  it can only be invoked by a single
    // worker thread at a time.  There is no need for locking to ensure this.
)
{
    std::cout << "===============================\nexample_lock_free:\n";

    // many 'consumer' threads will compete to be the thread which drains the queue.
    static auto constexpr number_of_worker_threads = 1;
    // number of values to push through the queue
    static auto constexpr number_of_operations = (1 << 20);
    // a lock free queue.  notice, SINGLE CONSUMER even though there are many 'consumer' threads
    bcpp::spsc_fixed_queue<std::uint64_t> queue(number_of_operations * 2);

    // create work contract group
    bcpp::work_contract_group workContractGroup;

    // create async worker thread to service scheduled contracts
    std::vector<std::jthread> workerThreads(number_of_worker_threads);
    std::atomic<std::uint64_t> pendingThreads(number_of_worker_threads);
    for (auto & workerThread : workerThreads)
        workerThread = std::move(std::jthread([&](auto const & stopToken)
        {
            --pendingThreads;
            while (pendingThreads > 0)
                ;

            while (!stopToken.stop_requested()) 
                workContractGroup.execute_next_contract();
        }));

    // create a work contract
    std::uint64_t volatile errorCount = 0;
    auto workFunction = [&, expected = 0](auto & contractToken) mutable
            {
                // work contracts are executed by a single thread (thread safe) regardless of how
                // many threads are servicing the parent work contract group.

                // pop a value from the lock free SPSC queue
                std::uint64_t value;
                if (queue.try_pop(value))
                {
                    // validate that this is the expected next value 
                    if (value != expected)
                    {
                 //       std::cerr << "expected " << expected << " but received " << value << "\r";
                        errorCount = errorCount + 1;
                    }
                    else
                    {
                //        std::cout << "thread id [" << std::this_thread::get_id() << "] popped value " << expected << " of " << number_of_operations << "\r";
                    }
                    ++expected;
                    if (value == number_of_operations)
                        contractToken.release(); // initiate self destruction
                }
                contractToken.schedule();
            };

    auto workContract = workContractGroup.create_contract(workFunction, bcpp::work_contract::initial_state::scheduled);

    // push values into the queue
    while (pendingThreads > 0)
        ;

    auto start = std::chrono::system_clock::now();
    for (auto i = 0; i <= number_of_operations; ++i)
    {
        while (!queue.push(i))
            ;
        workContract.schedule(); // ensure that the work contract is scheduled so that worker threads can empty the queue
    }

    // wait until the work contract has been destroyed (after the last value has been consumed from the queue)
    while (workContract.is_valid())
        ;
    auto finish = std::chrono::system_clock::now();
    std::cout << (std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count() / number_of_operations) << "ns\n\n\n";

    // stop the worker thread
    for (auto & workerThread : workerThreads)
    {
        workerThread.request_stop();
        workerThread.join();
    }

    // display results.  There should be no errors
    if (errorCount > 0)
        std::cout << "\033[A\33[2KT\r" << errorCount << " ERROR(s) encountered\n";
    else
        std::cout << "\033[A\33[2KT\rTest successful\n";
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    example_lock_free();

    return 0;
}
