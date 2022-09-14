unset(SRC)
aux_source_directory(. SRC)
set(SRC
  ${SRC}
  main64.asm
  cores.asm
  compartment.asm
  exceptions.asm)
set_property(SOURCE cores.asm APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inc/macros.asm)
set_property(SOURCE compartment.asm APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inc/macros.asm)

add_library(monza_rt_arch STATIC ${SRC})
target_include_directories(monza_rt_arch PRIVATE inc)
target_compile_options(monza_rt_arch PRIVATE $<$<COMPILE_LANGUAGE:ASM_NASM>:-I${CMAKE_CURRENT_SOURCE_DIR}/inc/>)
# Many weird casts need to occur between C++/ASM/hypervisor.
target_compile_options(monza_rt_arch PRIVATE $<$<COMPILE_LANGUAGE:C,CXX>:-fno-strict-aliasing>)
target_link_libraries(monza_rt_arch PRIVATE monza_kernel_compatibility)
