#include "./this_contract.h"


namespace bcpp::internal::work_contract
{

    thread_local this_contract * this_contract::tlsThisContract_ = nullptr;

} // namespace bcpp::internal::work_contract