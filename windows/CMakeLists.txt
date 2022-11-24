cmake_minimum_required(VERSION 3.15)

include(CMakePrintHelpers)

set(CMAKE_C_COMPILER clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)

project(
  monza_windows
  LANGUAGES CXX
)

set(APPS_INSTALL ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/apps)

add_subdirectory(apps/example)
