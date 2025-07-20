add_executable(2_recurrent_scheduling main.cpp)

target_include_directories(2_recurrent_scheduling PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(2_recurrent_scheduling 
PRIVATE
    pthread
    rt
    work_contract
)