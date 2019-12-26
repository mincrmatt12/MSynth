/* MSynth bootloader linker file
 *
 * Sets up a minimal environment.
 *
 * The application linker scripts all assume a fixed base address (this is/was to simplify the initial programming
 * before updating was supported)
 *
 */

/* The entry point is the Reset_Handler (CMSIS naming) */
ENTRY(Reset_Handler)

/* Initial stack */
_estack = 0x10010000; /* end of CCRAM */

MEMORY {
	RAM (rwx) : ORIGIN = 0x20020000, LENGTH = 64K
	FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 16K
	VRAM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
	CCMRAM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
	BKPRAM (rw) : ORIGIN = 0x40024000, LENGTH = 4K
}

SECTIONS {
	.isr_vector : {
		. = ALIGN(4);
		KEEP(*(.isr_vector)) /* ISR vector table */
		. = ALIGN(4);
	} >FLASH

	/* Main program text */
	.text :  {
		. = ALIGN(4);
		*(.text)       
		*(.text*)      
		*(.glue_7)     
		*(.glue_7t)    
					   
		KEEP (*(.init)) /* ensure we don't delete constructor data */
		KEEP (*(.fini))
					   
		. = ALIGN(4);
		_etext = .;
	} >FLASH

	/* constant data*/
	.rodata : {
		. = ALIGN(4);
		*(.rodata)         /* .rodata sections (constants, strings, etc.) */
		*(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
		. = ALIGN(4);
	} >FLASH

	/* Provide dummy exidx symbols */
	__exidx_start = 0;
	__exidx_end = 0;

	.preinit_array : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array*))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	} >FLASH
	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array*))
		PROVIDE_HIDDEN (__init_array_end = .);
	} >FLASH
	.fini_array : {
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array*))
		PROVIDE_HIDDEN (__fini_array_end = .);
	} >FLASH

	/* used by the startup to initialize data */
	_sidata = LOADADDR(.data);

	/* Initialized data sections goes into RAM, load LMA copy after code */
	.data : 
	{
		. = ALIGN(4);
		_sdata = .;        /* create a global symbol at data start */
		*(.data)           /* .data sections */
		*(.data*)          /* .data* sections */

		. = ALIGN(4);
		_edata = .;        /* define a global symbol at data end */
	} >RAM AT> FLASH


	/* Uninitialized data section */
	. = ALIGN(4);
	.bss :
	{
		/* This is used by the startup in order to initialize the .bss secion */
		_sbss = .;         /* define a global symbol at bss start */
		__bss_start__ = _sbss;
		*(.bss)
		*(.bss*)
		*(COMMON)

		. = ALIGN(4);
		_ebss = .;         /* define a global symbol at bss end */
		__bss_end__ = _ebss;
	} >RAM

	/* VRAM */
	.vram : {
		. = ALIGN(4);
		framebuffer_data = .;
		. = . + (480*272);
		*(.vram)           /* VRAM data (something to fill up those handy 512 bytes) */

		. = ALIGN(4);
	} >VRAM

	/* CCMRAM */
	.ccmdata : {
		. = ALIGN(4);
		*(.ccmdata)
		*(.ccmdata*)

		. = ALIGN(4);
		PROVIDE(end = .);
		PROVIDE(_end = .);
		/* Bootloader does not init CCMRAM */
	} >CCMRAM

	.bkpram : {
		. = . + (4096 - 128);
		bootloader_data = .;
	} >BKPRAM

	/* discard data from libc/m/gcc, as well as all exception data */
	/DISCARD/ : {
		libc.a (*)
		libm.a (*)
		libgcc.a (*)

		*(.ARM.ex*)
	}

	.ARM.attributes 0 : { *(.ARM.attributes) }
}
