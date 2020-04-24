#include <stm32f4xx.h>
#include "irq.h"
#include "in.h"

#include <msynth/util.h>
#include <msynth/sd.h>

// ISRs

ISR(SDIO) {
	sd::sdio_interrupt();
}


ISR(USB_OTG_HS) {
	ms::in::usb_host.usb_global_irq();
}

void ms::irq::init() {
	// Currently no interesting IRQs.
}
