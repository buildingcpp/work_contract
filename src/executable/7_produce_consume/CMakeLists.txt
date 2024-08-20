add_executable(7_produce_consume main.cpp)

target_include_directories(7_produce_consume PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(7_produce_consume 
PRIVATE
    work_contract
)