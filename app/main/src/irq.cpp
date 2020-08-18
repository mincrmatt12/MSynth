#include <stm32f4xx.h>
#include "irq.h"
#include "in.h"

#include <msynth/util.h>
#include <msynth/sd.h>

#include "audio.h"

// ISRs

ISR(SDIO) {
	sd::sdio_interrupt();
}


ISR(USB_OTG_HS) {
	ms::in::usb_host.usb_global_irq();
}

ISR(DMA1_Stream4) {
	ms::audio::dma_interrupt();
}

void ms::irq::init() {
	// Enable DMA interrupts.
	NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	NVIC_SetPriority(DMA1_Stream4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0)); // TODO: pick a reasonable priority for this.
}
