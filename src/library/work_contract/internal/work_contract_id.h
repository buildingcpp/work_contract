#pragma once

#include <cstdint>
#include <concepts>


namespace bcpp::internal::work_contract
{

    class contract_index
    {
    public:

        using value_type = std::uint64_t;
        static value_type constexpr invalid_value = ~0;

        contract_index() noexcept = default;
        contract_index(value_type value) noexcept :value_(value){}
        contract_index(contract_index const &)  noexcept = default;
        contract_index & operator = (contract_index const &)  noexcept = default;
        contract_index(contract_index &&)  noexcept = default;
        contract_index & operator = (contract_index &&)  noexcept = default;

        operator value_type() const noexcept{return value_;}
        
    private:

        value_type value_{invalid_value};
    };

    class segment_index
    {
    public:

        using value_type = std::uint64_t;
        static value_type constexpr invalid_value = ~0;

        segment_index() noexcept = default;
        segment_index(value_type value) noexcept :value_(value){}
        segment_index(segment_index const &)  noexcept = default;
        segment_index & operator = (segment_index const &)  noexcept = default;
        segment_index(segment_index &&)  noexcept = default;
        segment_index & operator = (segment_index &&)  noexcept = default;

        operator value_type() const noexcept{return value_;}
        
    private:

        value_type value_{invalid_value};
    };


    template <std::size_t signal_tree_capacity>
    class work_contract_id
    {
    public:

        using value_type = std::uint64_t;
        using segment_index = internal::work_contract::segment_index;
        using contract_index = internal::work_contract::contract_index;
        static auto constexpr max = signal_tree_capacity;
        static value_type constexpr invalid_value = ~0;

        work_contract_id() noexcept = default;
        work_contract_id(value_type value) noexcept : value_(value){}
        work_contract_id(segment_index signalIndex, contract_index contractIndex) noexcept : work_contract_id((signalIndex * signal_tree_capacity) | contractIndex){}
        work_contract_id(work_contract_id const &) noexcept = default;
        work_contract_id & operator = (work_contract_id const &) noexcept = default;
        work_contract_id(work_contract_id &&) noexcept = default;
        work_contract_id & operator = (work_contract_id &&) noexcept = default;

        auto is_valid() const noexcept{return (value_ != invalid_value);}

        operator contract_index() const noexcept{return (value_ % signal_tree_capacity);}
        operator segment_index() const noexcept{return (value_ / signal_tree_capacity);}

        value_type get() const noexcept{return value_;}

    private:

        value_type value_{invalid_value};
    };

    template <typename T>
    concept work_contract_id_concept = std::is_same_v<T, work_contract_id<T::max>>;

} // namespace bcpp::internal::work_contract