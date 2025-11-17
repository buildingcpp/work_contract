add_executable(benchmark main.cpp)

target_compile_options(benchmark PRIVATE ${TBB_CXX_STD_FLAG})

target_include_directories(benchmark
PRIVATE
    ${_concurrentqueue_src_path}
    ${_lockfree_src_path}
    ${_tbb_src_path}/include
    ${_lockfree_mpmc_queue_src_path}
    ${_googletest_src_path}
    ${_fmt_src_path}/include
)

target_link_libraries(benchmark 
PRIVATE
    pthread
    work_contract
    tbb_static
    fmt
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(benchmark PRIVATE rt)
endif()

