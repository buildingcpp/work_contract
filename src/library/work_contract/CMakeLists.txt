
add_library(work_contract
    ./work_contract_group.cpp
    ./work_contract_this.cpp
)


target_link_libraries(work_contract 
    pthread
)

# rt library is only available on Linux
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(work_contract rt)
endif()


target_include_directories(work_contract
PUBLIC
    ${_work_contract_dir}/src
    ${_include_dir}/src
)

# Install the static library (.a)
install(TARGETS work_contract
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install work_contract headers, preserving relative structure under include/bcpp/work_contract
install(DIRECTORY ${_work_contract_dir}/src/library
    DESTINATION include/bcpp/work_contract
    FILES_MATCHING PATTERN "*.h"
)
install(FILES ${_work_contract_dir}/src/library/work_contract.h
    DESTINATION include/bcpp/work_contract/library
)

# Install related signal_tree headers, preserving structure under include/bcpp/signal_tree
install(DIRECTORY ${_work_contract_dir}/src/include
    DESTINATION include/bcpp/work_contract
    FILES_MATCHING PATTERN "*.h"
)
install(FILES ${_work_contract_dir}/src/include/signal_tree.h
    DESTINATION include/bcpp/work_contract/include
)
