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
#include <fmt/format.h>

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

static auto constexpr test_duration = 1s;
static auto constexpr max_tasks = (1 << 13);
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

#include "./test_harness.h"


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
    std::cout <<fmt::format("{:<15}{:<20}{:<25}{:<10.4f}{:<10.4f}\n", numThreads, taskTotal, (int)((taskTotal / testDurationInSeconds) / numThreads), taskCv, threadCv);

    for (auto & _ : taskExecutionCount)
        _ = 0;
    for (auto & _ : threadExecutionCount)
        _ = 0;
}


//=============================================================================
template <std::size_t N>
auto hash_task()
{
    static auto constexpr str = "guess what? chicken butt!";
    auto volatile n = 0;
    for (auto i = 0ull; i < N; ++i)
        n *= std::hash<std::string>()(str);
    return n;
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
auto execute_test
(
    std::size_t numWorkerThreads,
    auto && threadFunction
)
{
    create_worker_threads(numWorkerThreads, threadFunction);

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
template <algorithm T>
auto test_algorithm
(
    std::size_t numWorkerThreads,
    std::invocable auto && task
)
{
    test_harness<T, std::decay_t<decltype(task)>> testHarness(max_tasks);
    for (auto i = 0; i < max_tasks; ++i)
        testHarness.add_task(task);
    execute_test(numWorkerThreads, [&](){testHarness.process_next_task();});
}


//=============================================================================
auto get_task_duration
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
    return ((double)std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count() / counter);
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
        std::string green = "\033[1m";
        std::string defaultColor = "\033[0m";
        std::string line = "==================================================================================\n";
        std::cout << fmt::format("\n\nTask {}, average task duration is {:.2f} ns\n", title, get_task_duration(task));
        auto header = fmt::format("{:<15}{:<20}{:<25}{:<10}{:<10}\n", "Thread Count:", "Tasks per Second:", "Tasks per Thread/sec:", "Task cv:", "Thread cv:");

        std::cout << "\n" << green << line << "TBB concurrent_queue:\n" << header << line << defaultColor;
        for (auto i = 2ull; i <= max_threads; ++i)
            test_algorithm<algorithm::tbb>(i, task);

        std::cout << "\n" << green << line << "Strauss MPMC queue:\n" << header << line << defaultColor;
        for (auto i = 2ull; i <= max_threads; ++i)
            test_algorithm<algorithm::es>(i, task);

        std::cout << "\n" << green << line << "MoodyCamel ConcurrentQueue:\n" << header << line << defaultColor;
        for (auto i = 2ull; i <= max_threads; ++i)
            test_algorithm<algorithm::moody_camel>(i, task);

        std::cout << "\n" << green << line << "Work Contract:\n" << header << line << defaultColor;
        for (auto i = 2ull; i <= max_threads; ++i)
            test_algorithm<algorithm::work_contract>(i, task);

        std::cout << "\n" << green << line << "Blocking Work Contract:\n" << header << line << defaultColor;
        for (auto i = 2ull; i <= max_threads; ++i)
            test_algorithm<algorithm::blocking_work_contract>(i, task);
    };

    run_test(hash_task<0>, "maximum contention"); // approx 1.5ns
    run_test(hash_task<1>, "high contention"); // approx 17ns
    run_test(hash_task<64>, "medium contention"); // ~1100ns
    run_test(hash_task<256>, "low contention"); // ~4100ns

    return 0;
}
