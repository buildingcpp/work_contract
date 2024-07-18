#include "./enqueue_test.h"
#include <concurrentqueue.h>
#include <mpmc_queue.h>

#include <library/work_contract.h>
#include <include/mpmc_queue.h>

#include <atomic>
#include <thread>
#include <vector>
#include <concepts>
#include <iostream>


namespace
{
    int cores[] = {16,20,24,28,17,21,25,29,18,22,26,30,19,23,27,31};
    int mainCpu = 0;

    static auto constexpr total_ops = (1 << 18);


    //==============================================================================
    bool set_cpu_affinity
    (
        int value
    )
    {
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        CPU_SET(value, &cpuSet);
        return (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet) == 0);
    }

}


//=============================================================================
auto signal_tree_enqueue_test
(
    std::size_t threadCount
)
{
    auto opsPerThread = (total_ops / threadCount);

    // create work contracts and schedule all of them (like queuing in a work queue)

    auto signalTree = std::make_unique<bcpp::signal_tree<total_ops>>();

    std::atomic<bool> startEnqueue = false;
    std::atomic<bool> startDequeue = false;

    std::atomic<std::size_t> readyEnqueueCount = 0;
    std::atomic<std::size_t> readyDequeueCount = 0;

    // threads
    std::vector<std::jthread> threads(threadCount);
    for (auto i = 0; i < threadCount; ++i)
    {
        threads[i] = std::jthread([&, threadIndex=i]
                (
                ) mutable
                {                 
                    // set core affinity
                    set_cpu_affinity(cores[threadIndex]);

                    // wait for test to start
                    readyEnqueueCount++;
                    while (!startEnqueue)
                        ;

                    // start enqueue
                    auto n = threadIndex;
                    for (auto i = 0; i < opsPerThread; ++i)
                    {
                        signalTree->set(n);
                        n += threadCount;
                    }

                    // end enqueue phase
                    readyEnqueueCount--;
                    readyDequeueCount++;
                    while (!startDequeue)
                        ;
                    
                    // dequeue.  select bias bits which tend to keep threads
                    // from contending.  This is an advantage that queue's can't really duplicate.
                    for (auto i = 0; i < opsPerThread; ++i)
                        signalTree->select(threadIndex * opsPerThread);

                    readyDequeueCount--;
                });
    }

    // wait until all threads are ready to start enqueue
    while (readyEnqueueCount != threads.size())
        ;
    auto startEnqueueTime = std::chrono::system_clock::now();
    startEnqueue = true;
    // wait for all threads to complete enqueue
    while (readyEnqueueCount != 0)
        ;
    // time enqueue
    auto endEnqueueTime = std::chrono::system_clock::now();
    auto elapsedEnqueueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEnqueueTime - startEnqueueTime);
    double enqueueLatency = (double)(elapsedEnqueueTime.count() / total_ops);

    // wait until all threads are ready to start dequeue
    while (readyDequeueCount != threads.size())
        ;
    auto startDequeueTime = std::chrono::system_clock::now();
    startDequeue = true;
    // wait for all threads to complete dequeue
    while (readyDequeueCount != 0)
        ;
    // time dequeue
    auto endDequeueTime = std::chrono::system_clock::now();

    auto elapsedDequeueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDequeueTime - startDequeueTime);
    double dequeueLatency = ((double)elapsedDequeueTime.count() / total_ops);

    // report results
    std::cout << (std::uint64_t)(1'000'000'000 / (enqueueLatency + dequeueLatency)) <<
            ", " << (std::uint64_t)(enqueueLatency) << 
            ", " << (std::uint64_t)(dequeueLatency) <<
            "\n";
}


//=============================================================================
auto moody_camel_enqueue_test
(
    std::size_t threadCount
)
{
    auto opsPerThread = (total_ops / threadCount);
    moodycamel::ConcurrentQueue<std::int32_t> queue(total_ops);

    std::atomic<bool> startEnqueue = false;
    std::atomic<bool> startDequeue = false;

    std::atomic<std::size_t> readyEnqueueCount = 0;
    std::atomic<std::size_t> readyDequeueCount = 0;

    std::atomic<std::uint64_t> pushTotal;
    std::atomic<std::uint64_t> popTotal;

    // threads
    std::vector<std::jthread> threads(threadCount);
    for (auto i = 0; i < threadCount; ++i)
    {
        threads[i] = std::jthread([&, threadIndex=i]
                (
                ) mutable
                {                 
                    // set core affinity
                    set_cpu_affinity(cores[threadIndex]);

                    // wait for test to start
                    readyEnqueueCount++;
                    while (!startEnqueue)
                        ;

                    // start enqueue
                    std::uint64_t localPushTotal = 0;
                    for (auto i = 0; i < opsPerThread; ++i)
                    {
                        while (!queue.enqueue(threadIndex))
                            ;
                        localPushTotal += threadIndex;
                    }
                    // end enqueue phase
                    pushTotal += localPushTotal;
                    readyEnqueueCount--;

                    readyDequeueCount++;
                    while (!startDequeue)
                        ;

                    std::int32_t valuePopped;
                    std::uint64_t localPopTotal = 0;
                    for (auto i = 0; i < opsPerThread; ++i)
                    {
                        while (!queue.try_dequeue(valuePopped))
                            ;
                        localPopTotal += valuePopped;
                    }
                    popTotal += localPopTotal;
                    readyDequeueCount--;
                });
    }


    // wait until all threads are ready to start enqueue
    while (readyEnqueueCount != threads.size())
        ;

    // begin enqueue
    auto startEnqueueTime = std::chrono::system_clock::now();
    startEnqueue = true;
    // wait for all threads to complete enqueue
    while (readyEnqueueCount != 0)
        ;
    // time enqueue
    auto endEnqueueTime = std::chrono::system_clock::now();
    auto elapsedEnqueueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEnqueueTime - startEnqueueTime);
    double enqueueLatency = (double)(elapsedEnqueueTime.count() / total_ops);

    // wait until all threads are ready to start dequeue
    while (readyDequeueCount != threads.size())
        ;

    // begin dequeue
    auto startDequeueTime = std::chrono::system_clock::now();
    startDequeue = true;
    // wait for all threads to complete dequeue
    while (readyDequeueCount != 0)
        ;
    // time dequeue
    auto endDequeueTime = std::chrono::system_clock::now();
    auto elapsedDequeueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDequeueTime - startDequeueTime);
    double dequeueLatency = ((double)elapsedDequeueTime.count() / total_ops);

    if (pushTotal != popTotal)
        std::cout << "ERROR: push total = " << pushTotal << ", pop total = " << popTotal << " -  ";

    // report results
    std::cout << (std::uint64_t)(1'000'000'000 / (enqueueLatency + dequeueLatency)) <<
            ", " << (std::uint64_t)(enqueueLatency) << 
            ", " << (std::uint64_t)(dequeueLatency) <<
            "\n";
}


//=============================================================================
auto mpmc_queue
(
    std::size_t threadCount
)
{
    auto opsPerThread = (total_ops / threadCount);
    es::lockfree::mpmc_queue<std::int32_t> queue(total_ops);

    std::atomic<bool> startEnqueue = false;
    std::atomic<bool> startDequeue = false;

    std::atomic<std::size_t> readyEnqueueCount = 0;
    std::atomic<std::size_t> readyDequeueCount = 0;

    // threads
    std::vector<std::jthread> threads(threadCount);
    for (auto i = 0; i < threadCount; ++i)
    {
        threads[i] = std::jthread([&, threadIndex=i]
                (
                ) mutable
                {                 
                    // set core affinity
                    set_cpu_affinity(cores[threadIndex]);

                    // wait for test to start
                    readyEnqueueCount++;
                    while (!startEnqueue)
                        ;

                    // start enqueue
                    for (auto i = 0; i < opsPerThread; ++i)
                    {
                        while (!queue.enqueue(i))
                            ;
                    }
                    // end enqueue phase
                    readyEnqueueCount--;
                    readyDequeueCount++;
                    while (!startDequeue)
                        ;

                    std::int32_t valuePopped;
                    for (auto i = 0; i < opsPerThread; ++i)
                        while (!queue.dequeue(valuePopped))
                            ;
                        
                    readyDequeueCount--;
                });
    }


    // wait until all threads are ready to start enqueue
    while (readyEnqueueCount != threads.size())
        ;

    // begin enqueue
    auto startEnqueueTime = std::chrono::system_clock::now();
    startEnqueue = true;
    // wait for all threads to complete enqueue
    while (readyEnqueueCount != 0)
        ;
    // time enqueue
    auto endEnqueueTime = std::chrono::system_clock::now();
    auto elapsedEnqueueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEnqueueTime - startEnqueueTime);
    double enqueueLatency = (double)(elapsedEnqueueTime.count() / total_ops);

    // wait until all threads are ready to start dequeue
    while (readyDequeueCount != threads.size())
        ;

    // begin dequeue
    auto startDequeueTime = std::chrono::system_clock::now();
    startDequeue = true;
    // wait for all threads to complete dequeue
    while (readyDequeueCount != 0)
        ;
    // time dequeue
    auto endDequeueTime = std::chrono::system_clock::now();
    auto elapsedDequeueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDequeueTime - startDequeueTime);
    double dequeueLatency = ((double)elapsedDequeueTime.count() / total_ops);

    // report results
    std::cout << (std::uint64_t)(1'000'000'000 / (enqueueLatency + dequeueLatency)) <<
            ", " << (std::uint64_t)(enqueueLatency) << 
            ", " << (std::uint64_t)(dequeueLatency) <<
            "\n";
}


//=============================================================================
auto mpsc_enqueue_test
(
    std::size_t threadCount
)
{
    auto opsPerThread = (total_ops / threadCount);
    bcpp::mpmc_queue<std::int32_t, total_ops> queue;

    std::atomic<bool> startEnqueue = false;
    std::atomic<bool> startDequeue = false;

    std::atomic<std::size_t> readyEnqueueCount = 0;
    std::atomic<std::size_t> readyDequeueCount = 0;

    std::atomic<std::uint64_t> pushTotal;
    std::atomic<std::uint64_t> popTotal;

    // threads
    std::vector<std::jthread> threads(threadCount);
    for (auto i = 0; i < threadCount; ++i)
    {
        threads[i] = std::jthread([&, threadIndex = i]
                (
                ) mutable
                {                 
                    // set core affinity
                    set_cpu_affinity(cores[threadIndex]);

                    // wait for test to start
                    readyEnqueueCount++;
                    while (!startEnqueue)
                        ;

                    std::int32_t valuePopped;
                    std::uint64_t localPopTotal = 0;

                    // start enqueue
                    std::uint64_t localPushTotal = 0;
                    std::uint64_t totalPopsRemaining = opsPerThread;

                    auto valueToPush = (threadIndex + 1);
                    for (auto i = 0; i < opsPerThread; ++i)
                    {
                        while (!queue.push(valueToPush))
                            ;
                   //     localPushTotal += valueToPush;
                   //     valueToPush += (threadIndex + 1);
                    }
                    // end enqueue phase
                //    pushTotal += localPushTotal;
                    readyEnqueueCount--;

                    readyDequeueCount++;
                    while (!startDequeue)
                        ;

                    while (totalPopsRemaining > 0)
                    {
                        if (queue.pop(valuePopped))
                        {
                        //    localPopTotal += valuePopped;
                            --totalPopsRemaining;
                        }
                    }
                //    popTotal += localPopTotal;
                    readyDequeueCount--;
                });
    }


    // wait until all threads are ready to start enqueue
    while (readyEnqueueCount != threads.size())
        ;

    // begin enqueue
    auto startEnqueueTime = std::chrono::system_clock::now();
    startEnqueue = true;
    // wait for all threads to complete enqueue
    while (readyEnqueueCount != 0)
        ;
    // time enqueue
    auto endEnqueueTime = std::chrono::system_clock::now();
    auto elapsedEnqueueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEnqueueTime - startEnqueueTime);
    double enqueueLatency = (double)(elapsedEnqueueTime.count() / total_ops);

    // wait until all threads are ready to start dequeue
    while (readyDequeueCount != threads.size())
        ;

    // begin dequeue
    auto startDequeueTime = std::chrono::system_clock::now();
    startDequeue = true;
    // wait for all threads to complete dequeue
    while (readyDequeueCount != 0)
        ;
    // time dequeue
    auto endDequeueTime = std::chrono::system_clock::now();
    auto elapsedDequeueTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDequeueTime - startDequeueTime);
    double dequeueLatency = ((double)elapsedDequeueTime.count() / total_ops);

    if (!queue.empty())
        std::cout << "Error: queue not empty\n";

    if (pushTotal != popTotal)
        std::cout << "ERROR: push total = " << pushTotal << ", pop total = " << popTotal << " -  ";

    // report results
    std::cout << (std::uint64_t)(1'000'000'000 / (enqueueLatency + dequeueLatency)) <<
            ", " << (std::uint64_t)(enqueueLatency) << 
            ", " << (std::uint64_t)(dequeueLatency) <<
            "\n";
}


//=============================================================================
void enqueue_test
(
)
{

 //   std::cout << "\nSignal tree: \n";
 //   for (auto i = 2; i <= 16; ++i)
 //       signal_tree_enqueue_test(i);

    for (auto i = 0; i < 1024; ++i)
    {
        std::cout << "\nMPSC queue: \n";
        for (auto i = 2; i <= 16; ++i)
            mpsc_enqueue_test(i);
    }

    std::cout << "\nes MPMC queue: \n";
    for (auto i = 2; i <= 16; ++i)
        mpmc_queue(i);

    std::cout << "\nMoody concurrent queue: \n";
    for (auto i = 2; i <= 16; ++i)
        moody_camel_enqueue_test(i);
}