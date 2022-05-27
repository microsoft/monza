add_library(monza_io INTERFACE)

target_include_directories(monza_io INTERFACE ../include/public)
target_include_directories(monza_io INTERFACE ../include/private)
target_include_directories(monza_io INTERFACE ../io/include)
target_include_directories(monza_io INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../external/ccf-ringbuffer)

add_subdirectory(cnet EXCLUDE_FROM_ALL)
target_link_libraries(monza_io INTERFACE monza_io_cnet)
target_link_libraries(monza_io_cnet PRIVATE monza_io)