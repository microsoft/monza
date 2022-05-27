cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0022 NEW)

include(CMakePrintHelpers)

set(CMAKE_C_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang)
set(CMAKE_CXX_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang++)
set(CMAKE_LINKER ${MONZA_LLVM_LOCATION}/bin/ld.lld)
set(CMAKE_AR ${MONZA_LLVM_LOCATION}/bin/llvm-ar)
set(CMAKE_OBJCOPY ${MONZA_LLVM_LOCATION}/bin/llvm-objcopy)
set(CMAKE_NM ${MONZA_LLVM_LOCATION}/bin/llvm-nm)
set(CMAKE_RANLIB ${MONZA_LLVM_LOCATION}/bin/llvm-ranlib)

set(ARCH "x86_64")

project(monza-cnet-benchmark C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

message(STATUS "Build Type for Monza CNet Host ${CMAKE_BUILD_TYPE}")
set(CMAKE_CONFIGURATION_TYPES  Release Debug RelWithDebInfo)
message(STATUS "Build types ${CMAKE_CONFIGURATION_TYPES}")

# snmalloc interface library
set(SNMALLOC_HEADER_ONLY_LIBRARY ON)
add_subdirectory(../../external/snmalloc ./external/snmalloc EXCLUDE_FROM_ALL)

unset(SRC)
aux_source_directory(. SRC)
add_executable(monza_cnet_benchmark ${SRC})
target_compile_options(monza_cnet_benchmark PRIVATE
  -Wall
)
target_link_libraries(monza_cnet_benchmark PRIVATE
  rt
  uv
)
# Link option, but in fact snmalloc is an INTERFACE library that configures compiler options
target_link_libraries(monza_cnet_benchmark PRIVATE snmalloc)
# snmalloc also brings in pthreads
