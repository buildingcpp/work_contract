add_executable(4_error_handling main.cpp)

target_include_directories(4_error_handling PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(4_error_handling 
PRIVATE
    pthread
    rt
    work_contract
)