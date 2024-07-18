add_executable(benchmark main.cpp enqueue_test.cpp)

fetch_dependency("https://github.com/cameron314/concurrentqueue.git;master")
fetch_dependency("https://github.com/wjakob/tbb.git;master")
fetch_dependency("https://github.com/boostorg/lockfree.git;master")
fetch_dependency("https://github.com/erez-strauss/lockfree_mpmc_queue.git;master")


target_include_directories(benchmark
PRIVATE
    ${_concurrentqueue_src_path}
    ${_lockfree_src_path}
    ${_tbb_src_path}
    ${_lockfree_mpmc_queue_src_path}
)

target_link_libraries(benchmark 
PRIVATE
    work_contract
)

target_compile_options(benchmark PRIVATE ${TBB_CXX_STD_FLAG})