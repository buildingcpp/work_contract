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

#include <library/work_contract.h>

#include <tbb/concurrent_queue.h>
#include <include/boost/lockfree/queue.hpp>
#include <concurrentqueue.h>
#include <mpmc_queue.h>


using namespace bcpp;
using namespace std::chrono;


// it might look a bit odd to hard code the cpus to use in the benchmark
// but one of my test machines has a blend of different cpus and I can't seem
// to disable hyperthreading on that machine via the bios nor the terminal.
// This made ensuring that the benchmark is running on the preferred physical cores 
// a bit difficult so I just did it this way until I have time to write a tool
// to dynamically figure out the optimal cores to use.
//int cores[] = {0,2,4,6,8,10,12,14};
//int cores[] = {0,2,4,6,8,10,12,14,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
int cores[] = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
//int cores[] = {16,20,24,28,17,21,25,29,18,22,26,30,19,23,27,31};
int mainCpu = 0;

std::size_t dummy = 0;

static auto constexpr test_duration = 1s;
static auto constexpr max_tasks = 16384;
static auto constexpr max_threads = std::extent_v<decltype(cores)>;

// containers for gathering stats during test
std::array<std::atomic<std::size_t>, max_tasks> taskExecutionCount;
std::array<std::atomic<std::size_t>, max_threads> threadExecutionCount;
thread_local std::array<std::size_t, max_tasks> tlsExecutionCount;

std::size_t thread_local tlsThreadIndex;
std::size_t thread_local tlsCurrentTaskId;

std::atomic<bool> startTest = false;
std::atomic<bool> endTest = false;
std::vector<std::jthread> testThreads;


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
auto gather_stats
(
    auto const input
) -> std::tuple<std::size_t, long double, long double, long double>
{
    std::size_t total = 0;    
    for (auto const & v : input)
        total += v;
    long double mean = ((long double)total / input.size());
    long double k = 0;
    for (auto const & v : input)
        k += ((v - mean) * (v - mean));
    k /= (input.size() - 1);
    auto sd = std::sqrt(k);
    return {total, mean, sd, sd / mean};
}


//=============================================================================
void print_stats
(
    auto numThreads,
    auto testDurationInSeconds
)
{
    auto [taskTotal, taskMean, taskSd, taskCv] = gather_stats(std::span(taskExecutionCount.data(), taskExecutionCount.size()));
    auto [threadTotal, threadMean, threadSd, threadCv] = gather_stats(std::span(threadExecutionCount.begin(), numThreads));
    std::cout << std::fixed << std::setprecision(3) << taskTotal << 
        (int)((taskTotal / testDurationInSeconds) / numThreads) << "," << taskMean << "," << taskSd << "," << 
            taskCv << "," << threadSd << "," << threadCv << "\n";

    for (auto & _ : taskExecutionCount)
        _ = 0;
    for (auto & _ : threadExecutionCount)
        _ = 0;
}


//=============================================================================
void execute_test
(
    // the actual test.  signal all threads to begin, wait, signal all threads to 
    // stop. take timing.  calculate and print stats.
)
{
    auto numThreads = testThreads.size();

    // start test
    auto startTime = std::chrono::system_clock::now();
    startTest = true;
    // wait for duration of test
    std::this_thread::sleep_for(test_duration);
    endTest = true;
    auto stopTime = std::chrono::system_clock::now();
    // stop worker threads
    for (auto & testThread : testThreads)
    {
        testThread.request_stop();
        testThread.join();
    }

    // test completed
    // gather timing
    auto elapsedTime = (stopTime - startTime);
    auto testDurationInSeconds = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;
    print_stats(testThreads.size(), testDurationInSeconds);
}


//=============================================================================
template <std::size_t N>
std::int32_t seive
(
    // calculate primes up to N
)
{
    static auto constexpr str = "guess what? chicken butt!";
    auto n = 0;
    for (auto i = 0; i < N; ++i)
        n *= std::hash<std::string>()(str);
    return n;
    /*
    auto total = 0;
    static auto constexpr max = N;
    bool seive[max];
    for (auto & _ : seive)
        _ = true;
    seive[0] = seive[1] = false;
    for (auto i = 2ull; i < max; ++i)
        for (auto n = i * 2; n < max; n += i)
            seive[n] = false;
    for (auto n : seive)
        total += (n == true);
    dummy += total;
    return total;
    */
};


//=============================================================================
auto create_worker_threads
(
    // prep the test, create the worker threads, wait until all are ready
    std::size_t numWorkerThreads,
    std::invocable auto && work
)
{
    startTest = false;
    endTest = false;

    testThreads.resize(numWorkerThreads);
    std::atomic<std::size_t> readyThreadCount = 0;
    auto index = 0;
    for (auto & thread : testThreads)
    {
        thread = std::jthread([&readyThreadCount, work, threadId = index]
                (
                ) mutable
                {                 
                    set_cpu_affinity(cores[threadId]);
                    tlsThreadIndex = threadId; 
                    for (auto & _ : tlsExecutionCount)
                        _ = 0;
                    readyThreadCount++;
                    while (!startTest)
                        ;
                    while (!endTest)
                        work();
                    // copy tls stats to global
                    for (auto i = 0; i < max_tasks; ++i)
                    {
                        taskExecutionCount[i] += tlsExecutionCount[i];
                        threadExecutionCount[threadId] += tlsExecutionCount[i];
                    }
                });
        ++index;
    }

    while (readyThreadCount != testThreads.size())
        ;
}


//=============================================================================
auto mpmc_queue_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds test_duration,
    std::invocable auto && task
)
{
    // create queue and fill it with 'max_tasks' tasks
    es::lockfree::mpmc_queue<std::int32_t> queue(max_tasks * 2);

    // to make this test as fair as possible the tasks will be created up front
    // and only the index of the task will be managed by the queue.
    // However, I expect that the typical real world usage case would involve the overhead
    // of creating the task each time.
    std::vector<std::decay_t<decltype(task)>> tasks;
    for (auto i = 0; i < max_tasks; ++i)
    {
        tasks.push_back(task);
        while (!queue.push(i))
            ;
    }

    create_worker_threads(numWorkerThreads, 
            [&]()
            {                        
                std::int32_t taskIndex;
                if (queue.pop(taskIndex))
                {
                    tasks[taskIndex]();                     // execute task
                    while (!queue.push(taskIndex))       // push back onto back of work queue
                        ;
                    tlsExecutionCount[taskIndex]++;   // update stats
                }
            });

    execute_test();
}


//=============================================================================
auto mc_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds test_duration,
    std::invocable auto && task
)
{
    // create queue and fill it with 'max_tasks' tasks
    moodycamel::ConcurrentQueue<std::int32_t> queue(max_tasks * 2);

    // to make this test as fair as possible the tasks will be created up front
    // and only the index of the task will be managed by the queue.
    // However, I expect that the typical real world usage case would involve the overhead
    // of creating the task each time.
    std::vector<std::decay_t<decltype(task)>> tasks;
    for (auto i = 0; i < max_tasks; ++i)
    {
        tasks.push_back(task);
        while (!queue.enqueue(i))
            ;
    }

    create_worker_threads(numWorkerThreads, 
            [&]()
            {                        
                std::int32_t taskIndex;
                if (queue.try_dequeue(taskIndex))
                {
                    tasks[taskIndex]();                     // execute task
                    while (!queue.enqueue(taskIndex))       // push back onto back of work queue
                        ;
                    tlsExecutionCount[taskIndex]++;   // update stats
                }
            });

    execute_test();
}


//=============================================================================
auto tbb_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds test_duration,
    std::invocable auto && task
)
{
    // craete queue and fill it with 'max_tasks' tasks
    tbb::concurrent_queue<std::int32_t> queue;

    // to make this test as fair as possible the tasks will be created up front
    // and only the index of the task will be managed by the queue.
    // However, I expect that the typical real world usage case would involve the overhead
    // of creating the task each time.
    std::vector<std::decay_t<decltype(task)>> tasks;
    for (auto i = 0; i < max_tasks; ++i)
    {
        tasks.push_back(task);
        queue.push(i);
    }

    create_worker_threads(numWorkerThreads, 
            [&]()
            {                        
                std::int32_t taskIndex;
                if (queue.try_pop(taskIndex))
                {
                    tasks[taskIndex]();                     // execute task
                    tlsExecutionCount[taskIndex]++; // update stats
                    queue.push(taskIndex);         // push back onto back of work queue
                }
            });

    execute_test();
}


//=============================================================================
auto boost_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds test_duration,
    std::invocable auto && task
)
{
    // craete queue and fill it with 'max_tasks' tasks
    boost::lockfree::queue<std::int32_t> queue(max_tasks * 2);

    // to make this test as fair as possible the tasks will be created up front
    // and only the index of the task will be managed by the queue.
    // However, I expect that the typical real world usage case would involve the overhead
    // of creating the task each time.
    std::vector<std::decay_t<decltype(task)>> tasks;
    for (auto i = 0; i < max_tasks; ++i)
    {
        tasks.push_back(task);
        while (!queue.push(i))
            ;
    }

    create_worker_threads(numWorkerThreads, 
            [&]()
            {                        
                std::int32_t taskIndex;
                if (queue.pop(taskIndex))
                {
                    tasks[taskIndex]();                     // execute task
                    tlsExecutionCount[taskIndex]++;         // update stats
                    while (!queue.push(taskIndex));         // push back onto back of work queue
                }
            });

    execute_test();
}


//=============================================================================
template <bcpp::synchronization_mode T>
auto work_contract_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds test_duration,
    std::invocable auto && task
)
{
    // create work contracts and schedule all of them (like queuing in a work queue)
    bcpp::work_contract_group workContractGroup((max_tasks * 4) < (1 << 18) ? (1 << 18) : (max_tasks * 4));
    std::vector<bcpp::work_contract> workContracts(max_tasks);

    for (auto i = 0; i < max_tasks; ++i)
    {
        workContracts[i] = workContractGroup.create_contract(
                [&, contractId = i](auto & token)
                {
                    task();                             // execute the task
                    token.schedule();                   // reschedule this contract (like pushing to back of work queue again)
                    tlsExecutionCount[contractId]++;    // update stats
                }, bcpp::work_contract::initial_state::scheduled);
                
        if (!workContracts[i].is_valid())
            std::cout << "ERROR creating work contract\n";
    }

    create_worker_threads(numWorkerThreads, 
            [&]()
            {                        
            //    if constexpr (T == bcpp::synchronization_mode::blocking)
            //        workContractGroup.execute_next_contract(std::chrono::milliseconds(1));
            //    else
                    workContractGroup.execute_next_contract();
            });
    execute_test();
}


//=============================================================================
std::chrono::nanoseconds get_task_duration
(
    // this function tests the actual time it takes to execute a task without the multithreaded frameworks
    std::invocable auto && task
)
{
    std::size_t counter = 0;
    bool volatile start = false;
    bool volatile end = false;
    bool volatile ready = false;
    std::size_t total = 0;
    std::jthread thread([&]()
            {
                ready = true;
                while (!start)
                    ;
                while (!end)
                {
                    counter++;
                    total += task();
                }
            });

    while (!ready)
        ;
    auto startTime = std::chrono::system_clock::now();
    start = true;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    end = true;
    auto endTime = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime) / counter;
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    set_cpu_affinity(mainCpu);

    auto run_test = []<typename T>
    (
        T task,
        std::string title
    )
    {
        std::cout << "\n\nTask " << title << ", average task duration is  " << get_task_duration(task).count() << " ns\n";

        std::cout << "Boost lockfree::queue\nTotal TasksTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
        for (auto i = 2ull; i <= max_threads; ++i)
            boost_test(i, test_duration, task);

        std::cout << "TBB concurrent_queue\nTotal TasksTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
        for (auto i = 2ull; i <= max_threads; ++i)
            tbb_test(i, test_duration, task);

    //    std::cout << "Work Contract (blocking)\nTotal TasksTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
    //    for (auto i = 2; i <= max_threads; ++i)
    //        work_contract_test<bcpp::synchronization_mode::blocking>(i, test_duration, task);

        std::cout << "Strauss mpmc_queue: \nTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
        for (auto i = 2ull; i <= max_threads; ++i)
            mpmc_queue_test(i, test_duration, task);

        std::cout << "Work Contract: \nTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
        for (auto i = 2ull; i <= max_threads; ++i)
            work_contract_test<bcpp::synchronization_mode::non_blocking>(i, test_duration, task);

        std::cout << "MoodyCamel (MPMC)\nTotal Tasks,Tasks per second per thread,task mean,task std dev,task cv,thread std dev,thread cv\n";
        for (auto i = 2ull; i <= max_threads; ++i)
            mc_test(i, test_duration, task);
    };

    run_test(seive<0>, "maximum contention"); // ~1ns
    run_test(seive<1>, "high contention"); // ~266ns
    run_test(seive<64>, "medium contention"); // ~1086ns
    run_test(seive<256>, "low contention"); // ~4248

    return dummy;
}
