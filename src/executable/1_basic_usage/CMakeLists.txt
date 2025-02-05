add_executable(1_basic_usage main.cpp)

target_include_directories(1_basic_usage PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(1_basic_usage 
PRIVATE
    pthread
    rt
    work_contract
)