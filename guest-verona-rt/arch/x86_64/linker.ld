ENTRY(start)

IMAGE_START = 0x600000;
HV_PAGE_SIZE = 0x1000;
PAGE_SIZE = 0x200000;
EARLY_STACK_SIZE = 0x1000;
STACK_SIZE = 0x100000;
HV_LOADER_DATA_SIZE = 0x10000;

PHDRS
{
  loaded PT_LOAD ;
  tls PT_TLS ;
  eh_frame PT_GNU_EH_FRAME ;
}

SECTIONS
{
  . = IMAGE_START;

  .text : ALIGN(PAGE_SIZE)
  {
    __text_start = .;
    __elf_start = .;
    *(.text.start)
    *(.text)
    *(.text.*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(HV_PAGE_SIZE);
    __hv_hypercall_codepage_start = .;
    . = . + HV_PAGE_SIZE;
    __text_end = .;
	} : loaded

  .gcc_except_table : ALIGN(8)
  {
    __gcc_except_table_start = .;
    *(.gcc_except_table)
    *(.gcc_except_table.*)
    __gcc_except_table_end = .;
  }

  .rodata : ALIGN(8)
  {
    __rodata_start = .;
    *(.rodata)
    *(.rodata.*)
    __rodata_end = .;
  }

  .protected_data : ALIGN(8)
  {
    __protected_data_start = .;
    *(.protected_data)
    __protected_data_end = .;
  }

  /* Include the list of Monza initialization functions. */
  .__monza_init : ALIGN(8)
  {
    __monza_init_start = .;
    KEEP (*(__monza_init))
    __monza_init_end = .;
  }

  /* Include the list of initialization functions sorted. */
  .init_array : ALIGN(8)
  {
    __init_array_start = .;
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array))
    __init_array_end = .;
  }

  /* Include the list of termination functions sorted. */
  .fini_array : ALIGN(8)
  {
    __fini_array_start = .;
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array))
    __fini_array_end = .;
  }

  .eh_frame : ALIGN(8)
  {
    __eh_frame_start = .;
    KEEP(*(.eh_frame))
    __eh_frame_end = .;
  }

  .eh_frame_hdr : ALIGN(8)
  {
    __eh_frame_hdr_start = .;
    KEEP(*(.eh_frame_hdr))
    __eh_frame_hdr_end = .;
  } : loaded : eh_frame

  .tdata : ALIGN(8)
  {
    __tdata_start = .;
    *(.tdata)
    *(.tdata.*)
    __tdata_end = .;
  } : loaded : tls

  /* Alignment needs to match TBSS_ALIGNMENT in tls.cc. */
  /* Linker can choose to make this larger without feedback so make it large by default. */
  .tbss : ALIGN(4096)
  {
    *(.tbss)
    *(.tbss.*)
  } : tls

  /* Align the DATA to the PAGE_SIZE boundary as this is the boundary of read-only loaded data */
  .data : ALIGN(PAGE_SIZE)
  {
    __elf_writable_start = .;
    __stack_size = .;
    QUAD(STACK_SIZE)
    __tbss_size = .;
    QUAD(SIZEOF(.tbss))
    *(.data)
    *(.data.*)
  } : loaded

  /* Maybe we can find a way for this to not be emitted */
  .got : ALIGN(8)
  {
    *(.got)
  }

  /* Zero-initialized early stack used during hypervisor detection */
  .early_stack : ALIGN(16)
  {
    __early_stack_start = .;
    . = . + EARLY_STACK_SIZE;
    __early_stack_end = .;
  }

  /* HV loader adds some boot data after the last loaded section, which needs to be skipped */
  .hv_loader_data : ALIGN(HV_PAGE_SIZE)
  {
    __hv_loader_data_start = .;
    . = . + HV_LOADER_DATA_SIZE;
  } : NONE

  /* Align the BSS (which includes all other unloaded memory as well) to the PAGE_SIZE */
  /* This ensures that any platform-specific memory acceptence can be done at PAGE_SIZE granularity */
  .bss (NOLOAD) : ALIGN(PAGE_SIZE)
  {
    __unloaded_start = .;
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    __bss_end = .;
    /* To guarantee 16-byte alignment of stack */
    . = ALIGN(16);
    __stack_start = .;
    . = . + STACK_SIZE;
    __stack_end = .;
    /* PAGE_SIZE alignment as these need special permissions in the pagetable */
    . = ALIGN(PAGE_SIZE);
    __interrupt_stacks_start = .;
    *(.interrupt_stacks)
    __interrupt_stacks_end = .;
    /* PAGE_SIZE alignment to revert to regular pagetable mappings */
    . = ALIGN(PAGE_SIZE);
    __heap_start = .;
  } : NONE
}
