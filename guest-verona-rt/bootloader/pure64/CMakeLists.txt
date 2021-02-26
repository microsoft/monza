set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f bin -o <OBJECT> <SOURCE>")
set(CMAKE_ASM_NASM_LINK_EXECUTABLE "cat <OBJECTS> > <TARGET> && truncate -s 5M <TARGET>")

# Ordering matters a lot here!
# Multiboot first, then pure64 only after that any local files.
set(SRC
    ../../../external/pure64/src/bootsectors/multiboot.asm
    ../../../external/pure64/src/pure64.asm
    emulate.asm
  )
add_executable(bootloader.sys ${SRC})
target_compile_options(bootloader.sys PRIVATE -i${CMAKE_CURRENT_SOURCE_DIR}/../../../external/pure64/src/)