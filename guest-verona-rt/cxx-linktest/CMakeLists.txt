add_executable(monza_cxx_linktest empty.cc)
target_link_libraries(monza_cxx_linktest PRIVATE
  monza_compatibility
  
  -Wl,-r
  -Wl,--whole-archive

  compiler-rt-builtins
  monza_libc
  cxxrt-static
  unwind_static
  monza_libcxx
)
add_custom_command(TARGET monza_cxx_linktest POST_BUILD
  COMMAND ${CMAKE_NM} $<TARGET_FILE:monza_cxx_linktest> | ${CMAKE_CURRENT_SOURCE_DIR}/validate_dependencies.py
)
set_target_properties(monza_cxx_linktest PROPERTIES LINK_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/validate_dependencies.py
)
