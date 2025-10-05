#pragma once

#include <cstdint>


namespace bcpp::internal::work_contract
{

        using state_flags = std::uint64_t;
        static state_flags constexpr release_flag      = 0x00000004ull;
        static state_flags constexpr execute_flag      = 0x00000002ull;
        static state_flags constexpr schedule_flag     = 0x00000001ull;

} // namespace bcpp::internal::work_contract