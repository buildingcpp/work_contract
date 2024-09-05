add_executable(benchmark main.cpp)

target_compile_options(benchmark PRIVATE ${TBB_CXX_STD_FLAG})

target_include_directories(benchmark
PRIVATE
    ${_concurrentqueue_src_path}
    ${_lockfree_src_path}
    ${_tbb_src_path}
    ${_lockfree_mpmc_queue_src_path}
    ${_googletest_src_path}
)

target_link_libraries(benchmark 
PRIVATE
    work_contract
    tbb_static
)

