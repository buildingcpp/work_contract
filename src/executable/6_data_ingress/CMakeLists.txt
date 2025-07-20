add_executable(6_data_ingress main.cpp)

target_include_directories(6_data_ingress PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(6_data_ingress 
PRIVATE
    pthread
    rt
    work_contract
)