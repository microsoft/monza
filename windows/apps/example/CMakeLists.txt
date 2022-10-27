cmake_minimum_required(VERSION 3.15)

include(CMakePrintHelpers)

project(
  monza_example_app
  LANGUAGES CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../../app-framework app-framework EXCLUDE_FROM_ALL)

unset(SRC)
set(SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../apps/example/host.cc
)
add_executable(apps-example-host ${SRC})
target_link_libraries(apps-example-host PRIVATE monza-app-host)

add_custom_target(apps-example ALL)
add_dependencies(apps-example apps-example-host)

install(TARGETS apps-example-host
  DESTINATION ${APPS_INSTALL}
)
