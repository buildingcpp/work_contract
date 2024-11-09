
add_executable(blocking_work_contract_test
    ./main.cpp
)


target_link_libraries(blocking_work_contract_test 
    pthread
    work_contract
)


target_include_directories(blocking_work_contract_test
PUBLIC
    ${_work_contract_dir}/src
    ${_include_dir}/src
)
