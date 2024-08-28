
add_library(work_contract
    ./work_contract_group.cpp
)


target_link_libraries(work_contract 
    pthread
)


target_include_directories(work_contract
PUBLIC
    ${_work_contract_dir}/src
    ${_include_dir}/src
)
