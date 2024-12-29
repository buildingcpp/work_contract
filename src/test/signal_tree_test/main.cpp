
#include <include/signal_tree.h>
#include <iostream>
#include <utility>
#include <thread>
#include <atomic>


//=============================================================================
template <std::size_t desired_signal_tree_capacity>
int test_multi_threaded
(
)
{
    try
    {
        // construct signal tree
        std::cout << "signal tree: test_multi_threaded<" << desired_signal_tree_capacity << ">\n";
        using signal_tree_type = bcpp::signal_tree<desired_signal_tree_capacity>;
        auto signalTree = std::make_unique<signal_tree_type>();
        static auto constexpr actual_signal_tree_capacity = signal_tree_type::capacity;

        auto num_threads = std::thread::hardware_concurrency();

        // do multithreaded set of all signals in the tree
        std::vector<std::jthread> threads(num_threads);
        auto threadIndex = 0ull;
        std::atomic<std::size_t> setErrorCount{0};
        for (auto & thread : threads)
        {
            thread = std::move(std::jthread([&, n = threadIndex]() mutable
                    {
                        while (n < actual_signal_tree_capacity)
                        {
                            auto [_, success] = signalTree->set(n);
                            if (!success)
                                setErrorCount++;
                            n += num_threads;
                        }
                    }));
            ++threadIndex;
        }
        for (auto & thread : threads)
            thread.join();
        if (setErrorCount != 0)
            throw std::runtime_error("multithreaded set failures detected");

        // do multithreaded select of all signals in the tree
        threadIndex = 0;
        std::atomic<std::size_t> selectErrorCount{0};
        for (auto & thread : threads)
        {
            thread = std::move(std::jthread([&, n = threadIndex]() mutable
                    {
                        while (n < actual_signal_tree_capacity)
                        {
                            auto [selectedId, _] = signalTree->select(n);
                            if (selectedId != n)
                                selectErrorCount++;
                            n += num_threads;
                        }
                    }));
            ++threadIndex;
        }
        for (auto & thread : threads)
            thread.join();
        if (selectErrorCount != 0)
            throw std::runtime_error("multithreaded select failures detected");

        //=========================================================================
        // after selecting all signals the tree should be empty.  Subsequent calls to 
        // select should return invalid signal id (~0ull)
        if (!signalTree->empty())
            throw std::runtime_error("signal tree expected to be empty after all signals selected");

        if (auto [signalId, transitionedToEmpty] = signalTree->select(0); signalId != ~0ull)
            throw std::runtime_error("empty signal tree should return invalid signal id when select is called");
        std::cout << "signal tree: empty tree correctly returns invalid signal id to select call\n";

        std::cout << "signal tree: all tests passed\n";
        return 0;
    }
    catch (std::exception const & exception)
    {
        std::cerr << "ERROR: " << exception.what() << "\n";
        return -1;
    }
}


//=============================================================================
template <std::size_t desired_signal_tree_capacity>
int test_single_threaded
(
)
{
    try
    {
        // constrcut tree
        std::cout << "signal tree: test_single_threaded<" << desired_signal_tree_capacity << ">\n";
        using signal_tree_type = bcpp::signal_tree<desired_signal_tree_capacity>;
        auto signalTree = std::make_unique<signal_tree_type>();
        static auto constexpr actual_signal_tree_capacity = signal_tree_type::capacity;

        //=========================================================================
        // tree should start out 'empty == true'
        if (!signalTree->empty())
                throw std::runtime_error("tree should be empty");

        std::cout << "signal tree: starts empty\n";

        //=========================================================================
        // populate tree.  first signal set should return that the tree was empty 
        // prior to the call to set.  all other calls to set additional signals should
        // return that the tree was NOT empty prior to that call to set.
        for (auto i = 0ull; i < actual_signal_tree_capacity; ++i)
        {
            auto wasEmpty = signalTree->empty();
            auto [transitionedToNonEmpty, success] = signalTree->set(i);
            if (wasEmpty != transitionedToNonEmpty)
                throw std::runtime_error("empty test failure");

            if (!success)
                throw std::runtime_error("signal tree set() failed");
        }
        std::cout << "signal tree: all signals set correctly\n";

        //=========================================================================
        // repeat above.  But since signals are all set, each call to set() should
        // return 'not transitioned from empty' and 'failure to set'
        for (auto i = 0ull; i < actual_signal_tree_capacity; ++i)
        {
            auto [transitionedToNonEmpty, success] = signalTree->set(i);
            if (transitionedToNonEmpty)
                throw std::runtime_error("signal tree set returned transition state error");

            if (success)
                throw std::runtime_error("signal tree set() should have failed for already set signal");

        }
        std::cout << "signal tree: all signals set correctly\n";

        //=========================================================================
        // select all elements out of tree.  Do so using the bias hint to target exactly
        // one specific signal.  verify that the select signal does match the bias hint.
        // ensure that invalid signal id (~0ull) is never returned.
        // ensure that 'transitionedToEmpty' is true ONLY when the last signal has been
        // selected from the tree (the selection caused tree to become empty)
        std::vector<bool> selected(actual_signal_tree_capacity);
        std::fill(selected.begin(), selected.end(), false);
        for (auto i = 0ull; i < actual_signal_tree_capacity; ++i)
        {
            auto [signalId, transitionedToEmpty] = signalTree->select(i);
            if (signalId >= actual_signal_tree_capacity)
                throw std::runtime_error("selected invalid signal index");

            // signal should not have been previously selected
            if (selected[signalId] == true)
                throw std::runtime_error("signal selected twice without being set twice");

            selected[signalId] = true;
            // signal selected should match the bias hint provided
            if (signalId != i)
                throw std::runtime_error("select returned wrong signal id");

            if (signalTree->empty())
            {
                if (transitionedToEmpty != true)
                    throw std::runtime_error("empty tree returned transitionedToEmpty=false after selection");
            }
            else
            {
                // tree is not empty after selection. transitionedToEmpty should be false
                if (transitionedToEmpty == true)
                    throw std::runtime_error("non empty tree returned transitionedToEmpty=true after selection");
            }
        }
        std::cout << "signal tree: all signals seletected correctly\n";

        //=========================================================================
        // after selecting all signals the tree should be empty.  Subsequent calls to 
        // select should return invalid signal id (~0ull)
        if (!signalTree->empty())
            throw std::runtime_error("signal tree expected to be empty after all signals selected");

        if (auto [signalId, transitionedToEmpty] = signalTree->select(0); signalId != ~0ull)
            throw std::runtime_error("empty signal tree should return invalid signal id when select is called");
        std::cout << "signal tree: empty tree correctly returns invalid signal id to select call\n";

        std::cout << "signal tree: all tests passed\n";
        return 0;
    }
    catch (std::exception const & exception)
    {
        std::cerr << "ERROR: " << exception.what() << "\n";
        return -1;
    }
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    if (!test_single_threaded<64>() == 0)
        return -1;
    if (!test_single_threaded<512>() == 0)
        return -1;
    if (!test_single_threaded<8192>() == 0)
        return -1;
    if (!test_single_threaded<1'000'000>() == 0)
        return -1;
    if (!test_single_threaded<100'000'000>() == 0)
        return -1;

    if (!test_multi_threaded<64>() == 0)
        return -1;
    if (!test_multi_threaded<512>() == 0)
        return -1;
    if (!test_multi_threaded<8192>() == 0)
        return -1;
    if (!test_multi_threaded<1'000'000>() == 0)
        return -1;    
    if (!test_multi_threaded<100'000'000>() == 0)
        return -1;    
    return 0;
}
