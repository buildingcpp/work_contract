


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
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(value, &cpuSet);
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet) == 0);
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    {
        bcpp::signal_tree<32768, 1> s;
        for (auto i = 0; i < 32768; ++i)
        {
            s.set(i);
        }
        for (auto i = 0; i < 32768; ++i)
        {
            if (i == 684)
                int y = 9;
            auto n = s.select(i);
            if (n != i)
                int y = 9;
        }
    }

    set_cpu_affinity(mainCpu);

    using signal_tree_type = bcpp::signal_tree<1 << 11>;
    auto signalTree = std::make_unique<signal_tree_type>();
    std::cout << "overhead for tree = " << sizeof(*signalTree) << "\n";
    static auto constexpr num_threads = 16;
    static auto constexpr num_loops = ((100'000'000 + signal_tree_type::capacity() - 1) / signal_tree_type::capacity());

    std::atomic<std::size_t> totalSet = 0;
    std::atomic<std::size_t> totalSelected = 0;

    std::vector<std::function<void()>> wc(signal_tree_type::capacity());
    for (auto & x : wc)
        x = [](){};

    std::atomic<bool> startTest = false;
    std::atomic<std::size_t> activeThreadCount = 0;
    auto test = [&]()
            {
                auto threadIndex = activeThreadCount++;
                set_cpu_affinity(cores[threadIndex]);
                auto localTotalSet = 0;
                auto localTotalSelected = 0;

                while (!startTest)
                    ;

                for (auto loopIndex = 0; loopIndex < num_loops; ++loopIndex)
                {
                    // add signals 
                    for (auto i = threadIndex; i < signalTree->capacity(); i += num_threads)
                        localTotalSet += signalTree->set(i);
                    
                    // remove signals using bias
                    for (auto i = threadIndex; i < signalTree->capacity(); i += num_threads)
                    {
                        if (auto signalIndex = signalTree->select(i); signalIndex != ~0)
                        {
                            wc[signalIndex]();
                            localTotalSelected += (signalIndex == i);
                        }
                    }

/*
                    // remove signals randomly
                    while (!signalTree->empty())
                    {
                        auto signalIndex = signalTree->select();
                        localTotalSelected += (signalIndex != ~0);
                    }

                    while (localTotalSet != localTotalSelected)
                        ;
*/
                }

                totalSet += localTotalSet;
                totalSelected += localTotalSelected;
                activeThreadCount--;
            };

    std::vector<std::jthread> threads(num_threads);
    for (auto & thread : threads)
        thread = std::jthread(test);

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
    else
        std::cout << "Success - total singals set and detected = " << totalSelected << "\n";

    std::cout << "Elapsed time: " << sec << " sec\n";
    std::cout << (((double)totalSet / std::mega::num) / sec) << " MB operations/sec\n";

    std::cout << ((double)elapsed.count() / totalSelected) << " ns per operation\n";

    std::cout << "Test complete\n";
    return 0;
}
