add_executable(3_lock_free main.cpp)

target_include_directories(3_lock_free PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(3_lock_free 
PRIVATE
    work_contract
)