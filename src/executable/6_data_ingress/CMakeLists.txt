add_executable(6_data_ingress main.cpp)

target_include_directories(6_data_ingress PUBLIC ${_work_contract_dir}/src ${_include_dir}/src)

target_link_libraries(6_data_ingress 
PRIVATE
    pthread
    work_contract
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(6_data_ingress PRIVATE rt)
endif()
