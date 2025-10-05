add_executable(4_blocking_mode main.cpp)

target_include_directories(4_blocking_mode PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(4_blocking_mode 
PRIVATE
    pthread
    work_contract
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(4_blocking_mode PRIVATE rt)
endif()
