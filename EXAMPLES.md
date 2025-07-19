# Work Contract Examples

This document provides a series of examples that introduce Work Contract’s features incrementally, starting with a basic "Hello World" and building up to advanced concepts. Each example focuses on one new feature, demonstrating its usage in a concise, practical way. For design rationale, see [DESIGN.md](DESIGN.md). Note: The include path `<library/work_contract.h>` is used pending an installer update to align with the project’s structure.

## Table of Contents
- [1. Hello World: Basic Contract Execution](#1-hello-world-basic-contract-execution)
- [2. Repeatable Contract: Recurrent Scheduling](#2-repeatable-contract-recurrent-scheduling)
- [3. Release Callback: Custom Cleanup](#3-release-callback-custom-cleanup)
- [4. Blocking Mode: Efficient Waiting](#4-blocking-mode-efficient-waiting)
- [5. Exception Handling: Synchronous Error Handling](#5-exception-handling-synchronous-error-handling)
- [6. Data-Driven Contract: Processing Ingress Data](#6-data-driven-contract-processing-ingress-data)

## 1. Hello World: Basic Contract Execution
**Concept**: Creates, schedules, and executes a single contract in non-blocking mode.

**Complexity**: Low

**Details**: This example introduces the core Work Contract components: creating a `work_contract_group`, defining a contract with a simple callback, scheduling it with `schedule()`, and waiting for completion with `is_valid()`. The worker thread uses `std::jthread` and `execute_next_contract()` to process the contract, showcasing the basic lifecycle (Scheduled → Executing → Released).

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <thread>

int main() 
{
    // Create a work contract group
    bcpp::work_contract_group group;

    // Start worker thread to process scheduled work contracts
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    // Create a work contract from the work contract group
    auto contract = group.create_contract([]() 
            {
                std::cout << "Hello, World! from Work Contract\n";
                bcpp::this_contract::release(); // Release to async destroy this contract
            });

    // Schedule the contract
    contract.schedule();

    // Main thread waits for contract to be released
    while (contract.is_valid()) {}

    // Signal worker to stop
    worker.request_stop();
    worker.join();

    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`

---

## 2. Repeatable Contract: Recurrent Scheduling
**Concept**: Demonstrates repeatability by rescheduling the contract within its callback using `bcpp::this_contract::schedule()`.

**Complexity**: Low

**Details**: Building on Example 1, this example introduces repeatability by using `bcpp::this_contract::schedule()` to reschedule the contract for multiple executions. The contract tracks execution count with a lambda capture (`[count = 0]`) and releases after 3 executions, highlighting the recurrent nature of contracts.

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <thread>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract([count = 0]() mutable 
            {
                std::cout << "Execution #" << ++count << "\n";
                if (count < 3)
                    bcpp::this_contract::schedule(); // Reschedule for up to 3 executions
                else
                    bcpp::this_contract::release(); // Release after 3 executions
            });

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`

---

## 3. Release Callback: Custom Cleanup
**Concept**: Adds a release callback to perform cleanup when the contract is released.

**Complexity**: Medium

**Details**: This example extends Example 2 by adding a release callback to `create_contract`, executed when `release()` is called. The release callback performs cleanup (e.g., logging), demonstrating how Work Contract supports custom resource management during async destruction, a key feature for low-latency systems.

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <thread>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract(
        [count = 0]() mutable 
        {
            std::cout << "Execution #" << ++count << "\n";
            if (count < 3)
                bcpp::this_contract::schedule();
            else
                bcpp::this_contract::release();
        },
        []() { std::cout << "Contract cleanup completed\n"; } // Release callback
    );

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`

---

## 4. Blocking Mode: Efficient Waiting
**Concept**: Uses blocking mode for energy-efficient waiting when no contracts are scheduled.

**Complexity**: Medium

**Details**: Building on Example 3, this example demonstrates blocking mode’s energy-efficient waiting using `blocking_work_contract_group`. The main thread schedules the contract in a loop with 100ms sleeps, while the contract self-releases after 3 executions. The group uses condition variables internally to wait when idle, contrasting with the non-blocking mode of prior examples.

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() 
{
    bcpp::blocking_work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract(
        [count = 0]() mutable 
        {
            std::cout << "Execution #" << ++count << "\n";
            if (count >= 3)
                bcpp::this_contract::release(); // Self-release after 3 executions
        },
        []() { std::cout << "Contract cleanup completed\n"; }
    );

    while (contract.is_valid())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        contract.schedule();
    }

    worker.request_stop();
    worker.join();
    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`

---

## 5. Exception Handling: Synchronous Error Handling
**Concept**: Handles exceptions thrown in a contract’s callback via an exception callback, invoked synchronously.

**Complexity**: Medium-High

**Details**: This example introduces exception handling by adding an exception handler to `create_contract`. Unlike prior examples, the main function throws a `std::runtime_error`, which is caught and handled synchronously by the exception handler. The exception handler triggers release, showcasing Work Contract’s immediate error handling, which offers a streamlined alternative to the deferred error mechanisms often found in futures or coroutines.

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <thread>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    auto contract = group.create_contract(
        []() 
        {
            throw std::runtime_error("Error in contract");
        },
        []() { std::cout << "Contract cleanup completed\n"; },
        [](std::exception_ptr e) 
        {
            try { std::rethrow_exception(e); }
            catch (const std::exception& ex) 
            {
                std::cout << "Exception caught: " << ex.what() << "\n";
            }
            bcpp::this_contract::release();
        }
    );

    contract.schedule();
    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`

---

## 6. Data-Driven Contract: Processing Ingress Data
**Concept**: Demonstrates capturing and processing ingress data using a lock-free SPSC queue.

**Complexity**: High

**Details**: This example extends prior examples by using a `bcpp::spsc_fixed_queue` to handle ingress data. The main thread pushes integers (1–5) and a -1 termination flag, scheduling the contract each time. The contract pops and processes data, rescheduling if the queue is not empty, and releases on -1, demonstrating lock-free data processing for high-throughput scenarios.

```cpp
#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() 
{
    bcpp::work_contract_group group;
    std::jthread worker([&group](auto stopToken) 
            {
                while (not stopToken.stop_requested())
                    group.execute_next_contract();
            });

    // Create a lock-free SPSC queue for ingress data (capacity: 10 integers)
    bcpp::spsc_fixed_queue<int> dataQueue(10);

    auto contract = group.create_contract(
        [&dataQueue]() mutable 
        {
            int value;
            if (dataQueue.pop(value))
            {
                if (value == -1)
                    bcpp::this_contract::release(); // Release on termination flag
                else
                    std::cout << "Processed value: " << value << "\n";
            }
            if (not dataQueue.empty())
                bcpp::this_contract::schedule(); // Reschedule if more data
        },
        []() { std::cout << "Contract cleanup completed\n"; }
    );

    // Push data to queue and schedule contract
    for (int i = 1; i <= 5; ++i)
    {
        dataQueue.push(i);
        contract.schedule();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    dataQueue.push(-1); // Push termination flag
    contract.schedule();

    while (contract.is_valid()) {}
    worker.request_stop();
    worker.join();
    return 0;
}
```

**Compile**: `g++ main.cpp -lwork_contract -lpthread -lrt`