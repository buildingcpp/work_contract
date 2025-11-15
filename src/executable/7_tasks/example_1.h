#pragma once

#include "./example_common.h"

#include <thread>


namespace example_1
{

    using namespace example_common;

    struct task
    {
        template <typename T>
        task(work_contract_group & wcg, T && task):wc_(wcg.create_contract(std::forward<std::decay_t<T>>(task), 
                work_contract::initial_state::scheduled)){}
        void join(){while (wc_.is_valid()) std::this_thread::yield();}
        void abort(){wc_.release();}
        work_contract wc_;
    };


    //=========================================================================
    void run
    (
        // create one task per file in the specified directory
        // each task will scan each file in chunks of 256 bytes and count the number
        // of times that the specified character (the target) appeared in that file.
        // Creates a thread pool to do the work concurrently (and in parallel if
        // more than one thread and more than one file).

        std::filesystem::path directory,    // parent directory of files to process
        char target,                        // letter to count
        std::uint64_t numThreads            // number of worker threads
    )
    {
        // create list of files to process 
        auto paths = load_paths(directory);

        // create work contract group with sufficient capacity for the
        // number of tasks to be created (one per file).
        work_contract_group wcg(paths.size());

        // create worker thread pool (parallel processing of input files) which will
        // execute the tasks by selecting and executing the contract associated with
        // the task.
        auto worker_thread_func = [&](auto stopToken){while (not stopToken.stop_requested())  wcg.execute_next_contract();};
        std::vector<std::jthread> threads(numThreads);
        for (auto & thread : threads)
            thread = std::jthread(worker_thread_func);

        // create one contract per task and provide the task as the primary work callable to that contract.
        std::vector<work_contract> contracts;
        for (auto const & path : paths)
            contracts.push_back(wcg.create_contract(char_count_task(path, target), work_contract::initial_state::scheduled));

        // wait for each contract to expire (all files are processed)
        for (auto & contract : contracts)
            while (contract.is_valid())
                std::this_thread::yield();

        // shut down thread pool
        for (auto & thread : threads)
        {
            thread.request_stop();
            thread.join();
        }
    }

}
