#include <usb.h>

// This file contains various device types.
// as well as HostBase

void usb::HostBase::enable() {
	// Enable the HPRT interrupts
	USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_PRTIM;
	// Set FS
	USB_OTG_HS_HOST->HCFG = 0b101; // Set 48mhz FS (will adjust)

	// Turn on port power
	LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2); // dwrrrrrrrrrn
	USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PPWR;

	// Debounce
	util::delay(200);
}

void usb::HostBase::disable() {
	// Check the state of the USB
	if (inserted()) {
		// Device was inserted, so let's disable the port
		USB_OTG_HS_HPRT0 &= ~(USB_OTG_HPRT_PENA);

		util::delay(5);

	}

	// Kill port power
	USB_OTG_HS_HPRT0 &= ~(USB_OTG_HPRT_PPWR);
	LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);

	// TODO: Clean up after the old thing
	// End all channels
	// Clear all FIFOs
	// Cause nuclear reset
}

usb::init_status usb::HostBase::init_host_after_connect() {
	if (!inserted()) return init_status::NotInserted;

change_speed:
	// Start by resetting the port
	USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PRST;
	// Wait at least twice the value in the datasheet, you never know what bullcrap is in them these days
	util::delay(15);
	USB_OTG_HS_HPRT0 &= ~(USB_OTG_HPRT_PRST);
	// Wait for a PENCHNG interrupt
	while (!got_penchng) {;}
	got_penchng = 0;

	// Check enumerated speed
	if ((USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PSPD) == USB_OTG_HPRT_PSPD_0) {
		// Full speed
		if ((USB_OTG_HS_HOST->HCFG & USB_OTG_HCFG_FSLSPCS) != 1) {
			USB_OTG_HS_HOST->HCFG = 0b101; // Set the speed 
			util::delay(1);
			// Reset again
			goto change_speed;
		}

		USB_OTG_HS_HOST->HFIR = 47999U;
	}
	else if ((USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PSPD) == USB_OTG_HPRT_PSPD_1) {
		// Low speed
		if ((USB_OTG_HS_HOST->HCFG & USB_OTG_HCFG_FSLSPCS) != 2) {
			USB_OTG_HS_HOST->HCFG = 0b110; // Set the speed 
			util::delay(1);
			// Reset again
			goto change_speed;
		}

		USB_OTG_HS_HOST->HFIR = 6999U;
	}
	else {
		// not supported
		return init_status::NotSupported;
	}

	// Set the FIFO sizes
	//
	// Addresses are in units of words, maximum 0x140
	// We set up the bus with:
	// 	- 0x80 (128 word) RX and non-periodic TX fifo
	// 	- 0x3C (60 word) TX periodic fifo

	USB_OTG_HS->GRXFSIZ = 0x100;
	USB_OTG_HS->DIEPTXF0_HNPTXFSIZ = (0x80 << 16) | 0x100 /* start address */;
	USB_OTG_HS->HPTXFSIZ = (0x40 << 16) | 0x180;

	// We are now inited at the host level. Begin device enumeration.

	// At this point, the device has completed reset and is responding on ADDR 0
	return init_status::Ok;
}

void usb::HostBase::init() {
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_OTGHS);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);

	// Try resetting it the AHB way
	LL_AHB1_GRP1_ForceReset(LL_AHB1_GRP1_PERIPH_OTGHS);

	util::delay(2);

	LL_AHB1_GRP1_ReleaseReset(LL_AHB1_GRP1_PERIPH_OTGHS);

	// Setup the power switch GPIOs
	{
		LL_GPIO_InitTypeDef init;
		init.Mode = LL_GPIO_MODE_OUTPUT;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Pull = LL_GPIO_PULL_NO;
		init.Pin = LL_GPIO_PIN_2;
		init.Speed = LL_GPIO_SPEED_FREQ_LOW;

		LL_GPIO_Init(GPIOE, &init); // USB_EN

		init.Pin = LL_GPIO_PIN_3;
		init.Mode = LL_GPIO_MODE_INPUT;
		init.Pull = LL_GPIO_PULL_UP;

		LL_GPIO_Init(GPIOE, &init); // USB_FAULT

		init.Pin = LL_GPIO_PIN_14 | LL_GPIO_PIN_15;
		init.Mode = LL_GPIO_MODE_ALTERNATE;
		init.Alternate = LL_GPIO_AF_12; // this isn't in the datasheet but it's in the examples
		init.Pull = LL_GPIO_PULL_NO;
		init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;

		LL_GPIO_Init(GPIOB, &init);
	}

	while (USB_OTG_HS->GRSTCTL & USB_OTG_GRSTCTL_CSRST) {;}

	// Initialize USB
	USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
	USB_OTG_HS->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
	USB_OTG_HS->GUSBCFG |= (0x9 << USB_OTG_GUSBCFG_TRDT_Pos);
	USB_OTG_HS->GUSBCFG &= ~(USB_OTG_GUSBCFG_HNPCAP | USB_OTG_GUSBCFG_SRPCAP);
	USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_PHYLPCS | USB_OTG_GUSBCFG_PHYSEL;

	// Disable DMAmode
	USB_OTG_HS->GAHBCFG &= ~(USB_OTG_GAHBCFG_DMAEN);

	// Clear all interrupts
	USB_OTG_HS->GINTSTS = 0xFFFFFFFF; // technically not compliant but screw it

	// Power UP PHY
	USB_OTG_HS_PCGCCTL = 0;
	USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_PWRDWN;

	// Disable sensing
	USB_OTG_HS->GCCFG &= ~(USB_OTG_GCCFG_VBUSASEN | USB_OTG_GCCFG_VBUSBSEN);
	USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
	// Disable SOF
	USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_SOFOUTEN;

	// Flush all FIFOs
	USB_OTG_HS->GRSTCTL = (15 << USB_OTG_GRSTCTL_TXFNUM_Pos) | USB_OTG_GRSTCTL_TXFFLSH;
	while (USB_OTG_HS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) {;}
	USB_OTG_HS->GRSTCTL |= USB_OTG_GRSTCTL_RXFFLSH;
	while (USB_OTG_HS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) {;}

	// Enable mode mismatch/OTG interrupts
	USB_OTG_HS->GINTMSK = 0;
	USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_MMISM | USB_OTG_GINTMSK_OTGINT;

	// Enable USB IRQ
	NVIC_SetPriority(OTG_HS_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
	NVIC_EnableIRQ(OTG_HS_IRQn);

	// Enable interrupts
	USB_OTG_HS->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

	// We are now inited. To continue host initilaization,
	// the user must call enable();
}

void usb::HostBase::usb_global_irq() {
	// Check what triggered the interrupt
	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_HPRTINT) {
		// Host port interrupt
		if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PENCHNG) {
			// Port enable status changed
			got_penchng = 1;
			USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PENCHNG;
		}
		else if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PCDET) {
			// Port connection detected, for now just a no-op.
			USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PCDET; // clear
		}
	}
}

