add_executable(hello_world main.cpp)

target_include_directories(hello_world PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(hello_world 
PRIVATE
    pthread
    rt
    work_contract
)