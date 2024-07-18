#pragma once

#include <include/signal_tree.h>


namespace bcpp
{

    template <std::size_t N0, std::size_t N1>
    class signal_map : signal_tree<N0, N1>
    {
    public:

        std::uint64_t create();

        void release(std::uint64_t);

    private:

        template <std::size_t, std::size_t>
        std::uint64_t create();

    }; // signal_map

} // namespace bcpp 


//=============================================================================
template <std::size_t N0, std::size_t N1>
inline std::uint64_t bcpp::signal_map<N0, N1>::create
(
    // locate an unused signal index, mark it as in use and return its index.
)
{
    while (true)
    {
        // select the subtree with the smallest size
        auto smallestSubTree = this->begin();
        for (auto iter = signal_tree<N0, N1>::begin(); iter != signal_tree<N0, N1>::end(); ++iter)
            if (smallestSubTree->size() >= iter->size())
                smallestSubTree = iter;
        // top level of any tree contains only one node
        auto & node = smallestSubTree->nodes_[0];
        // find the smallest counter within that node
        auto smallestCounterIndex = 0;
        auto minCounterValue = node.counter_capacity;
        auto value = node.value_.load();
        for (auto i = 0; i < node.number_of_counters; ++i)
        {
            if ((value & node.counter_mask) < minCounterValue)
            {
                smallestCounterIndex = i;
                minCounterValue = (value & node.counter_mask);
            }
            value >>= node.bits_per_counter;
        }

        while (value < node.counter_capacity)
        {
        }
    }

    return ~0;//create(smallestSubTree);
}

/*
//=============================================================================
template <std::size_t N0, std::size_t N1>
std::uint64_t bcpp::signal_map<N0, N1>::create
(
    tree_level<N0, N1> & treeLevel
)
{
    auto nodeIndex = 0;
    for (auto i = 0; i < treeLevel.
}

*/
