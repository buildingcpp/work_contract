add_executable(6_spmc main.cpp)

target_include_directories(6_spmc PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(6_spmc 
PRIVATE
    pthread
    rt
    work_contract
)