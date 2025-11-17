add_executable(3_custom_cleanup main.cpp)

target_include_directories(3_custom_cleanup PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(3_custom_cleanup 
PRIVATE
    pthread
    work_contract
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(3_custom_cleanup PRIVATE rt)
endif()
