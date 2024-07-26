#pragma once

#include "./mpsc_queue.h"
#include <vector>
#include <atomic>
#include <cstdint>
#include <type_traits>
#include <thread>


namespace bcpp
{

    template <typename T, std::size_t N>
    class alignas(64) mpmc_queue
    {
    public:

        static auto constexpr capacity = (1 << (64 - std::countl_zero(N - 1)));

        mpmc_queue();

        template <typename T_>
        bool push
        (
            T_ &&
        );

        bool pop
        (
            T &
        );

        bool empty() const noexcept;

    private:

        static auto constexpr sub_queues = 1024;

        using subqueue_type = mpsc_queue<T, capacity>;

        std::array<std::unique_ptr<subqueue_type>, sub_queues> queues_;

        work_contract_tree<bcpp::synchronization_mode::non_blocking> workContractTree_;

        std::array<work_contract<bcpp::synchronization_mode::non_blocking>, sub_queues> workContract_;

        static thread_local std::int32_t    tlsValuePopped_;
        static thread_local bool            tlsPopResult_;
    };


    template <typename T, std::size_t N> thread_local std::int32_t  mpmc_queue<T, N>::tlsValuePopped_;
    template <typename T, std::size_t N> thread_local bool mpmc_queue<T, N>::tlsPopResult_;

} // namespace bcpp




//=============================================================================
template <typename T, std::size_t N>
inline bcpp::mpmc_queue<T, N>::mpmc_queue
(
):
    workContractTree_(sub_queues)
{
    for (auto & queue : queues_)
        queue = std::make_unique<subqueue_type>();

    auto queueIndex = 0;
    for (auto & workContract :  workContract_)
    {
        workContract = workContractTree_.create_contract([this, queue = queues_[queueIndex].get()]
                (
                    auto & token
                )
                {
                    auto [success, more] = queue->pop(tlsValuePopped_);
                    tlsPopResult_ = success;
                    if ((!success) || (more))
                        token.schedule();
                });
        if (!workContract.is_valid())
            std::cout << "Failed to create work contract\n";
        ++queueIndex;
    }
}


//=============================================================================
template <typename T, std::size_t N>
template <typename T_>
inline bool bcpp::mpmc_queue<T, N>::push
(
    T_ && data
)
{
    static thread_local auto queue_index = []()
            {
                auto threadId = std::this_thread::get_id();
                std::uint64_t x = *reinterpret_cast<std::uint64_t const *>(&threadId);
                x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
                x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
                x = x ^ (x >> 31);
                x %= sub_queues;
                return x;
            }();

    if (auto success = queues_[queue_index]->push(std::forward<T>(data)); success)
    {
        workContract_[queue_index].schedule();
        return true;
    }
    return false;
}


//=============================================================================
template <typename T, std::size_t N>
inline bool bcpp::mpmc_queue<T, N>::pop
(
    T & data
)
{
    tlsPopResult_ = false;
    workContractTree_.execute_next_contract();
    data = tlsValuePopped_;
    return tlsPopResult_;
}


//=============================================================================
template <typename T, std::size_t N>
inline bool bcpp::mpmc_queue<T, N>::empty
(
) const noexcept
{
    for (auto const & queue : queues_)
        if (!queue->empty())
            return false;
    return true;
}
