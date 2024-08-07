#pragma once

#include "./helper.h"
#include "./signal_index.h"

#include <bit>
#include <cstdint>
#include <atomic>
#include <array>
#include <functional>


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
        using bias_flags = std::uint64_t;
        using value_type = std::uint64_t;

        class counter_index
        {
        public:
            using value_type = std::uint64_t;
            counter_index(signal_index signalIndex):value_(signalIndex / counter_capacity){}
            value_type value() const{return value_;}
        private:
            value_type const value_;
        };

        bool set
        (
            counter_index
        ) noexcept;

        bool empty() const noexcept;

        template <template <std::uint64_t, std::uint64_t> class>
        signal_index select
        (
            bias_flags
        ) noexcept;

    protected:

        std::atomic<value_type> value_{0};

        static std::array<std::uint64_t, number_of_counters> constexpr addend
                {
                    []<std::size_t ... N>(std::index_sequence<N ...>) -> std::array<std::uint64_t, number_of_counters>
                    {
                        return {(1ull << ((number_of_counters - N - 1) * bits_per_counter)) ...};
                    }(std::make_index_sequence<number_of_counters>())
                };
    };

} // namespace bcpp::implementation::signal_tree


//=============================================================================
template <bcpp::implementation::signal_tree::node_traits_concept T>
inline bool bcpp::implementation::signal_tree::node<T>::set
(
    counter_index counterIndex
) noexcept
{
    if constexpr (non_leaf_node_traits<T>)
    {
        // non-leaf node.  increment correct sub counter
        value_ += addend[counterIndex.value()];
        return true;
    }
    else
    {
        // leaf node. counters are 1 bit in size
        // set correct counter bit and return true if not already set
        auto bit = 0x8000000000000000ull >> counterIndex.value();
        return ((value_.fetch_or(bit) & bit) == 0); 
    }
}


//=============================================================================
template <bcpp::implementation::signal_tree::node_traits_concept T>
template <template <std::uint64_t, std::uint64_t> class selector>
inline auto bcpp::implementation::signal_tree::node<T>::select
(
    bias_flags biasFlags
) noexcept -> signal_index
{
    auto expected = value_.load();
    while (expected)
    {
        auto counterIndex = selector<number_of_counters, bits_per_counter>()(biasFlags, expected);
        if constexpr (non_leaf_node_traits<T>)
        {            
            if (value_.compare_exchange_strong(expected, expected - addend[counterIndex]))
                return counterIndex;
        }
        else
        {
            auto bit = 0x8000000000000000ull >> counterIndex;
            if (expected = value_.fetch_and(~bit); ((expected & bit) == bit))
                return counterIndex;
        }
        int y = 9;
    }
    return invalid_signal_index; 
}
