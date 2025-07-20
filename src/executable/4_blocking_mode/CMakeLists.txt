add_executable(4_blocking_mode main.cpp)

target_include_directories(4_blocking_mode PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(4_blocking_mode 
PRIVATE
    pthread
    rt
    work_contract
)