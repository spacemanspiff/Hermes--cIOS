OUTPUT_FORMAT("elf32-bigarm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
 

MEMORY {
	exe		: ORIGIN = 0x00000000, LENGTH = 0x4000
}



 
SECTIONS
{
    .debug_aranges 0 : { *(.debug_aranges) } .debug_pubnames 0 : { *(.debug_pubnames) } .debug_info 0 : { *(.debug_info) } .debug_abbrev 0 : { *(.debug_abbrev) } .debug_line 0 : { *(.debug_line) } .debug_frame 0 : { *(.debug_frame) } .debug_str 0 : { *(.debug_str) } .debug_loc 0 : { *(.debug_loc) } .debug_macinfo 0 : { *(.debug_macinfo) } .note.arm.ident 0 : { KEEP (*(.note.arm.ident)) }

    .init 0x0 :  { . = .; KEEP (*(.init)) } > exe

    .text ALIGN (0x4) : { 
    	*(.text*) 
    	*(.gnu.warning) 
    	*(.gnu.linkonce.t.*) 
    	*(.init) 
    	*(.glue_7) 
    	*(.glue_7t) } > exe
    .data : { *(.data*) *(.data1) *(.gnu.linkonce.d.*)  . = ALIGN (4);  __CTOR_LIST__ = ABSOLUTE (.); KEEP (*(SORT (.ctors*))) __CTOR_END__ = ABSOLUTE (.); __DTOR_LIST__ = ABSOLUTE (.); KEEP (*(SORT (.dtors*))) __DTOR_END__ = ABSOLUTE (.); *(.dynamic) *(.sdata*) *(.gnu.linkonce.s.*) . = ALIGN (4); *(.2ram.*) } > exe
    .fini : { . = .; *(.fini) } > exe
     .rodata ALIGN (0x4) : { 
    	. = .; 
    	*(.rodata*) 
    	*(.gnu.linkonce.r.*) } > exe
    .rodata1 ALIGN (0x4) : { . = .; *(.rodata1) } > exe
    .fixup ALIGN (0x4) : { . = .; *(.fixup) } > exe
    .gcc_except_table ALIGN (0x4) : { . = .; *(.gcc_except_table) } > exe
    .got ALIGN (0x4) : { __GOT_START = ABSOLUTE(.); *(.got.plt) *(.got) __GOT_END = ABSOLUTE(.); } > exe
    .bss ALIGN (0x4) : { *(.scommon) *(.dynsbss) *(.sbss*) *(.gnu.linkonce.sb.*) *(.dynbss) *(.bss*) *(.gnu.linkonce.b.*) *(COMMON)  } > exe
  	. = ALIGN(4);
}