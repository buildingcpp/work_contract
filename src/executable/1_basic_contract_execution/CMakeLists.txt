add_executable(1_basic_contract_execution main.cpp)

target_include_directories(1_basic_contract_execution PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(1_basic_contract_execution 
PRIVATE
    pthread
    rt
    work_contract
)