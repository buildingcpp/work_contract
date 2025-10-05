


#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <vector>
#include <thread>
#include <cmath>
#include <iomanip>
#include <span>
#include <ratio>
#include <functional>

#include <include/jthread.h>
#include <include/signal_tree.h>

// it might look a bit odd to hard code the cpus to use in the benchmark
// but one of my test machines has a blend of different cpus and I can't seem
// to disable hyperthreading on that machine via the bios nor the terminal.
// This made ensuring that the benchmark is running on the preferred physical cores 
// a bit difficult so I just did it this way until I have time to write a tool
// to dynamically figure out the optimal cores to use.
//int cores[] = {0,2,4,6,8,10,12,14};
//int cores[] = {0,2,4,6,8,10,12,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
//int cores[] = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
int cores[] = {16,20,24,28,17,21,25,29,18,22,26,30,19,23,27,31};
int mainCpu = 0;


//==============================================================================
bool set_cpu_affinity
(
    int value
)
{
#ifdef __linux__
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(value, &cpuSet);
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet) == 0);
#else
    // macOS doesn't support CPU affinity via pthread_setaffinity_np
    (void)value;
    return true;
#endif
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    static auto constexpr max_signal_count = 1000000;

    set_cpu_affinity(mainCpu);

    for (auto num_threads = 1ull; num_threads <= 10; num_threads++)
    {
        using signal_tree_type = bcpp::signal_tree<max_signal_count>;
        auto signalTree = std::make_unique<signal_tree_type>();

        std::atomic<std::uint64_t> totalSet = 0;
        std::atomic<std::uint64_t> totalSelected = 0;

        std::atomic<bool> startTest = false;
        std::atomic<std::uint64_t> activeThreadCount = 0;

        std::atomic<std::uint64_t> setterCount{0};
        std::atomic<std::uint64_t> resetterCount{0};
        auto test = [&]()
                {
                    auto threadIndex = activeThreadCount++;
                    set_cpu_affinity(cores[threadIndex]);
                    auto localTotalSet = 0;
                    auto localTotalSelected = 0;

                    while (!startTest)
                        ;

                    auto opsPerThread = (signal_tree_type::capacity / num_threads);

                    // set all signals
                    auto base = (threadIndex * opsPerThread);
                    for (auto i = 0ull; i < opsPerThread; ++i)
                    {
                        auto [wasSet, isSet] = signalTree->set(base + i); // linear set from base
                        localTotalSet += isSet;
                    }
                    
                    // set all signals
                    base = threadIndex;
                    for (auto i = 0ull; i < opsPerThread; ++i)
                    {
                      //auto [signalNumber, emptyAfterSelect] = signalTree->select(base + i); // linear reset (no contention)
                        auto [signalNumber, emptyAfterSelect] = signalTree->select(base); base += num_threads; // stride reset (maximum contention)
                        if (signalNumber == bcpp::invalid_signal_index)
                            std::cout << "invalid signal returned\n";
                        localTotalSelected += (signalNumber != bcpp::invalid_signal_index);
                    }

                    totalSet += localTotalSet;
                    totalSelected += localTotalSelected;
                    activeThreadCount--;
                };

        std::vector<bcpp::detail::jthread> threads(num_threads);
        for (auto & thread : threads)
            thread = bcpp::detail::jthread(test);

        while (activeThreadCount != num_threads)
            ;

        auto start = std::chrono::system_clock::now();
        startTest = true;
        while (activeThreadCount)
            ;
        auto finish = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start);
        auto sec = ((double)elapsed.count() / std::nano::den);    
    
        if (totalSelected != totalSet)
              std::cout << "Error - total signals set = " << totalSet << ", total signals detected = " << totalSelected << "\n";
        //else
        //    std::cout << "Success - total singals set and detected = " << totalSelected << "\n";

 //       std::cout << "Elapsed time: " << sec << " sec\n";
        std::cout << "num threads = " << num_threads << ", sets/second = " << (std::uint64_t)(totalSet / sec) << "\n";
    //    std::cout << ((double)elapsed.count() / totalSelected) << " ns per operation\n";
    }
    return 0;
}
