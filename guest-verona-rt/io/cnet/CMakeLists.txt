unset(SRC)
set(SRC
  ${SRC}
  net_cnet.cc
  netpoller.cc
  proto/udp_cnet.cc)
add_library(monza_io_cnet STATIC ${SRC})

target_include_directories(monza_io_cnet PRIVATE ../cnet/include)
target_include_directories(monza_io_cnet PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/ccf-ringbuffer)