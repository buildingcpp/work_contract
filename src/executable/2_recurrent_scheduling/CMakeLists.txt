add_executable(2_recurrent_scheduling main.cpp)

target_include_directories(2_recurrent_scheduling PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(2_recurrent_scheduling 
PRIVATE
    pthread
    work_contract
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(2_recurrent_scheduling PRIVATE rt)
endif()
