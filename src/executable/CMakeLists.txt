if (WORK_CONTRACT_BUILD_BENCHMARK)
    add_subdirectory(benchmark)
   # add_subdirectory(signal_tree_benchmark)

  #  add_subdirectory(7_produce_consume)
endif(WORK_CONTRACT_BUILD_BENCHMARK)    

add_subdirectory(1_basic_usage)
    add_subdirectory(2_multi_invocation)
    add_subdirectory(3_lock_free)
    add_subdirectory(4_error_handling)
    add_subdirectory(5_spsc)
    add_subdirectory(6_spmc)
    add_subdirectory(8_data)