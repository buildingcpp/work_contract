# Work Contracts was presented at CppCon 2024
See here - https://youtu.be/oj-_vpZNMVw


# Work Contracts

Work Contracts are an alternative to traditional task based concurrency (tasks and task queues).  They are designed to be both easy to use and better suited for use in low latency applications.


## The fundamentals:

At its core, a `work_contract` is a representation of some logic (a callable) that can be scheduled for asynchronous invocation an arbitrary number of times.  They are created within the context of a parent `work_contract_group` and are scheduled for execution by invoking `work_contract::schedule()`.  Once scheduled, the actual invocation of the contract is handled by a thread which calls `work_contract_group::execute_next_contract()` on the `work_contract_group` which created that `work_contract`.

```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    int n = 0;
    bcpp::work_contract_group workContractGroup;
    auto workContract = workContractGroup.create_contract
            (
                [&](){std::cout << "n = " << n++ << "\n";}
            );

    while (n < 3)
    {
        workContract.schedule();
        workContractGroup.execute_next_contract();
    }
    return 0;
}
```

**Output:**
```
n = 0
n = 1
n = 2
```

</br>

## Initial schedule state:

By default, a `work_contract` is created in an unscheduled state, however, this initial state can be specified by providing an additional argument of type:

```
enum class work_contract::initial_state 
{
    unscheduled = 0,
    scheduled = 1
};
```

```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    int n = 0;
    bcpp::work_contract_group workContractGroup;
    auto workContract = workContractGroup.create_contract
            (
                [&](){std::cout << "n = " << n++ << "\n";},
                bcpp::work_contract::initial_state::scheduled
            );

    while (n < 3)
    {
        workContractGroup.execute_next_contract();
        workContract.schedule();
    }
    return 0;
}
```

</br>

## Optional work_contract_token:
The `work_contract` callable can take one of two forms:

The first form (as used in the above examples) takes no arguments:
```
auto workContract = workContractGroup.create_contract([](){});
```

The second form accepts a single argument of type `work_contract_token`:
```
auto workContract = workContractGroup.create_contract([](bcpp::work_contract_token &){});
```

The `work_contract_token` is a token which references the `work_contract` which is being invoked.  This token can be used to re-schedule that same `work_contract` during the current invocation:


```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    int n = 0;
    bcpp::work_contract_group workContractGroup;
    auto workContract = workContractGroup.create_contract
            (
                [&](auto & token)
                {
                    std::cout << "n = " << n++ << "\n";
                    if (n < 3)
                        token.schedule();
                },
                bcpp::work_contract::initial_state::scheduled
            );

    while (n < 3)
        workContractGroup.execute_next_contract();
    return 0;
}
```

[WIP] This document is a work in progress.  More will be added soon.