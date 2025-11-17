add_executable(5_exception_handling main.cpp)

target_include_directories(5_exception_handling PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(5_exception_handling 
PRIVATE
    pthread
    work_contract
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(5_exception_handling PRIVATE rt)
endif()
