# Work Contract Design Rationale

## Introduction

Work Contract is designed to provide a lightweight, high-performance alternative to traditional concurrency primitives in C++. It introduces "contracts" as repeatable, schedulable tasks that can be managed with minimal overhead, supporting both lock-free (non-blocking) and blocking synchronization modes. The library's core innovation is the signal tree data structure, which enables O(log N) selection of pending tasks, making it suitable for low-latency applications like real-time systems, games, or high-frequency trading.

The design draws inspiration from task-based concurrency models but addresses their limitations in latency and flexibility. By allowing contracts to be rescheduled or released explicitly, it offers precise control over task lifecycles, enabling powerful patterns like recurrent callbacks without the need for heavy thread pools or futures.

For implementation details and code examples, see [EXAMPLES.md](EXAMPLES.md). This document focuses on the design principles and rationale.

## Core Concepts

### Contracts
- **Definition**: A work contract is a lightweight handle to a task, represented as a callable (lambda or function) that can be executed multiple times.
- **Repeatability**: Contracts are recurrent by design—users can reschedule them via `bcpp::this_contract::schedule()` within the work callback, enabling event-driven patterns.
- **Lifecycle**: Contracts can be:
  - Scheduled: Sets the signal which corresponds to the contract, indicating that it is scheduled for execution.
  - Executing: Selected and run via `execute_next_contract()`, clearing the signal corresponding to the contract.
  - Released: Scheduled for asynchronous cleanup via `bcpp::this_contract::release()`, triggering the release callback and async destruction.
- **Rationale**: This model allows efficient reuse of tasks, eliminating allocation overhead associated with one-shot futures. The `this_contract` API provides thread-local, thread-safe control functions, eliminating explicit token passing.

### Groups
- **Definition**: A `work_contract_group` manages a pool of contracts, handling scheduling, selection, and execution.
- **Modes**: 
  - **Non-blocking**: Wait-free scheduling and lock-free selection, using atomics and signal trees for high throughput.
  - **Blocking**: Wait-free scheduling and lock-free selection when one or more contracts are scheduled, otherwise blocks (condition variable) until at least one contract is scheduled.
- **Execution**: `execute_next_contract()` selects and executes the next scheduled contract.
- **Rationale**: Groups centralize management, enabling safe multi-threaded execution. Dual modes cater to diverse use cases: lock-free for low-latency, blocking for energy-efficient waiting when no contracts are scheduled.

#### Synchronization Modes Details
The blocking mode is lock-free when contracts are scheduled, using atomic checks to avoid mutex locks during task selection and execution. Locks and condition variables are only engaged when the group is idle (no scheduled contracts), allowing for efficient waiting without polling.

In lock-free mode, scheduling is wait-free, relying on atomic operations without retries, while selecting is lock-free, ensuring progress under contention but potentially requiring retries in high-contention scenarios.

### Signal Tree
- **Definition**: A multi-level tree data structure for signaling and selecting contracts.
- **How It Works**:
  - Leaves represent individual contracts (set to 1 when scheduled).
  - Nodes aggregate counts for fast selection (O(log N) time).
  - Select returns a pair: signal index and a bool indicating if the tree is now empty.
- **Rationale**: Unlike traditional data structures with contention or polling overhead, the signal tree is lock-free in non-blocking mode, using atomics for updates. Its fixed-size design trades moderate memory usage for predictable latency, allowing it to vastly outperform dynamic alternatives like concurrent queues under load.

## Design Choices

### Lock-Free vs Blocking Modes
- **Non-Blocking**: Uses atomics and CAS for updates, ideal for high-contention scenarios where spinning is acceptable.
- **Blocking**: Employs condition variables and counters for efficient waiting, suitable for low-load or battery-constrained environments.
- **Rationale**: Dual modes let users tailor performance to workload. The bool from `signal_tree::select` supports blocking mode’s `notify_all` by tracking tree emptiness.

### Contract `release()` Functionality
- **Power**: Release schedules an asynchronous cleanup callback and invalidates the contract, with async destruction as a major feature for non-blocking resource management. It’s idempotent via atomic flags.
- **Rationale**: Explicit release empowers users to control task termination, enabling powerful patterns like self-terminating or error handling workflows. Async destruction ensures non-blocking cleanup, critical for low-latency systems. Combined with repeatability, it enables flexible workflows.

### Thread-Local `this_contract` API
- **Design**: Uses thread-local variables enabling `schedule()`/`release()` of the currently executing contract, ensuring thread-safe access without explicit token passing.
- **Rationale**: Simplifies callback signatures (`void()` for work) while providing control via `bcpp::this_contract::schedule()`. RAII guard ensures nesting safety with atomic operations.

### Performance Considerations
- **Signal Tree**: Achieves O(log N) selection with sub-counter arity for balanced levels, packing nodes into `std::atomic<std::uint64_t>` counters to minimize depth and atomic operations.
- **Atomic Operations**: Kept minimal in hot paths; bias flags reduce contention. The atomic `shared_ptr` for `releaseToken_` ensures thread-safe lifecycle management.
- **Benchmarks**: See [EXAMPLES.md](EXAMPLES.md) for comparisons with TBB/concurrentqueue, demonstrating superior task selection performance.
- **Rationale**: Optimized for low-latency, with benchmarks showing efficiency over standard concurrency primitives.

For examples and usage, see [EXAMPLES.md](EXAMPLES.md).