add_executable(7_tasks main.cpp)

target_include_directories(7_tasks PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(7_tasks 
PRIVATE
    pthread
    rt
    work_contract
)