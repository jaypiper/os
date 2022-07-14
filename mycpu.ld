ENTRY(_start)

SECTIONS {
  /* _pmem_start and _entry_offset are defined in LDFLAGS */
  . = _addr_start;
  .text : {
    *(entry)
    *(.text*)
  }

  _initcode_start = ALIGN(0x1000);
  . = _initcode_start;
  .initcode : {
    KEEP(*(initcode_entry))
    KEEP(*(initcode_text))
    KEEP(*(initcode_data))
  }
  _initcode_end = .;

/*  . = _ram_start; */
  .rodata : {
    *(.rodata*)
  }
  .data : {
    *(.data)
  }
  edata = .;
  _data = .;
  .bss : {
	_bss_start = .;
    *(.bss*)
    *(.sbss*)
    *(.scommon)
  }
  _stack_top = ALIGN(0x1000);
  . = _stack_top + 0x8000;
  _stack_pointer = .;
  end = .;
  _end = .;
  _heap_start = ALIGN(0x1000);
}
