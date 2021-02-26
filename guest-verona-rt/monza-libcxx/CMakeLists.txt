set(LLVM_LIBCXX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../external/llvm-project/libcxx)
set(LLVM_LIBCXX_SRC_DIR ${LLVM_LIBCXX_DIR}/src)
set(LLVM_LIBCXX_INCLUDE_DIR ${LLVM_LIBCXX_DIR}/include)
set(LLVM_LIBCXX_HEADERS ${MONZA_LLVM_LOCATION}/include/c++/v1)
set(LIBCXX_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/include)

macro(add_sources target_to_append root_folder)
  set(temp_list ${ARGN})
  list(TRANSFORM temp_list PREPEND ${root_folder}/)
  target_sources(${target_to_append} PRIVATE ${temp_list})
endmacro()

# Cannot import original CMakeLists.txt and override files
# Copy headers from LLVM installation and reconfigure instead
file(COPY ${LLVM_LIBCXX_HEADERS}/ DESTINATION ${LIBCXX_HEADERS})

# Generate Monza-specific configuration
set(_LIBCPP_ABI_VERSION "1")
set(_LIBCPP_ABI_NAMESPACE "")
set(_LIBCPP_ABI_FORCE_ITANIUM ON)
set(_LIBCPP_HAS_NO_GLOBAL_FILESYSTEM_NAMESPACE ON)
set(_LIBCPP_HAS_NO_FILESYSTEM ON)
set(_LIBCPP_HAS_NO_FILES ON)
set(_LIBCPP_HAS_NO_STDIN ON)
set(_LIBCPP_HAS_THREAD_API_EXTERNAL ON)
set(_LIBCPP_HAS_MUSL_LIBC ON)
set(_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS ON)
configure_file(${LLVM_LIBCXX_INCLUDE_DIR}/__config_site.in ${LIBCXX_HEADERS}/__config_site @ONLY)

add_library(monza_libcxx_headers INTERFACE)
target_include_directories(monza_libcxx_headers INTERFACE include)

add_library(monza_libcxx STATIC "")
add_subdirectory(src)
add_subdirectory(overridden_includes)

add_sources(monza_libcxx ${LLVM_LIBCXX_SRC_DIR}
  algorithm.cpp
  charconv.cpp
  chrono.cpp
  exception.cpp
  hash.cpp
  ios.cpp
  ios.instantiations.cpp
  iostream.cpp
  locale.cpp
  memory.cpp
  mutex.cpp
  mutex_destructor.cpp
  new.cpp
  optional.cpp
  regex.cpp
  stdexcept.cpp
  string.cpp
  strstream.cpp
  system_error.cpp
  variant.cpp
  vector.cpp
)

target_compile_definitions(monza_libcxx PRIVATE _LIBCPP_BUILDING_LIBRARY)
target_compile_definitions(monza_libcxx PRIVATE _LIBCPP_USE_CLOCK_GETTIME)
target_compile_definitions(monza_libcxx PRIVATE LIBCXXRT)

foreach(file ${OVERRIDDEN_LIBCXX_HEADERS})
  file(COPY overridden_includes/${file} DESTINATION ${LIBCXX_HEADERS})
endforeach(file)

add_custom_target(libcxx_override_headers)
foreach(file ${OVERRIDDEN_LIBCXX_HEADERS})
  string(REPLACE "/" "_" file_as_target ${file})
  add_custom_target(libcxx_override_headers_${file_as_target}
      DEPENDS ${LIBCXX_HEADERS}/${file}
    )
  add_custom_command(OUTPUT ${LIBCXX_HEADERS}/${file}
      COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/overridden_includes/${file} ${LIBCXX_HEADERS}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/overridden_includes/${file}
    )
  add_dependencies(libcxx_override_headers libcxx_override_headers_${file_as_target})
endforeach(file)
add_dependencies(monza_libcxx_headers libcxx_override_headers)

add_dependencies(monza_libcxx monza_libcxx_headers)
