include(ExternalProject)

include(ProcessorCount)
ProcessorCount(N)

# Interface library to control the compilation of OpenSSL code with the right compatibility.
# This is used by custom code, even if it is not used by the external project.
add_library(monza_openssl_compatibility INTERFACE)
target_link_libraries(monza_openssl_compatibility INTERFACE monza_kernel_compatibility)

# CMake does not allow extracting the target flags, so we need to replicate the settings for monza_openssl_compatibility.
set(MONZA_OPENSSL_CFLAGS
  -std=gnu11
  --target=x86_64-istvan-monza
  -nostdinc
  -fomit-frame-pointer
  -ftls-model=initial-exec
  -mcx16
  -ffunction-sections -fdata-sections
  -isystem${COMPILER_HEADERS}
  -I${LIBC_HEADERS}
  -I${CMAKE_CURRENT_SOURCE_DIR}/include/private
  --include${CMAKE_CURRENT_SOURCE_DIR}/include/private/stubs.h
  -Werror
  -Wno-bitwise-op-parentheses
  -Wno-shift-op-parentheses
  -Wno-unused-but-set-variable
)
if (CMAKE_BUILD_TYPE EQUAL "Release")
  set(MONZA_OPENSSL_CFLAGS ${MONZA_OPENSSL_CFLAGS} -O3 -DNDEBUG)
elseif (CMAKE_BUILD_TYPE EQUAL "RelWithDebInfo")
  set(MONZA_OPENSSL_CFLAGS ${MONZA_OPENSSL_CFLAGS} -O2 -DNDEBUG -g)
else()
  set(MONZA_OPENSSL_CFLAGS ${MONZA_OPENSSL_CFLAGS} -O0 -g)
endif()

set(MONZA_OPENSSL_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../external/openssl)
set(MONZA_OPENSSL_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR}/openssl)
set(MONZA_OPENSSL_LIBCRYPTO_PATH ${MONZA_OPENSSL_BUILD_PATH}/libcrypto.a)
set(MONZA_OPENSSL_LIBSSL_PATH ${MONZA_OPENSSL_BUILD_PATH}/libssl.a)

# Import the OpenSSL Makefile target and configure it for Monza.
# The Monza configuration uses the UEFI defines and disables unused functionality.
# Depends on monza_openssl_compatibility to make sure that all headers are available already.
ExternalProject_Add(openssl
  SOURCE_DIR ${MONZA_OPENSSL_SOURCE_PATH}
  BINARY_DIR ${MONZA_OPENSSL_BUILD_PATH}
  CONFIGURE_COMMAND  
    ${MONZA_OPENSSL_SOURCE_PATH}/Configure
      "CC=${CMAKE_C_COMPILER}"
      "AR=${CMAKE_AR}"
      "RANLIB=${CMAKE_RANLIB}"
      no-pic no-shared
      no-ssl3 no-tls1 no-tls1_1 no-tls1_2 no-dtls
      no-afalgeng
      no-ui-console no-secure-memory no-randfile no-file-store no-directories
      no-stdio no-posix-io no-sock --with-rand-seed=getrandom
      ${MONZA_OPENSSL_CFLAGS}
      -DOPENSSL_SYS_LINUX -DOPENSSL_NO_MUTEX_ERRORCHECK -DNO_SYSLOG -DOSSL_IMPLEMENT_GETAUXVAL
  BUILD_COMMAND make build_generated && make depend && env bash -c "make libcrypto.a libssl.a -j \${CMAKE_BUILD_PARALLEL_LEVEL:-${N}}"
  INSTALL_COMMAND ""
  BUILD_ALWAYS TRUE
  DEPENDS monza_openssl_compatibility
)

# Interface library for OpenSSL header usage (in either apps or core components).
add_library(monza_openssl_headers INTERFACE)
add_dependencies(monza_openssl_headers openssl)
target_include_directories(monza_openssl_headers INTERFACE
  ${MONZA_OPENSSL_BUILD_PATH}/include
  ${MONZA_OPENSSL_SOURCE_PATH}/include
)

# monza_libcrypto CMake library wrapping the libcrypto archive from the openssl external project.
add_custom_command(
  OUTPUT ${MONZA_OPENSSL_LIBCRYPTO_PATH}
  DEPENDS openssl
)
add_custom_target(monza_libcrypto_a_target DEPENDS ${MONZA_OPENSSL_LIBCRYPTO_PATH})
add_library(monza_libcrypto STATIC IMPORTED)
add_dependencies(monza_libcrypto monza_libcrypto_a_target)
set_target_properties(monza_libcrypto PROPERTIES IMPORTED_LOCATION ${MONZA_OPENSSL_LIBCRYPTO_PATH})
target_link_libraries(monza_libcrypto INTERFACE
  monza_openssl_headers
)

# monza_libssl CMake library wrapping the libssl archive from the openssl external project.
add_custom_command(
  OUTPUT ${MONZA_OPENSSL_LIBSSL_PATH}
  DEPENDS openssl
)
add_custom_target(monza_libssl_a_target DEPENDS ${MONZA_OPENSSL_LIBSSL_PATH})
add_library(monza_libssl STATIC IMPORTED)
add_dependencies(monza_libssl monza_libssl_a_target)
set_target_properties(monza_libssl PROPERTIES IMPORTED_LOCATION ${MONZA_OPENSSL_LIBSSL_PATH})
target_link_libraries(monza_libcrypto INTERFACE
  monza_openssl_headers
)

# Interface library combining the OpenSSL components into one.
add_library(monza_openssl INTERFACE)
target_link_libraries(monza_openssl INTERFACE
    monza_libcrypto
    monza_libssl
)
