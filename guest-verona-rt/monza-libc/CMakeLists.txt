set(LIBC_TOP_HALF_MUSL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../external/musl)
set(LIBC_TOP_HALF_MUSL_SRC_DIR ${LIBC_TOP_HALF_MUSL_DIR}/src)

set(LIBC_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/include)

set (LIBC_TOP_HALF_MUSL_SOURCES "")

macro(add_sources list_to_append root_folder local_path)
  set(temp_list ${ARGN})
  list(TRANSFORM temp_list PREPEND ${root_folder}/${local_path}/)
  list(APPEND ${list_to_append} ${temp_list})
endmacro()

macro(add_all_sources_except list_to_append root_folder local_path)
  file(GLOB temp_list RELATIVE ${root_folder}/${local_path} CONFIGURE_DEPENDS ${root_folder}/${local_path}/*.c)
  if(${ARGC} GREATER 3)
    list(REMOVE_ITEM temp_list ${ARGN})
  endif()
  list(TRANSFORM temp_list PREPEND ${root_folder}/${local_path}/)
  list(APPEND ${list_to_append} ${temp_list})
endmacro()

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} errno
    # Excluded since it uses pthread, errno.cc offers alternative implementation.
    __errno_location.c
  )

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} env
    # For the initialization of the thread-local internal state.
    __init_tls.c
  )

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} exit
    # Exclude since it uses signals and syscalls, abort.cc offers alternative implementation.
    abort.c
    abort_lock.c
    exit.c
    _Exit.c
  )

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} string
    # Excluded since architecture-specific implementation used instead.
    memcpy.c
    memmove.c
    memset.c
    # Excluded as signals not available.
    strsignal.c
  )
list(APPEND LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR}/string/${ARCH}/memcpy.s)
list(APPEND LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR}/string/${ARCH}/memmove.s)
list(APPEND LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR}/string/${ARCH}/memset.s)
add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} multibyte)
add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} ctype)

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} math)
add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} complex)
add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} fenv
    fenv.c
  )
list(APPEND LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR}/fenv/${ARCH}/fenv.s)

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} internal
    # These are the basis of number<->string conversion routines and need to be included.
    floatscan.c
    intscan.c
    shgetc.c
    # This represents the core internals.
    libc.c
  )

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} stdio
    # These are the files needed to support stdout, stderr and string operations.
    __lockfile.c
    __overflow.c
    __stdio_exit.c
    __stdio_write.c
    __stdout_write.c
    __toread.c
    __towrite.c
    __uflow.c
    asprintf.c
    ferror.c
    fflush.c
    fgetc.c
    fgetln.c
    fgets.c
    fgetwc.c
    fgetws.c
    fprintf.c
    fputc.c
    fputs.c
    fputwc.c
    fread.c
    fscanf.c
    fwide.c
    fwprintf.c
    fwrite.c
    fwscanf.c
    getc.c
    getc_unlocked.c
    getdelim.c
    getline.c
    getwc.c
    ofl.c
    printf.c
    putc.c
    putc_unlocked.c
    putchar.c
    putchar_unlocked.c
    puts.c
    putw.c
    putwc.c
    putwchar.c
    snprintf.c
    sprintf.c
    sscanf.c
    stderr.c
    stdout.c
    swprintf.c
    swscanf.c
    ungetc.c
    ungetwc.c
    vasprintf.c
    vprintf.c
    vsnprintf.c
    vsprintf.c
    vsscanf.c
    vswprintf.c
    vswscanf.c
    vfprintf.c
    vfscanf.c
    vfwprintf.c
    vfwscanf.c
  )

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} stdlib)

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} locale
    # Disable all functionality regarding manual locale manipulation.
    duplocale.c
    freelocale.c
    newlocale.c
    setlocale.c
    uselocale.c
    # What is this?
    catopen.c
    catclose.c
    catget.c
    # Disable textdomain stuff.
    bind_textdomain_codeset.c
    dcngettext.c
    textdomain.c
  )

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} regex
    # No reason for globbing without files (header also skipped).
    glob.c
  )

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} search)

add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} prng)

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} thread
    __lock.c
  )

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} network
    # These are the files needed to support in-memory conversions.
    htonl.c
    htons.c
    ntohl.c
    ntohs.c
)

add_sources(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} time
    __month_to_secs.c
    __secs_to_tm.c
    __tm_to_secs.c
    __year_to_secs.c
    __tz.c
    asctime.c
    asctime_r.c
    clock.c
    ctime.c
    ctime_r.c
    gettimeofday.c
    gmtime.c
    gmtime_r.c
    localtime.c
    localtime_r.c
    mktime.c
    strftime.c
    strptime.c
    time.c
    timegm.c
    timespec_get.c
  )

#add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} select)
#add_all_sources_except(LIBC_TOP_HALF_MUSL_SOURCES ${LIBC_TOP_HALF_MUSL_SRC_DIR} conf)

set(MUSL_HEADERS
    arch/${ARCH}/atomic_arch.h

    arch/generic/fp_arch.h

    include/alloca.h
    include/assert.h
    include/byteswap.h
    include/complex.h
    include/ctype.h
    include/dlfcn.h
    include/endian.h
    include/errno.h
    include/features.h
    include/fenv.h
    include/float.h
    include/fnmatch.h
    include/iconv.h
    include/inttypes.h
    include/iso646.h
    include/langinfo.h
    include/limits.h
    include/locale.h
    include/malloc.h
    include/math.h
    include/memory.h
    include/monetary.h
    include/mqueue.h
    include/nl_types.h
    include/pthread.h
    include/regex.h
    include/sched.h
    include/search.h
    include/signal.h
    include/stdalign.h
    include/stdarg.h
    include/stdbool.h
    include/stdc-predef.h
    include/stddef.h
    include/stdint.h
    include/stdio.h
    include/stdlib.h
    include/stdnoreturn.h
    include/string.h
    include/strings.h
    include/tgmath.h
    include/time.h
    include/threads.h
    include/uchar.h
    include/values.h
    include/wchar.h
    include/wctype.h
  )
list(TRANSFORM MUSL_HEADERS PREPEND ${LIBC_TOP_HALF_MUSL_DIR}/)

set(MUSL_HEADERS_BITS
    arch/${ARCH}/bits/fenv.h
    arch/${ARCH}/bits/float.h
    arch/${ARCH}/bits/io.h
    arch/${ARCH}/bits/limits.h
    arch/${ARCH}/bits/mman.h
    arch/${ARCH}/bits/posix.h
    arch/${ARCH}/bits/reg.h
    arch/${ARCH}/bits/signal.h
    arch/${ARCH}/bits/stdint.h

    arch/generic/bits/errno.h
    arch/generic/bits/hwcap.h
  )
list(TRANSFORM MUSL_HEADERS_BITS PREPEND ${LIBC_TOP_HALF_MUSL_DIR}/)

set(MUSL_HEADERS_SYS
    include/sys/mman.h
    include/sys/select.h
    include/sys/socket.h
    include/sys/time.h
    include/sys/types.h
  )
list(TRANSFORM MUSL_HEADERS_SYS PREPEND ${LIBC_TOP_HALF_MUSL_DIR}/)

set(MUSL_HEADERS_ARPA
    include/arpa/inet.h
  )
list(TRANSFORM MUSL_HEADERS_ARPA PREPEND ${LIBC_TOP_HALF_MUSL_DIR}/)

set(MUSL_HEADERS_NETINET
    include/netinet/in.h
  )
list(TRANSFORM MUSL_HEADERS_NETINET PREPEND ${LIBC_TOP_HALF_MUSL_DIR}/)

file(COPY ${MUSL_HEADERS} DESTINATION ${LIBC_HEADERS})
file(COPY ${MUSL_HEADERS_BITS} DESTINATION ${LIBC_HEADERS}/bits)
file(COPY ${MUSL_HEADERS_SYS} DESTINATION ${LIBC_HEADERS}/sys)
file(COPY ${MUSL_HEADERS_ARPA} DESTINATION ${LIBC_HEADERS}/arpa)
file(COPY ${MUSL_HEADERS_NETINET} DESTINATION ${LIBC_HEADERS}/netinet)

file(COPY ${MUSL_HEADERS} DESTINATION ${LIBC_HEADERS})

add_custom_target(monza_libc_headers)

add_custom_target(libc_generated_headers
    DEPENDS
      ${LIBC_HEADERS}/bits/alltypes.h
      ${LIBC_HEADERS}/bits/syscall.h
  )
add_custom_command(OUTPUT ${LIBC_HEADERS}/bits/alltypes.h
    COMMAND
      sed -f ${LIBC_TOP_HALF_MUSL_DIR}/tools/mkalltypes.sed
      ${LIBC_TOP_HALF_MUSL_DIR}/arch/${ARCH}/bits/alltypes.h.in
      ${LIBC_TOP_HALF_MUSL_DIR}/include/alltypes.h.in > ${LIBC_HEADERS}/bits/alltypes.h
    DEPENDS
      ${LIBC_TOP_HALF_MUSL_DIR}/tools/mkalltypes.sed
      ${LIBC_TOP_HALF_MUSL_DIR}/arch/${ARCH}/bits/alltypes.h.in
      ${LIBC_TOP_HALF_MUSL_DIR}/include/alltypes.h.in
  )
add_custom_command(OUTPUT ${LIBC_HEADERS}/bits/syscall.h
    COMMAND touch ${LIBC_HEADERS}/bits/syscall.h
  )
add_dependencies(monza_libc_headers libc_generated_headers)

add_library(monza_libc_tophalf_musl STATIC ${LIBC_TOP_HALF_MUSL_SOURCES})
target_include_directories(monza_libc_tophalf_musl PRIVATE
    ${LIBC_TOP_HALF_MUSL_SRC_DIR}/include
    ${LIBC_TOP_HALF_MUSL_SRC_DIR}/internal
    ${LIBC_HEADERS}
  )
target_compile_options(monza_libc_tophalf_musl PRIVATE
    -Wno-parentheses
    -Wno-shift-op-parentheses
    -Wno-bitwise-op-parentheses
    -Wno-logical-op-parentheses
    -Wno-string-plus-int
    -Wno-dangling-else
    -Wno-unknown-pragmas
    -Wno-unused-command-line-argument
    -Wno-format
    -UNDEBUG
  )
target_compile_definitions(monza_libc_tophalf_musl PRIVATE
    __libc_malloc=malloc
    __libc_calloc=calloc
    _INTERNAL_SYSCALL_H
  )
add_dependencies(monza_libc_tophalf_musl monza_libc_headers)

add_library(monza_libc_tophalf STATIC "")
add_subdirectory(libc-top-half)
target_include_directories(monza_libc_tophalf SYSTEM PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${MONZA_LIBC_LIBCXX_HEADERS}>)
target_include_directories(monza_libc_tophalf SYSTEM PRIVATE ${LIBC_HEADERS})
target_include_directories(monza_libc_tophalf SYSTEM PRIVATE ${MONZA_LIBC_COMPILER_HEADER})
add_dependencies(monza_libc_tophalf monza_libc_headers)
target_link_libraries(monza_libc_tophalf PUBLIC monza_libc_tophalf_musl)

add_library(monza_libc_bottomhalf STATIC "")
add_subdirectory(libc-bottom-half/src)
target_include_directories(monza_libc_bottomhalf SYSTEM PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${MONZA_LIBC_LIBCXX_HEADERS}>)
target_include_directories(monza_libc_bottomhalf SYSTEM PRIVATE ${LIBC_HEADERS})
target_include_directories(monza_libc_bottomhalf SYSTEM PRIVATE ${MONZA_LIBC_COMPILER_HEADER})
add_dependencies(monza_libc_bottomhalf monza_libc_headers)

add_library(monza_libc INTERFACE)
target_link_libraries(monza_libc INTERFACE
    monza_libc_tophalf
    monza_libc_bottomhalf
    ${MONZA_LIBC_COMPILERRT_LIBS}
  )
