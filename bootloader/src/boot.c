#include "boot.h"
#include <stm32f4xx.h>
#include <core_cm4.h>
#include "sys.h"

void boot_app(const app_hdr *app) {
	// TODO: load the BKP data correctly
	
	// Disable all interrupts
	RCC->CIR = 0;

	// TODO: turn off the various peripherals
	
	uint32_t topofstack = app->topofstack;
	uint32_t entry      = app->entry;

	// Load the important information into registers r0-r1
	asm volatile (
		"msr msp, %[TopOfStack]\n\t"
		"ldr lr, =app_return_handler\n\t"
		"bx %[StartAddr]\n\t"
		: [TopOfStack]"=r"(topofstack), [StartAddr]"=r"(entry)
	);
	
	// The app returned; it shouldn't since we _will_ set the
	// return address to something different by messing with the stack.
	//
	// This causes the app to jump to a specific handler, which is capable of
	// parsing the data contained within the bootcmd struct and/or restarting
	// cleanly in case of error. It also supports invoking the debug handlers.
	//
	// It should _not_ call reset.
	
	// TEMP: spin
	// no msg, since USART3 may have been reconfigured
	while (1) {;}
}

void __attribute__((naked)) app_return_handler() {
	asm volatile (
		"ldr r0, =0x08000000\n\t"
		"msr msp, r0\n\t"
		"ldr r1, =app_return_handler_impl\n\t"
		"bx r1\n\t"
	);
}

extern int _write(int, const char *, int);

void app_return_handler_impl() {
	// rerun sys-init
	sys_init();
	// directly _write
	_write(0, "app returned\n", 13);
	// reset
	NVIC_SystemReset();
}
