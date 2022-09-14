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

#TODO: set the nasm elf format based on arch
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
project(monza C CXX ASM ASM_NASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

message(STATUS "Build Type for Monza ${CMAKE_BUILD_TYPE}")
set(CMAKE_CONFIGURATION_TYPES  Release Debug RelWithDebInfo)
message(STATUS "Build types ${CMAKE_CONFIGURATION_TYPES}")

# Common settings for all llvm-based components
set(LLVM_CONFIG_PATH ${MONZA_LLVM_LOCATION}/bin/llvm-config)
set(TARGET_TRIPLE ${ARCH}-istvan-monza)
set(HAVE_UNWIND_H OFF)

# Folders for basic components and headers in case another component needs them directly
set(COMPILER_HEADERS ${MONZA_LLVM_LOCATION}/lib/clang/13.0.1/include)
set(LIBC ${CMAKE_CURRENT_BINARY_DIR}/monza-libc)
set(LIBC_HEADERS ${LIBC}/include)
set(LIBCXX ${CMAKE_CURRENT_BINARY_DIR}/monza-libcxx)
set(LIBCXX_HEADERS ${LIBCXX}/include)

add_custom_target(compiler_updated
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/compiler_updated.h
  )
add_custom_command(OUTPUT compiler_updated.h
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/compiler_updated.h
    DEPENDS ${MONZA_LLVM_LOCATION}/bin/clang ${MONZA_LLVM_LOCATION}/bin/clang++
  )

# Interface libraries used to set up requirements for Monza compatibility
add_library(monza_compatibility INTERFACE)

# Compiler options
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-Werror>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-mcx16>)
# Limit architecture to core2 for now since QEMU TCG does not support AVX
#target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-march=native>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-march=core2>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-mrdseed>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-fomit-frame-pointer>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-nostdinc>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-ffunction-sections -fdata-sections>)
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-target ${ARCH}-istvan-monza>)
target_compile_definitions(monza_compatibility INTERFACE _ALL_SOURCE)
target_compile_definitions(monza_compatibility INTERFACE LIBC_THREADED_GLOBALS)
target_include_directories(monza_compatibility INTERFACE ../external/verona/src/rt)
target_include_directories(monza_compatibility INTERFACE include/public)
# COMPILER_HEADERS as compile options and not include_directories as CMake filters it out otherwise
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-isystem ${COMPILER_HEADERS}>)
# Ordering here is very important: libc++ before libc
target_include_directories(monza_compatibility SYSTEM INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${LIBCXX_HEADERS}>)
target_include_directories(monza_compatibility SYSTEM INTERFACE ${LIBC_HEADERS})
# Fake include of empty file to force rebuild when compiler is updated
target_compile_options(monza_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-include ${CMAKE_CURRENT_BINARY_DIR}/compiler_updated.h>)
add_dependencies(monza_compatibility compiler_updated)

# Linker options
target_link_options(monza_compatibility INTERFACE
  -fuse-ld=lld
  -nostdlib
  -static
  -Wl,--gc-sections
  -z norelro
  -Wl,--no-export-dynamic
  -Wl,--build-id=none
)

# Verona settings
target_compile_definitions(monza_compatibility INTERFACE VERONA_EXTERNAL_THREADING)
target_compile_definitions(monza_compatibility INTERFACE VERONA_EXTERNAL_SEMAPHORE_IMPL)
# Only enable systematic testing on the CI for specific builds
if (MONZA_SYSTEMATIC_BUILD)
  target_compile_definitions(monza_compatibility INTERFACE USE_SYSTEMATIC_TESTING)
endif()
if(USE_SCHED_STATS)
  target_compile_definitions(monza_compatibility INTERFACE -DUSE_SCHED_STATS)
endif()
target_compile_definitions(monza_compatibility INTERFACE -DSNMALLOC_CHEAP_CHECKS)

# Interface library for user compartments that will only be running in ring 3
add_library(monza_compartment_compatibility INTERFACE)
target_compile_options(monza_compartment_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-mcompartment>)
target_compile_definitions(monza_compartment_compatibility INTERFACE MONZA_COMPARTMENT_NAMESPACE)
target_link_libraries(monza_compartment_compatibility INTERFACE
  monza_compatibility
)

# Interface library for the user application running in ring 0
add_library(monza_app INTERFACE)
target_link_libraries(monza_app INTERFACE
  monza_compatibility
)

# Interface library for building system components that will always run in ring 0
add_library(monza_kernel_compatibility INTERFACE)
target_include_directories(monza_kernel_compatibility INTERFACE include/private)
target_include_directories(monza_kernel_compatibility INTERFACE experimental/include)
target_link_libraries(monza_kernel_compatibility INTERFACE
  monza_compatibility
)

# Interface library for building system components that could run in ring 3
add_library(monza_hybrid_compatibility INTERFACE)
target_include_directories(monza_hybrid_compatibility INTERFACE include/private)
target_include_directories(monza_hybrid_compatibility INTERFACE experimental/include)
target_compile_options(monza_hybrid_compatibility INTERFACE $<$<COMPILE_LANGUAGE:C,CXX>:-mcompartment>)
target_link_libraries(monza_hybrid_compatibility INTERFACE
  monza_compatibility
)

# Bootloader to work with QEMU
add_subdirectory(bootloader/pure64 ./bootloader EXCLUDE_FROM_ALL)

# snmalloc interface library
set(SNMALLOC_HEADER_ONLY_LIBRARY ON)
set(SNMALLOC_PLATFORM_HAS_GETENTROPY OFF)
add_subdirectory(../external/snmalloc ./external/snmalloc EXCLUDE_FROM_ALL)
target_compile_definitions(snmalloc INTERFACE SNMALLOC_DISABLE_ABA_VERIFY)
# Hack to remove phtreads forcefully added to snmalloc link add_dependencies
# TODO: change snmalloc and push the pthreads dependency downstream to users of snmalloc that need interface
set_target_properties(snmalloc PROPERTIES INTERFACE_LINK_LIBRARIES "")
# Link option, but in fact snmalloc is an INTERFACE library that configures compiler options
target_link_libraries(monza_compatibility INTERFACE snmalloc)

# Push all low-level system libraries into one group as they depend on each other
target_link_libraries(monza_app INTERFACE
  -Wl,--start-group
)

# OpenSSL (crypto potentially used by architectural bits)
add_subdirectory(openssl openssl EXCLUDE_FROM_ALL)
target_link_libraries(monza_app INTERFACE monza_openssl)

# compiler-rt with Monza compatibility
set(COMPILER_RT_BAREMETAL_BUILD ON CACHE BOOL
  "Build for a bare-metal target." FORCE)
set(COMPILER_RT_BUILD_BUILTINS ON)
set(COMPILER_RT_BUILD_SANITIZERS OFF)
set(COMPILER_RT_BUILD_XRAY OFF)
add_subdirectory(../external/llvm-project/compiler-rt compiler-rt EXCLUDE_FROM_ALL)
target_link_libraries(clang_rt.builtins-${ARCH} PRIVATE monza_hybrid_compatibility)
add_library(compiler-rt-builtins ALIAS clang_rt.builtins-${ARCH})
target_link_libraries(monza_app INTERFACE compiler-rt-builtins)

# Monza-libc
set(MONZA_LIBC_LIBCXX_HEADERS ${LIBCXX_HEADERS})
set(MONZA_LIBC_COMPILER_HEADERS ${COMPILER_HEADERS})
add_subdirectory(monza-libc ${LIBC} EXCLUDE_FROM_ALL)
add_dependencies(monza_compatibility monza_libc_headers)
target_link_libraries(monza_libc_tophalf_musl PRIVATE monza_hybrid_compatibility)
target_link_libraries(monza_libc_tophalf PRIVATE monza_hybrid_compatibility)
target_link_libraries(monza_libc_bottomhalf PRIVATE monza_hybrid_compatibility)
target_link_libraries(monza_app INTERFACE monza_libc)

# libcxxrt with Monza compatibility
set(LIBCXXRT_NO_EMERGENCY_BUFFERS ON)
add_subdirectory(../external/libcxxrt ./external/libcxxrt EXCLUDE_FROM_ALL)
target_link_libraries(cxxrt-static monza_hybrid_compatibility)
target_compile_definitions(cxxrt-static PRIVATE LIBCXXRT_WEAK_LOCKS)
target_link_libraries(monza_app INTERFACE cxxrt-static)
target_include_directories(monza_compatibility INTERFACE ../external/libcxxrt/src)
# libcxx_compatibility should be a PRIVATE link library, but we cannot set this here due to the CMakeLists in libcxxrt
# Also remove dependence on libdl
set_target_properties(cxxrt-static PROPERTIES INTERFACE_LINK_LIBRARIES "")

# LLVM libunwind with Monza compatibility
set(LIBUNWIND_IS_BAREMETAL ON)
set(LIBUNWIND_ENABLE_SHARED OFF)
set(LIBUNWIND_ENABLE_STATIC ON)
set(LIBUNWIND_ENABLE_ASSERTIONS OFF)
# Disable threads since globals are promoted to TLS
set(LIBUNWIND_ENABLE_THREADS OFF)
set(LIBUNWIND_USE_COMPILER_RT ON)
set(LIBUNWIND_HAS_C_LIB OFF)
set(LIBUNWIND_HAS_DL_LIB OFF)
set(LIBUNWIND_HAS_PTHREAD_LIB OFF)
set(LIBUNWIND_SUPPORTS_NOSTDLIBXX_FLAG OFF)
set(LIBUNWIND_SUPPORTS_NODEFAULTLIBS_FLAG OFF)
# Already set for the Monza build, no need to set for libunwind
set(LIBUNWIND_HAS_NOSTDINCXX_FLAG OFF)
add_subdirectory(../external/llvm-project/libunwind libunwind EXCLUDE_FROM_ALL)
target_link_libraries(unwind_static PRIVATE monza_hybrid_compatibility)
# Disable warnings from baremetal build and tracing
target_compile_options(unwind_static PRIVATE
  -Wno-unused-but-set-variable
  -Wno-unused-parameter
)
target_link_libraries(monza_app INTERFACE unwind_static)

# libcxx with Monza compatibility
add_subdirectory(monza-libcxx ${LIBCXX} EXCLUDE_FROM_ALL)
target_link_libraries(monza_compatibility INTERFACE monza_libcxx_headers)
# Some of the STL functionality (ex: streams) will fail inside compartments,
# but there is no workaround for globals in header-only libraries at the moment.
#target_link_libraries(monza_libcxx PRIVATE monza_hybrid_compatibility)
target_link_libraries(monza_libcxx PRIVATE monza_kernel_compatibility)
target_link_libraries(monza_app INTERFACE monza_libcxx)

# Architecture-specific part of Monza runtime
set(LINKER_SCRIPT arch/${ARCH}/linker.ld)
add_subdirectory(arch/${ARCH} monza_rt_arch EXCLUDE_FROM_ALL)
target_link_libraries(monza_app INTERFACE monza_rt_arch)

# Set up linker script once architecture selected
set_target_properties(monza_compatibility PROPERTIES INTERFACE_LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT})
target_link_options(monza_compatibility INTERFACE -Wl,-T${CMAKE_CURRENT_SOURCE_DIR}/${LINKER_SCRIPT})

# Architecture-independent part of Monza runtime
unset(SRC)
aux_source_directory(crt SRC)
add_library(monza_rt STATIC ${SRC})
target_link_libraries(monza_rt PRIVATE monza_kernel_compatibility)

target_link_libraries(monza_app INTERFACE monza_rt)

# End of low-level system libraries for Monza
target_link_libraries(monza_app INTERFACE -Wl,--end-group)

# Test the linkage of the full C++ language runtimes
add_subdirectory(cxx-linktest EXCLUDE_FROM_ALL)
add_dependencies(monza_app monza_cxx_linktest)

# Experimental parts of Monza runtime
# Not added to monza_app, can be selected on-demand
add_subdirectory(experimental monza_experimental EXCLUDE_FROM_ALL)

warnings_high()

enable_testing()

macro(add_qemu_compatible_image new_target target)
  set(image_file ${new_target}.img)
  add_custom_target(${new_target}
      DEPENDS ${image_file}
    )
  set_target_properties(${new_target} PROPERTIES OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/${image_file}
    )
  add_custom_command(OUTPUT ${image_file}
      COMMAND
        ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> ${new_target}.bin &&
        cat $<TARGET_FILE:bootloader.sys> ${new_target}.bin > ${image_file}
      DEPENDS ${target} bootloader.sys
      BYPRODUCTS ${new_target}.bin
    )
endmacro()

macro(compartment_target_to_linkage_name compartment_name target)
  string(REPLACE "-" "_" compartment_name ${target})
endmacro()

macro(compartment_target_to_linkage_name compartment_name target)
  string(REPLACE "-" "_" compartment_name ${target})
endmacro()

macro(create_test target_name artefact_list test_name test_folder category)
  subdirlist(compartments ${test_folder})
  set(compartment_list "")
  foreach(compartment ${compartments})
    unset(SRC)
    aux_source_directory(${test_folder}/${compartment} SRC)
    add_library(${test_name}-${compartment} STATIC ${SRC})
    target_include_directories(${test_name}-${compartment} PRIVATE test)
    target_link_libraries(${test_name}-${compartment} PRIVATE monza_compartment_compatibility)
    list(APPEND compartment_list ${test_name}-${compartment})
  endforeach()
  unset(SRC)
  aux_source_directory(${test_folder} SRC)
  add_executable(${test_name} ${SRC})
  target_include_directories(${test_name} PRIVATE test)
  target_link_libraries(${test_name} PRIVATE ${compartment_list})
  target_link_libraries(${test_name} PRIVATE monza_app)
  # Tests may need access to private headers
  target_include_directories(${test_name} PRIVATE include/private)
  # Tests can also validate experimental features
  target_link_libraries(${test_name} PRIVATE monza_experimental)
  add_qemu_compatible_image(qemu-${test_name} ${test_name})
  if ((${test_name} STREQUAL "io-udp-echo") OR (${test_name} STREQUAL "io-tcp-echo") OR (${test_name} STREQUAL "io-ssl-echo"))
    add_test(${test_name} ${test_folder}/run-io-test.sh)
  else()
    set(memory_option_string "-m 1G")
    if ((${test_name} STREQUAL "crt-malloc"))
      set(memory_option_string "-m 8G")
    else ((${test_name} STREQUAL "io-shmem"))
      set(memory_option_string "-m 1G,slots=2,maxmem=1T \
        -object memory-backend-file,id=shmem,share=on,mem-path=mem.bin,size=64M,align=2M \
        -device pc-dimm,memdev=shmem,addr=0xfffc000000")
    endif()
    add_test(${test_name} sh -c "qemu-system-${ARCH} -cpu IvyBridge -no-reboot -nographic -kernel qemu-${test_name}.img -smp 4 ${memory_option_string} |\
      awk -F: 'BEGIN { not_found = 1; } index($0, \"CRITICAL: Execution finished with 0.\") > 0 { not_found = 0; } { print $0; } END { exit not_found; }'")
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 300)
  endif()
  list(APPEND ${artefact_list} $<TARGET_FILE:${test_name}> ${CMAKE_CURRENT_BINARY_DIR}/qemu-${test_name}.img)
  add_dependencies(${target_name} qemu-${test_name})
endmacro()

macro(subdirlist result curdir)
  file(GLOB children LIST_DIRECTORIES true CONFIGURE_DEPENDS RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

add_custom_target(monza_tests ALL)
set(test_artefacts "")

set(TESTDIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
subdirlist(TEST_CATEGORIES ${TESTDIR})
foreach(TEST_CATEGORY ${TEST_CATEGORIES})
  subdirlist(TESTS ${TESTDIR}/${TEST_CATEGORY})
  foreach(TEST ${TESTS})
    create_test(monza_tests test_artefacts "${TEST_CATEGORY}-${TEST}" ${TESTDIR}/${TEST_CATEGORY}/${TEST} ${TEST_CATEGORY})
  endforeach()
endforeach()

add_custom_target(rt_tests)

set(TESTDIR ${CMAKE_CURRENT_SOURCE_DIR}/../external/verona/src/rt/test)
#subdirlist(TEST_CATEGORIES ${TESTDIR})
set(TEST_CATEGORIES "func")
foreach(TEST_CATEGORY ${TEST_CATEGORIES})
  #subdirlist(TESTS ${TESTDIR}/${TEST_CATEGORY})
  set(TESTS 
      #"cownchain"
      #"cowngc1"
      #"hashmap"
      #memory"
    )
  foreach(TEST ${TESTS})
    create_test(rt_tests test_artefacts "${TEST_CATEGORY}-con-${TEST}" ${TESTDIR}/${TEST_CATEGORY}/${TEST})
  endforeach()
endforeach()

install(FILES ${test_artefacts}
  DESTINATION ${GUEST_TEST_INSTALL}
)
