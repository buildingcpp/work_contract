add_executable(2_multi_invocation main.cpp)

target_include_directories(2_multi_invocation PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(2_multi_invocation 
PRIVATE
    work_contract
)