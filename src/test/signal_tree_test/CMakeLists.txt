
add_executable(signal_tree_test
    ./main.cpp
)


target_link_libraries(signal_tree_test 
    pthread
    work_contract
)


target_include_directories(signal_tree_test
PUBLIC
    ${_work_contract_dir}/src
    ${_include_dir}/src
)
