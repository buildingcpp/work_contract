#pragma once

#include "./work_contract_id.h"

#include <include/non_copyable.h>
#include <include/non_movable.h>


namespace bcpp::internal::work_contract 
{

    class this_contract;


    struct alignas(64) this_contract : 
        non_copyable,
        non_movable
    {

        this_contract
        (
            std::uint64_t id, 
            void * payload,
            void(* release)(std::uint64_t, void *),
            void(* schedule)(std::uint64_t, void *)
        ) noexcept :
            prev_(tlsThisContract_),
            id_(id),
            payload_(payload),
            release_(release),
            schedule_(schedule)
        {
            tlsThisContract_ = this;
        }

        ~this_contract() noexcept 
        {
            tlsThisContract_ = prev_;
        }

        static inline void schedule() noexcept {tlsThisContract_->schedule_(tlsThisContract_->id_, tlsThisContract_->payload_);}

        static inline void release() noexcept {tlsThisContract_->release_(tlsThisContract_->id_, tlsThisContract_->payload_);}

        static inline auto get_id() noexcept{return tlsThisContract_->id_;}

        static thread_local this_contract * tlsThisContract_;

        this_contract *                             prev_;
        std::uint64_t    id_;
        void *                                      payload_{nullptr};
        void(* release_)(std::uint64_t, void *);
        void(* schedule_)(std::uint64_t, void *);
    }; // struct this_contract

} // namespace bcpp::internal::work_contract