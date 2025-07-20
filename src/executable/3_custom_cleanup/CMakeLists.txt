add_executable(3_custom_cleanup main.cpp)

target_include_directories(3_custom_cleanup PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(3_custom_cleanup 
PRIVATE
    pthread
    rt
    work_contract
)