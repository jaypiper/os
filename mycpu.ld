ENTRY(_start)
/*
PHDRS {
  text PT_LOAD FILEHDR PHDRS;
  initcode PT_LOAD;
  data PT_LOAD;
}
*/
SECTIONS {
  /* _pmem_start and _entry_offset are defined in LDFLAGS */
  . = _addr_start;
  .text : {
    *(entry)
    *(.text*)
  }
/*
  _initcode_start = ALIGN(0x1000);
  . = _initcode_start;
  .initcode : {
    *(.initcode)
  } : initcode
  _initcode_end = .;
*/
/*  . = _ram_start; */
  .rodata : {
    *(.rodata*)
  }
  .data : {
    *(.initcode)
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
