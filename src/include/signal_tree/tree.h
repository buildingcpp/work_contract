#pragma once

#include "./level.h"
#include "./signal_index.h"


namespace bcpp
{

    namespace implementation::signal_tree
    {

        //============================================================================= 
        template <std::uint64_t total_counters, std::uint64_t bits_per_counter>
        struct default_selector
        {
            inline auto operator()
            (
                // default select will select which ever child is non zero, or if both
                // children are non zero, prefer the child indicated by the bias flag.
                std::uint64_t biasFlags,
                std::uint64_t counters
            ) const noexcept -> signal_index
            {
                if constexpr (total_counters == 1)
                {
                    return 0;
                }
                else
                {
                    static auto constexpr counters_per_half = (total_counters / 2);
                    static auto constexpr bits_per_half = (counters_per_half * bits_per_counter);
                    static auto constexpr right_bit_mask = ((1ull << bits_per_half) - 1);
                    static auto constexpr left_bit_mask = (right_bit_mask << bits_per_half);
                    auto chooseRight = (((biasFlags & 0x8000000000000000ull) && (counters & right_bit_mask)) || ((counters & left_bit_mask) == 0ull));
                    counters >>= (chooseRight) ? 0 : bits_per_half;
                    return ((chooseRight) ? counters_per_half : 0) + default_selector<counters_per_half, bits_per_counter>()(biasFlags << 1, counters & right_bit_mask);
                }
            }
        };


        //=====================================================================
        // TODO: hack.  write better when have time
        template <std::uint64_t N = 64>
        static std::uint64_t consteval select_tree_size
        (
            std::uint64_t requested
        ) 
        {
            constexpr std::array<std::uint64_t, 14> valid
                {
                    64,
                    64 << 3,  
                    64 << 5,  
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
        template <std::uint64_t N>
        class tree
        {
        private:

            using root_level_traits = level_traits<1, N>;
            using root_level = level<root_level_traits>;
            
        public:

            static auto constexpr capacity = N;

            static_assert(select_tree_size(capacity) == capacity, "invalid signal_tree capacity");

            bool set
            (
                signal_index
            ) noexcept;

            bool empty() const noexcept;

            template <template <std::uint64_t, std::uint64_t> class = default_selector>
            signal_index select() noexcept;

            template <template <std::uint64_t, std::uint64_t> class = default_selector>
            signal_index select
            (
                std::uint64_t
            ) noexcept;

        private:

            root_level rootLevel_;

        }; // class tree

    } // namespace bcpp::implementation::signal_tree


    template <std::size_t N> 
    using signal_tree = implementation::signal_tree::tree<implementation::signal_tree::select_tree_size(N)>;

} // namespace bcpp


//=============================================================================
template <std::size_t N>
inline bool bcpp::implementation::signal_tree::tree<N>::set
(
    // set the leaf node associated with the index to 1
    signal_index signalIndex
) noexcept 
{
    return rootLevel_.set(signalIndex);
}


//=============================================================================
template <std::size_t N>
inline bool bcpp::implementation::signal_tree::tree<N>::empty
(
    // returns true if no leaf nodes are 'set' (root count is zero)
    // returns false otherwise
) const noexcept 
{
    return rootLevel_.empty();
}


//=============================================================================
template <std::size_t N>
template <template <std::uint64_t, std::uint64_t> class select_function>
inline auto bcpp::implementation::signal_tree::tree<N>::select 
(
    // select and return the index of a leaf which is 'set'
    // return invalid_signal_index if no leaf is 'set' (empty tree)
) noexcept -> signal_index
{
    static thread_local std::uint64_t tls_inclinationFlags;
    auto result = select<select_function>(tls_inclinationFlags++);
    tls_inclinationFlags = (result + 1);
    return result;
}


//=============================================================================
template <std::size_t N>
template <template <std::uint64_t, std::uint64_t> class select_function>
inline auto bcpp::implementation::signal_tree::tree<N>::select 
(
    // select and return the index of a leaf which is 'set'
    // return invalid_signal_index if no leaf is 'set' (empty tree)
    std::uint64_t bias
) noexcept -> signal_index
{
    return rootLevel_. template select<select_function>(bias);
}
