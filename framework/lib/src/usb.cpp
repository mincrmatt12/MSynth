#include <algorithm>
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

// PIPE STATE INFO
//
// pipe_xfer_buffers is the main location for info about ptrs:
// 	if 0 - unallocated
// 	if 0x 0000 1Fxx - state xx
// 	else in progress to buffer at 0x xxxx xxxx
//
// STATE ON HOST:
// 	allocated -> enabled -> transferring -> finished -|-> deallocated
// 	      \___________________________________________/

usb::pipe_t usb::HostBase::allocate_pipe() {
	for (pipe_t i = 0; i < 8; ++i) {
		if (pipe_xfer_buffers[i] == 0) {
			// allocate this pipe
			pipe_xfer_buffers[i] = (void *)(0x1F00 | (int)transaction_status::Inactive);
			pipe_callbacks[i].target = nullptr;
			// disable all interrupts
			USB_OTG_HS_HC(i)->HCINT = 0xff; // Kill off interrupts, an enable HAINT
			USB_OTG_HS_HOST->HAINT |= (1 << i); // Stop interrupt
			USB_OTG_HS_HOST->HAINTMSK |= (1 << i); // Enable interrupt channel

			return i;
		}
	}

	return pipe::Busy;
}

usb::transaction_status usb::HostBase::check_xfer_state(pipe_t i) {
	if (pipe_xfer_buffers[i] == 0) return transaction_status::NotAllocated;
	if ((unsigned int)pipe_xfer_buffers[i] & ~(0x1F00U)) return transaction_status::Busy;
	else return (transaction_status)((unsigned int)pipe_xfer_buffers[i] & 0xFF);
}

usb::transaction_status usb::HostBase::submit_xfer(pipe_t idx, uint16_t length, void * buffer, const pipe::Callback &cb, bool is_setup) {
	if (check_xfer_state(idx) == transaction_status::Busy) {
		return transaction_status::Busy;
	}
	pipe_callbacks[idx] = cb;
	return submit_xfer(idx, length, buffer, is_setup);
}

void usb::HostBase::destroy_pipe(pipe_t idx) {
	// Check if the channel is active ATM
	
	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_CHENA) {
		// Set an interrupt MSK
		USB_OTG_HS_HC(idx)->HCINTMSK |= USB_OTG_HCINTMSK_CHHM;
		// Set the channel as disabled
		USB_OTG_HS_HC(idx)->HCCHAR |= USB_OTG_HCCHAR_CHDIS;
		// Don't immediately say the channel is ded, but mark the channel as "shutting down"
		pipe_xfer_buffers[idx] = (void *)(0x1F00 | (int)transaction_status::ShuttingDown);
	}
	else {
		// The channel is inactive, end the channel immediately
		pipe_xfer_buffers[idx] = 0;
		// Disable interrupts on this channel
		USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
	}
}

void usb::HostBase::configure_pipe(pipe_t idx, uint8_t address, uint8_t endpoint_num, uint16_t max_pkt_size, pipe::EndpointDirection ed, pipe::EndpointType et, bool data_toggle) {
	// Setup the HCCHAR
	USB_OTG_HS_HC(idx)->HCCHAR = (
		((address << USB_OTG_HCCHAR_DAD_Pos) & USB_OTG_HCCHAR_DAD) |
		((endpoint_num << USB_OTG_HCCHAR_EPNUM_Pos) & USB_OTG_HCCHAR_EPNUM) |
		((max_pkt_size << USB_OTG_HCCHAR_MPSIZ_Pos) & USB_OTG_HCCHAR_MPSIZ) |
		(((unsigned)ed << USB_OTG_HCCHAR_EPDIR_Pos) & USB_OTG_HCCHAR_EPDIR) |
		(((unsigned)et << USB_OTG_HCCHAR_EPTYP_Pos) & USB_OTG_HCCHAR_EPTYP)
	);
	
	// TODO: setup the LSDEV
	
	// Set the data toggle on this ENDPOINT.
	// To get the data toggle later, the application can use get_pipe_data_toggle().
	// This should be maintained across endpoint+direction changes.
	
	if (data_toggle) data_toggles |= (1 << idx);
	else 			 data_toggles &= ~(1 << idx);
}

bool usb::HostBase::get_pipe_data_toggle(pipe_t idx) {
	return data_toggles & (1 << idx);
}

usb::transaction_status usb::HostBase::submit_xfer(pipe_t idx, uint16_t length, void * buffer, bool is_setup) {
	if (check_xfer_state(idx) == transaction_status::Busy) return transaction_status::Busy;

	// Come up with the value for PID
	
	uint8_t dpid = 0;
	bool is_periodic = false;

	if (is_setup) {
		// Set dpid to 0b11 - MDATA/SETUP
		dpid = 0b11;
	}
	else {
		// Check which type of transfer we are using
		switch (((pipe::EndpointType)(USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPTYP) >> USB_OTG_HCCHAR_EPTYP_Pos)) {
			case pipe::EndpointTypeControl:
				// If setup stage, we won't get here
				//
				// Otherwise, we are in DATA or STATUS phase, and both start with DATA1
				if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR) {
					// IN
					dpid = 0b10; // DATA1
					break;
				}
				else {
					// OUT
					// STATUS OUT sets toggle to 1
					if (length == 0) {
						data_toggles |= (1 << idx);
						dpid = 0b10;
					}
					else {
						dpid = (data_toggles & (1 << idx)) ? 0b10 : 0; // DATA1, DATA 0 based on toggle
					}
					break;
				}
			case pipe::EndpointTypeIso:
				is_periodic = true;
				// Start with DATA0
				dpid = 0b00; // DATA0
				break;
			case pipe::EndpointTypeInterrupt:
				is_periodic = true;
			case pipe::EndpointTypeBulk:
				// Check the data toggle
				dpid = (data_toggles & (1 << idx)) ? 0b10 : 0;
				break;
		}
	}

	// Compute the number of packets
	
	uint16_t mps = (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_MPSIZ) >> USB_OTG_HCCHAR_MPSIZ_Pos;
	uint16_t pkts = 0;
	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR && length % mps != 0) {
		length += mps - (length % mps);
		pkts = length / mps;
	}
	else if (length % mps != 0) {
		pkts = (length / mps) + 1;
	}
	else pkts = length / mps;

	// Setup the transfer
	
	this->pipe_xfer_buffers[idx] = buffer;
	USB_OTG_HS_HC(idx)->HCTSIZ = (dpid << USB_OTG_HCTSIZ_DPID_Pos) |
		((pkts << USB_OTG_HCTSIZ_PKTCNT_Pos) & USB_OTG_HCTSIZ_PKTCNT) |
		((length << USB_OTG_HCTSIZ_XFRSIZ_Pos) & USB_OTG_HCTSIZ_XFRSIZ);
	
	// Setup interrupts
	
	USB_OTG_HS_HOST->HAINT |= (1 << idx);
	USB_OTG_HS_HC(idx)->HCINTMSK = 0xff; // enable all interrupts

	// Begin transferring
	
	USB_OTG_HS_HC(idx)->HCCHAR |= USB_OTG_HCCHAR_CHENA; // BLAST

	// Fill up TX fifo if possible.
	
	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR) {
		// IN return now
		return transaction_status::InProgress;
	}

	// Otherwise, begin filling up
	uint16_t next_packet_size = mps / 4;
	if (length < mps) next_packet_size = (length + 3) / 4;

	while (is_periodic ? (USB_OTG_HS_HOST->HPTXSTS & 0xFFFF) : (USB_OTG_HS->HNPTXSTS & 0xFFF) >= next_packet_size) {
		// Load next_packet_size into the DFIFO
		for (int i = 0; i < next_packet_size; ++i) {
			length -= 4;
			USB_OTG_HS_DFIFO(idx) = *((uint32_t *)this->pipe_xfer_buffers[idx]);
			this->pipe_xfer_buffers[idx] = (uint32_t *)(this->pipe_xfer_buffers) + 1;
		}

		next_packet_size = mps / 4;
		if (length < mps) next_packet_size = (length + 3) / 4;
	}

	return transaction_status::InProgress;
}
