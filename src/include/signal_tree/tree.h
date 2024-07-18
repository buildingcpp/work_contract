#pragma once

#include "./level.h"


namespace bcpp
{

    namespace implementation::signal_tree
    {
    
        //=====================================================================
        // TODO: hack.  write better when have time
        template <std::uint64_t N = 64>
        static std::uint64_t consteval select_tree_size
        (
            std::uint64_t requested
        ) 
        {
            constexpr std::array<std::uint64_t, 13> valid
                {
                    64,
                    64 << 3,  
                    64 << 7, 
                    64 << 9,  
                    64 << 11,
                    64 << 12,
                    64 << 13,
                    64 << 14,
                    64 << 15,
                    64 << 16,
                    64 << 17,
                    64 << 18,
                    64 << 19
                };
            for (auto v : valid)
                if (v >= requested)
                    return v;
            return (0x800000000000ull >> std::countl_zero(requested));
        }
    

        //=============================================================================
        template <std::uint64_t N, std::uint64_t N1 = 1024>
        requires (std::popcount(N) == 1)
        class tree
        {
        public:

            static auto constexpr max_capacity = N;
            static auto constexpr sub_tree_count = N1;
            static auto constexpr sub_tree_capacity = select_tree_size(max_capacity / sub_tree_count);

            static std::uint64_t constexpr capacity() noexcept;

            bool set
            (
                std::uint64_t
            ) noexcept;

            bool empty() const noexcept;

            std::uint64_t select() noexcept;

            std::uint64_t select
            (
                std::uint64_t
            ) noexcept;

        protected:

            using root_level_traits = level_traits<1, sub_tree_capacity>;
            using root_level = level<root_level_traits>;

            using root_level_array = std::array<root_level, sub_tree_count>;

            root_level_array subTrees_;

        }; // class tree

    } // namespace bcpp::implementation::signal_tree


    template <std::size_t N0, std::size_t N1 = 1024> 
    using signal_tree = implementation::signal_tree::tree<N0, N1>;

} // namespace bcpp


//=============================================================================
template <std::size_t N, std::size_t N1>
requires (std::popcount(N) == 1)
inline std::uint64_t constexpr bcpp::implementation::signal_tree::tree<N, N1>::capacity
(
) noexcept
{
    return sub_tree_capacity * sub_tree_count;
}


//=============================================================================
template <std::size_t N, std::size_t N1>
requires (std::popcount(N) == 1)
inline bool bcpp::implementation::signal_tree::tree<N, N1>::set
(
    // set the leaf node associated with the index to 1
    std::uint64_t signalIndex
) noexcept 
{
    auto treeIndex = signalIndex / sub_tree_capacity;
    return subTrees_[treeIndex].set(signalIndex % sub_tree_capacity);
}


//=============================================================================
template <std::size_t N, std::size_t N1>
requires (std::popcount(N) == 1)
inline bool bcpp::implementation::signal_tree::tree<N, N1>::empty
(
    // returns true if no leaf nodes are 'set' (root count is zero)
    // returns false otherwise
) const noexcept 
{
    for (auto const & tree : subTrees_)
        if (!tree.empty())
            return false;
    return true;
}


//=============================================================================
template <std::size_t N, std::size_t N1>
requires (std::popcount(N) == 1)
inline auto bcpp::implementation::signal_tree::tree<N, N1>::select 
(
    // select and return the index of a leaf which is 'set'
    // return ~0 if no leaf is 'set' (empty tree)
) noexcept -> std::uint64_t
{
    static thread_local std::uint64_t tls_inclinationFlags;
    auto result = select(tls_inclinationFlags++);
    tls_inclinationFlags = (result + 1);
    return result;
}


//=============================================================================
template <std::size_t N, std::size_t N1>
requires (std::popcount(N) == 1)
inline auto bcpp::implementation::signal_tree::tree<N, N1>::select 
(
    // select and return the index of a leaf which is 'set'
    // return ~0 if no leaf is 'set' (empty tree)
    std::uint64_t bias
) noexcept -> std::uint64_t
{
    bias %= capacity();
    auto treeIndex = bias / sub_tree_capacity;
    static auto constexpr bias_bits_consumed_by_tree_index = minimum_bit_count(sub_tree_capacity) - 1;
    static auto constexpr shift_to_remove_tree_index = (64 - bias_bits_consumed_by_tree_index);
    bias <<= shift_to_remove_tree_index;

    for (auto i = 0; i < sub_tree_count; ++i)
    {
        auto & tree = subTrees_[treeIndex];
        auto signalIndex = tree.select(bias);
        if (signalIndex != ~0)
            return (treeIndex * sub_tree_capacity) + signalIndex;
        treeIndex++;
        treeIndex %= sub_tree_count;
    }
    return ~0;
}
