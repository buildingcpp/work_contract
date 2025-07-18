# Work Contract

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/buildingcpp/work_contract/actions) [![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) [![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

Work Contract is a high-performance C++ library for managing repeatable, schedulable tasks (called "contracts") in concurrent applications. These contracts are lightweight, recurrent callables that can be scheduled, executed, rescheduled, or explicitly released, with release triggering cleanup via customizable callbacks for precise lifecycle control. Supporting both non-blocking (lock-free) and blocking synchronization modes, it leverages an efficient signal tree data structure for low-latency task selection, making it ideal for real-time, game, or high-throughput systems.

## Motivation

Traditional concurrency primitives like futures, promises, or thread pools often involve overhead from locking or polling. Work Contract addresses this by offering a contract-based model where tasks are represented as lightweight objects that can be scheduled, released, and rescheduled. It supports custom callbacks for execution, release, and exception handling, with built-in efficiency for large-scale task management (via signal trees). The library is designed for scenarios requiring fine-grained control over task lifecycles without heavy synchronization.

For detailed design rationale, examples, and benchmarks, see [DESIGN.md](DESIGN.md) and [EXAMPLES.md](EXAMPLES.md).

## Requirements

- C++20 compiler (e.g., GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.5+
- POSIX threads (pthread) and real-time extensions (rt) on Linux/Unix-like systems
- Dependencies are fetched automatically via CMake FetchContent:
  - buildingcpp/scripts.git (for dependency management)
  - buildingcpp/include.git (for utilities like bit operations and synchronization modes)
- For benchmarks/tests (optional): Additional fetches like fmtlib/fmt.git, google/googletest.git, etc.

No external installations required beyond CMake and a compatible compiler.

## Building

Clone the repository and use CMake to build:

```bash
git clone https://github.com/buildingcpp/work_contract.git
cd work_contract
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DWORK_CONTRACT_BUILD_BENCHMARK=ON  # Optional: Enable benchmarks
make -j$(nproc)
```

- Options:
  - `-CMAKE_BUILD_TYPE=Release` (default) or `Debug`.
  - `-DWORK_CONTRACT_BUILD_BENCHMARK=ON` (default ON): Builds benchmarks and tests.
- Outputs: Binaries in `build/bin`, libs in `build/lib`.

## Installation

After building, install the library and headers:

```bash
make install  # Or cmake --install .
```

- Installs to `/usr/local/` by default (use `-CMAKE_INSTALL_PREFIX=/path` to customize).
- Installed files:
  - Static library: `/usr/local/lib/libwork_contract.a`
  - Headers: Under `/usr/local/include/bcpp/` (e.g., `bcpp/work_contract.h`, `bcpp/signal_tree/tree.h`).
- To use in your project: Link against `work_contract` and include `<bcpp/work_contract.h>`.

Uninstall (manual): Remove installed files from prefix.

## Usage

Basic "Hello World" example (see [EXAMPLES.md](EXAMPLES.md) for more):

```cpp
#include <library/work_contract.h>
#include <iostream>
#include <jthread>

int main() 
{
	// create a work contract group
	bcpp::work_contract_group group;

	// Start worker thread to process scheduled work contracts
	std::jthread worker([&group](auto stopToken) 
			{
				while (not stopToken.stop_requested())
					group.execute_next_contract();
			});

	// create a work contract from the work contract group
	auto contract = group.create_contract([]() 
			{
				std::cout << "Hello, World! from Work Contract\n";
				bcpp::this_contract::release(); // release to schedule async destruction of this contract
			});

	// schedule the work contract
	contract.schedule();

	// Main thread waits for contract to be released
	while (contract.is_valid()) {}

	// Signal worker to stop
	worker.request_stop();
	worker.join();

	return 0;
}
```

Compile with: `g++ main.cpp -lwork_contract -lpthread -lrt`

## Presentations

- Presented at CppCon 2024: ["Work Contracts: Rethinking Task Based Concurrency and Parallelism for Low Latency C++"](https://cppcon.org/cppcon-2024-program/)
- Accepted for CppCon 2025: ["Work Contracts in Action: Advancing High-performance, Low-latency Concurrency in C++"](https://cppcon.org)

## License

MIT License. See [LICENSE](LICENSE) for details.