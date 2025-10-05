#pragma once

#include <cstdint>


namespace bcpp
{

    // TODO: type rich
    using signal_index = std::uint64_t;

    static signal_index constexpr invalid_signal_index{~0ull};
/*

    class signal_index
    {
    public:

        using value_type = std::uint64_t;
        static value_type constexpr invalid_value = ~0;

        signal_index() noexcept = default;
        signal_index(value_type value) noexcept :value_(value){}
        signal_index(signal_index const &)  noexcept = default;
        signal_index & operator = (signal_index const &)  noexcept = default;
        signal_index(signal_index &&)  noexcept = default;
        signal_index & operator = (signal_index &&)  noexcept = default;

        value_type get() const noexcept{return value_;}
        
    private:

        value_type value_{invalid_value};
    };
*/
} // namespace bcpp