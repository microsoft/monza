cmake_minimum_required(VERSION 3.15)

include(CMakePrintHelpers)

set(CMAKE_C_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang)
set(CMAKE_CXX_COMPILER ${MONZA_LLVM_LOCATION}/bin/clang++)
set(CMAKE_LINKER ${MONZA_LLVM_LOCATION}/bin/ld.lld)
set(CMAKE_AR ${MONZA_LLVM_LOCATION}/bin/llvm-ar)
set(CMAKE_OBJCOPY ${MONZA_LLVM_LOCATION}/bin/llvm-objcopy)
set(CMAKE_NM ${MONZA_LLVM_LOCATION}/bin/llvm-nm)
set(CMAKE_RANLIB ${MONZA_LLVM_LOCATION}/bin/llvm-ranlib)

project(
  monza_example_app
  LANGUAGES CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(../../app-framework app-framework EXCLUDE_FROM_ALL)

unset(SRC)
set(SRC
  guest.cc
)
add_executable(apps-example-guest ${SRC})
target_link_libraries(apps-example-guest PRIVATE monza-app-guest)
add_qemu_compatible_image(qemu-apps-example-guest apps-example-guest)

unset(SRC)
set(SRC
  host.cc
)
add_executable(apps-example-host ${SRC})
target_link_libraries(apps-example-host PRIVATE monza-app-host)

add_custom_target(apps-example ALL)
add_dependencies(apps-example apps-example-guest qemu-apps-example-guest apps-example-host)

install(TARGETS apps-example-guest apps-example-host
  DESTINATION ${APPS_INSTALL}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qemu-apps-example-guest.img
  DESTINATION ${APPS_INSTALL}
)
