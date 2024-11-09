add_executable(8_data main.cpp)

target_include_directories(8_data PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(8_data 
PRIVATE
    work_contract
)