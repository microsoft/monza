cmake_minimum_required(VERSION 3.15)

include(CMakePrintHelpers)

add_subdirectory(../guest-verona-rt guest-verona-rt EXCLUDE_FROM_ALL)

set(CCF_REPO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/CCF)

add_library(monza-app-host INTERFACE)
target_compile_definitions(monza-app-host INTERFACE
  MONZA_HOST_SUPPORTS_QEMU
  CCF_LOGGER_NO_DEPRECATE
)
target_include_directories(monza-app-host INTERFACE
  include/common
  include/host
  ${CCF_REPO_DIR}/include
  ${CCF_REPO_DIR}/src
  ${CCF_REPO_DIR}/3rdparty/exported
)
target_link_libraries(monza-app-host INTERFACE
  rt
)

add_library(monza-app-guest INTERFACE)
target_compile_definitions(monza-app-guest INTERFACE
  CCF_LOGGER_NO_DEPRECATE
)
target_include_directories(monza-app-guest INTERFACE
  include/common
  include/guest
  ${CCF_REPO_DIR}/include
  ${CCF_REPO_DIR}/src
  ${CCF_REPO_DIR}/3rdparty/exported
)
target_link_libraries(monza-app-guest INTERFACE monza_app)
