cmake_minimum_required(VERSION 3.15)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
project(monza C CXX ASM_NASM)

set(CMAKE_CXX_STANDARD 17)

message(STATUS "Build Type for Monza ${CMAKE_BUILD_TYPE}")
set(CMAKE_CONFIGURATION_TYPES  Release Debug RelWithDebInfo)
message(STATUS "Build types ${CMAKE_CONFIGURATION_TYPES}")

include(CMakePrintHelpers)

# Clang-format target
include(cmake/clangformat.cmake)
clangformat_targets()

## Check copyright/license target
include(cmake/copyright.cmake)
copyright_targets()

##########################################################
#  Configure external installations (LLVM)
##########################################################
# ExternalProject is used to trick the CMake into building LLVM before Monza.
# We use two External Projects, so that the LLVM build can complete and install
# before we start the Monza one.  This is required as we consume CMake
# artifacts, which are not available otherwise.
include(ExternalProject)

set (LLVM_INSTALL ${CMAKE_BINARY_DIR}/Release/llvm)
#  Build or Download LLVM installation
set (EXTERNAL_EXTRA_CMAKE_ARGS)
list (APPEND EXTERNAL_EXTRA_CMAKE_ARGS
  -DMONZA_DOWNLOAD_LLVM=${MONZA_DOWNLOAD_LLVM}
  -DLLVM_INSTALL=${LLVM_INSTALL}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)

if (NOT MONZA_DOWNLOAD_LLVM)
  list (APPEND EXTERNAL_EXTRA_CMAKE_ARGS
    -DLLVM_EXTRA_CMAKE_ARGS=${LLVM_EXTRA_CMAKE_ARGS}
  )
else ()
  list (APPEND EXTERNAL_EXTRA_CMAKE_ARGS
    -DVERBOSE_LLVM_DOWNLOAD=${VERBOSE_LLVM_DOWNLOAD}
  )
endif ()

ExternalProject_Add(external
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external
  CMAKE_ARGS ${EXTERNAL_EXTRA_CMAKE_ARGS} 
  BUILD_ALWAYS true
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  STEP_TARGETS manual-update
  USES_TERMINAL_BUILD true
  USES_TERMINAL_CONFIGURE true
  EXCLUDE_FROM_ALL true
)

# Point Monza at the LLVM install, we just built/downloaded
list (APPEND MONZA_GUEST_EXTRA_CMAKE_ARGS
  -DMONZA_LLVM_LOCATION=${LLVM_INSTALL}/install
)

##########################################################
#  Build Monza guest subproject
##########################################################
set (GUEST_TEST_INSTALL ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/guests)
list (APPEND MONZA_GUEST_EXTRA_CMAKE_ARGS
  -DGUEST_TEST_INSTALL=${GUEST_TEST_INSTALL}
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DMONZA_SYSTEMATIC_BUILD=${MONZA_SYSTEMATIC_BUILD}
)

ExternalProject_Add(guest-verona-rt
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/guest-verona-rt
  DEPENDS external
  CMAKE_ARGS ${MONZA_GUEST_EXTRA_CMAKE_ARGS}
  BUILD_ALWAYS true
  TEST_AFTER_INSTALL true
  TEST_EXCLUDE_FROM_MAIN true
  STEP_TARGETS test
  USES_TERMINAL_BUILD true
  USES_TERMINAL_CONFIGURE true
  USES_TERMINAL_INSTALL true
  USES_TERMINAL_TEST true
)

######################################################
#  Add testing at top level
######################################################
enable_testing()

include(ProcessorCount)
ProcessorCount(N)

# Adds a target check that runs the tests.
add_custom_target(check DEPENDS guest-verona-rt-test)
