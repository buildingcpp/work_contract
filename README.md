# Work Contracts was presented at CppCon 2024
See here - https://youtu.be/oj-_vpZNMVw


# Work Contracts

Work Contracts are an alternative to traditional task based concurrency (tasks and task queues).  They are designed to be both easy to use and better suited for use in low latency applications.


## The fundamentals:

At its core, a `work_contract` is a representation of some logic (a callable) that can be scheduled for asynchronous invocation an arbitrary number of times.  A `work_contract` is created within the context of a parent `work_contract_group` and is scheduled for execution by invoking `work_contract::schedule()`.  Once scheduled, the actual invocation of the `work_contract`'s logic is performed by a worker thread after invoking `work_contract_group::execute_next_contract()` on the parent `work_contract_group` which was used to create that `work_contract`.

<img src="./img/wc_fundamentals_1.png">

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

## `work_contract_token`:
The `work_contract` callable can take one of two forms:

The first form (as used in the above examples) takes no arguments:
```
auto workContract = workContractGroup.create_contract([](){});
```

The second form accepts a single argument of type `work_contract_token`:
```
auto workContract = workContractGroup.create_contract([](bcpp::work_contract_token &){});
```

The `work_contract_token` is a token which references the `work_contract` which is being invoked.  This token can be used to re-schedule that same `work_contract` during the current invocation.  Below, the `work_contract` is created with an initial state of `scheduled`.  During invocation the contract is re-scheduled using the `work_contract_token` if `n < 3`.


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

</br>

## releasing a work contract (async "destruction"):

Work contracts are intended to be long lived.  It is often useful to perform some function when the contract finally expires.  When creating a work contract, a second callable can be provided which will be invoked once at the time when the work contract expires.  This callable will happen exactly once and it is guaranteed that no further invocations of the primary work callable shall take place once this release callable has been scheduled.  As with the primary work callable, the release callable is asynchronous, and is invoked via a call to `work_contract_group::execute_next_contract()`.

```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    bcpp::work_contract_group workContractGroup;
    auto workContract = workContractGroup.create_contract
            (
                [](){std::cout << "work contract executed\n";},    // this is the primary contract callable
                [](){std::cout << "work contract has expired\n";}, // this is the 'release' callable
                bcpp::work_contract::initial_state::scheduled
            );

    // exeucte the contract
    workContractGroup.execute_next_contract();
    
    // schedule the contract for release
    workContract.release();

    // execute the release callable
    workContractGroup.execute_next_contract();

    return 0;
}
```
**Output:**
```
work contract executed
work contract has expired
```

</br>
If a work contract goes out of scope then it is automatically scheduled for release:

```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    bcpp::work_contract_group workContractGroup;
    {
        auto workContract = workContractGroup.create_contract
                (
                    [](){},                                             // this is the primary contract callable
                    [](){std::cout << "work contract has expired\n";}  // this is the 'release' callable
                );
    } // leaving scope will schedule the work contract for release ...
    
    // execute the release callable
    workContractGroup.execute_next_contract();

    return 0;
}
```
**Output:**
```
work contract has expired
```
</br>
A contract can also be released via the `work_contract_token` during the primary work contract callback.
The function `work_contract::is_valid()` can be used to determine if the contract has expired or not.

```
#include <library/work_contract.h>
#include <iostream>

int main(int, char **)
{
    bcpp::work_contract_group workContractGroup;
    auto workContract = workContractGroup.create_contract
            (
                [](auto & token){token.release();},                 // this is the primary contract callable
                [](){std::cout << "work contract has expired\n";}, // this is the 'release' callable
                bcpp::work_contract::initial_state::scheduled
            );
    
    while (workContract.is_valid())
        workContractGroup.execute_next_contract();

    return 0;
}
```
**Output:**
```
work contract has expired
```


[WIP] This document is a work in progress.  More will be added soon.