#pragma once

#include "./example_common.h"

#include <thread>


namespace example_2
{

    // example 2:
    //
    // WC is intentionally primitive.  This is to allow the user to have complete control over threading etc and 
    // have access to the lowest latency possible.
    //
    // However, for a lot of purposes, the primitive nature of WC is not expected and developers often expect
    // more convenient wrappers to make code appear more as they are used to seeing it expressed.
    //
    // For instance, in example #1, the task 'char_count_task' serves as the callable which is passed to a work_contract.
    // Thus the code actually works with work_contract wrapping a 'char_count_task' - which can confuse those who are not
    // familiar with the concepts of WC.
    //
    // But it is fairly trivial to encapsulate WC from example #1 to create more familiar looking code.  
    // Example #2 does this with a simple wrapper class called `task`.  It functions exactly as work_contract does in 
    // example #1 but allows the user to:
    //
    // A: work with 'tasks' rather than directly with 'work_contracts' 
    // B: provides the ability to 'join' the task (block until task completion)
    // C: abort the task - by releasing the underlying work contract.

    using namespace example_common;


    //=========================================================================
    // hide the WC details so that it appears that you are dealing with tasks
    // and not 'work contracts'.  Bonus, this allows you to have join() and
    // abort() functionality for your 'task' in a trivial way.
    class task
    {
    public:

        template <typename T>
        task(work_contract_group & wcg, T && task):wc_(wcg.create_contract(std::forward<std::decay_t<T>>(task), 
                work_contract::initial_state::scheduled)){}
        void join(){while (wc_.is_valid()) std::this_thread::yield();}
        void abort(){wc_.release();}
    private:
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
        // NOTE: we could also wrap work_contract_group in the same way as we wrapped work_contract
        // in order to build helper functions to manage the tasks in a more familiar way as well.
        work_contract_group wcg(paths.size());

        // create worker thread pool (parallel processing of input files) which will
        // execute the tasks by selecting and executing the contract associated with
        // the task.
        auto worker_thread_func = [&](auto stopToken){while (not stopToken.stop_requested())  wcg.execute_next_contract();};
        std::vector<std::jthread> threads(numThreads);
        for (auto & thread : threads)
            thread = std::jthread(worker_thread_func);

        // deal with 'task' rather than directly with 'work_contract'
        std::vector<task> tasks;
        for (auto const & path : paths)
            tasks.emplace_back(wcg, char_count_task(path, target));

        // wait for each task to complete (all files are processed)
        for (auto & task : tasks)
            task.join();

        // shut down thread pool
        for (auto & thread : threads)
        {
            thread.request_stop();
            thread.join();
        }
    }

}
