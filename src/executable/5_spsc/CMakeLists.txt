add_executable(5_spsc main.cpp)

target_include_directories(5_spsc PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(5_spsc 
PRIVATE
    pthread
    rt
    work_contract
)