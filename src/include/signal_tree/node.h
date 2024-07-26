#pragma once

#include "./helper.h"

#include <bit>
#include <cstdint>
#include <atomic>
#include <array>


namespace bcpp::implementation::signal_tree
{

    template <std::size_t N>
    requires (is_power_of_two(N))
    struct node_traits
    {
        static auto constexpr capacity = N;
        static auto constexpr number_of_counters = sub_counter_arity_v<capacity>;
        static auto constexpr counter_capacity = capacity / number_of_counters;
        static auto constexpr bits_per_counter = minimum_bit_count(counter_capacity);
    };


    template <typename T> concept node_traits_concept = std::is_same_v<T, node_traits<T::capacity>>;
    template <typename T> concept leaf_node_traits = ((node_traits_concept<T>) && (T::capacity == 64));
    template <typename T> concept non_leaf_node_traits = ((node_traits_concept<T>) && (!leaf_node_traits<T>));


    //=============================================================================
    // non leaf nodes ...
    // node is a 64 bit integer which represents two (or more) counters
    template <node_traits_concept T>
    class alignas(64) node
    {
    public:

        static auto constexpr capacity = T::capacity;
        static auto constexpr number_of_counters = T::number_of_counters;
        static auto constexpr counter_capacity = T::counter_capacity;
        static auto constexpr bits_per_counter = T::bits_per_counter;
        static auto constexpr counter_mask = (1ull << bits_per_counter) - 1;

        using child_type = node<node_traits<capacity / number_of_counters>>;
        using item_index = std::uint64_t;
        using counter_index = std::uint64_t;
        using bias_flags = std::uint64_t;
        using value_type = std::uint64_t;

        bool set
        (
            item_index
        ) noexcept;

        item_index select
        (
            bias_flags
        ) noexcept;

        bool empty() const noexcept;

    protected:

        template <std::uint64_t>
        item_index select
        (
            bias_flags,
            value_type
        ) noexcept;

        std::atomic<value_type> value_{0};

        static std::array<std::uint64_t, number_of_counters> constexpr addend
                {
                    []<std::size_t ... N>(std::index_sequence<N ...>) -> std::array<std::uint64_t, number_of_counters>
                    {
                        return {(1ull << ((number_of_counters - N - 1) * bits_per_counter)) ...};
                    }(std::make_index_sequence<number_of_counters>())
                };

        std::size_t get_counter_arity() const noexcept
        {
            return number_of_counters;
        }

    };

} // namespace bcpp::implementation::signal_tree


//=============================================================================
template <bcpp::implementation::signal_tree::node_traits_concept T>
inline bool bcpp::implementation::signal_tree::node<T>::set
(
    item_index signalIndex
) noexcept
{
    if constexpr (non_leaf_node_traits<T>)
    {
        value_ += addend[(signalIndex % capacity) / counter_capacity];
        return true;
    }
    else
    {
        auto bit = 0x8000000000000000ull >> signalIndex;
        return ((value_.fetch_or(bit) & bit) == 0); 
    }
}


//=============================================================================
template <bcpp::implementation::signal_tree::node_traits_concept T>
inline auto bcpp::implementation::signal_tree::node<T>::select
(
    bias_flags biasFlags
) noexcept -> item_index
{
    if constexpr (non_leaf_node_traits<T>)
    {
        auto expected = value_.load();
        while (expected)
        {
            auto counterIndex = select<number_of_counters>(biasFlags, expected);
            if (value_.compare_exchange_strong(expected, expected - addend[counterIndex]))
                return counterIndex;
        }
        return invalid_signal_index;  
    }
    else
    {
        auto expected = value_.load();
        while (expected)
        {
            auto counterIndex = select<64>(biasFlags, expected);
            auto bit = 0x8000000000000000ull >> counterIndex;
            if (expected = value_.fetch_and(~bit); ((expected & bit) == bit))
                return counterIndex;
        }
        return invalid_signal_index;
    }   
}

       
//============================================================================= 
template <bcpp::implementation::signal_tree::node_traits_concept T>
template <std::uint64_t total_counters>
inline auto bcpp::implementation::signal_tree::node<T>::select
(
    bias_flags biasFlags,
    value_type counters
) noexcept -> item_index
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
        auto chooseRight = (((biasFlags & 0x8000000000000000ull) && (counters & right_bit_mask)) || ((counters & left_bit_mask) == 0));
        counters >>= (chooseRight) ? 0 : bits_per_half;
        return ((chooseRight) ? counters_per_half : 0) + select<counters_per_half>(biasFlags << 1, counters & right_bit_mask);
    }
}


//============================================================================= 
template <bcpp::implementation::signal_tree::node_traits_concept T>
inline bool bcpp::implementation::signal_tree::node<T>::empty
(
) const noexcept
{
    return (value_.load() == 0);
}
