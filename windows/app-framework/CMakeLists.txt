cmake_minimum_required(VERSION 3.15)

include(CMakePrintHelpers)

set(CCF_REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../external/CCF)

unset(SRC)
set(SRC
  ${SRC}
  host/hcs_enclave.cc)
add_library(monza-app-host STATIC ${SRC})
target_compile_definitions(monza-app-host PRIVATE
  _AMD64_
)
target_include_directories(monza-app-host PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app-framework/include/host
)
target_compile_definitions(monza-app-host INTERFACE
  MONZA_HOST_SUPPORTS_HCS
  CCF_LOGGER_NO_DEPRECATE
)
target_include_directories(monza-app-host INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app-framework/include/common
  ${CMAKE_CURRENT_SOURCE_DIR}/../../app-framework/include/host
  ${CCF_REPO_DIR}/include
  ${CCF_REPO_DIR}/src
  ${CCF_REPO_DIR}/3rdparty/exported
)
target_compile_options(monza-app-host INTERFACE
  -include ${CMAKE_CURRENT_SOURCE_DIR}/../include/nix2win.h
)
target_link_libraries(monza-app-host INTERFACE
  computecore rpcrt4 ntdll
)
