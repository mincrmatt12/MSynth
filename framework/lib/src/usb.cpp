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

	// Disable all interrupts
	USB_OTG_HS->GINTMSK = USB_OTG_GINTMSK_MMISM;
	USB_OTG_HS_HOST->HAINTMSK = 0;

	// Destroy all channels
	for (int i = 0; i < 8; ++i) {
		if (USB_OTG_HS_HC(i)->HCCHAR & USB_OTG_HCCHAR_CHENA) USB_OTG_HS_HC(i)->HCCHAR |= USB_OTG_HCCHAR_CHDIS;
		USB_OTG_HS_HC(i)->HCINT = 0xff;
		USB_OTG_HS_HC(i)->HCINTMSK = 0;
		this->pipe_xfer_buffers[i] = 0;
	}

	// Deplete all FIFOs
	USB_OTG_HS->GRSTCTL = (15 << USB_OTG_GRSTCTL_TXFNUM_Pos) | USB_OTG_GRSTCTL_TXFFLSH;
	while (USB_OTG_HS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) {;}
	USB_OTG_HS->GRSTCTL |= USB_OTG_GRSTCTL_RXFFLSH;
	while (USB_OTG_HS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) {;}

	// Wait a bit
	util::delay(5);

	// Yay!
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
	// 	- 0x40 (64 word) TX periodic fifo

	USB_OTG_HS->GRXFSIZ = 0x100;
	USB_OTG_HS->DIEPTXF0_HNPTXFSIZ = (0x80 << 16) | 0x100 /* start address */;
	USB_OTG_HS->HPTXFSIZ = (0x40 << 16) | 0x180;

	// Setup channel interrupt
	USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_HCIM | USB_OTG_GINTMSK_RXFLVLM;

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
	USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_MMISM;

	// Enable USB IRQ
	NVIC_SetPriority(OTG_HS_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 2, 0));
	NVIC_EnableIRQ(OTG_HS_IRQn);

	// Enable interrupts
	USB_OTG_HS->GAHBCFG |= USB_OTG_GAHBCFG_GINT; // Setup for half-empty interrupts.

	// We are now inited. To continue host initilaization,
	// the user must call enable();
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
//
// USAGE OF HOST REGISTERS
// 
// We place all channel _configuration_ into the HCCHAR registers. They are also
// used as the general-purpose storage for these parameters. For example, the endpoint type isn't
// stored in the HostBase class, rather it is read back from HCCHAR_EPTYP.
//
// Similarly, most transfer parameters are stored in HCTSIZ. Unlike the channel params, however, most of
// these parameters are duplicated in the HostBase.

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
	if ((unsigned int)pipe_xfer_buffers[i] & ~(0x1FFFU)) return transaction_status::Busy;
	else return (transaction_status)((unsigned int)pipe_xfer_buffers[i] & 0xFF);
}

usb::transaction_status usb::HostBase::submit_xfer(pipe_t idx, uint16_t length, void * buffer, const pipe::Callback &cb, bool is_setup) {
	if (check_xfer_state(idx) == transaction_status::Busy) {
		return transaction_status::Busy;
	}
	pipe_callbacks[idx] = cb;
	return submit_xfer(idx, length, buffer, is_setup);
}

#define STATE_TO_BUF(v) (void *)(0x1F00 | (int)(v))

void usb::HostBase::destroy_pipe(pipe_t idx) {
	// Check if the channel is active ATM
	
	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_CHENA) {
		// Set an interrupt MSK
		USB_OTG_HS_HC(idx)->HCINTMSK |= USB_OTG_HCINTMSK_CHHM;
		// Set the channel as disabled
		USB_OTG_HS_HC(idx)->HCCHAR |= USB_OTG_HCCHAR_CHDIS;
		// Don't immediately say the channel is ded, but mark the channel as "shutting down"
		pipe_xfer_buffers[idx] = STATE_TO_BUF(transaction_status::ShuttingDown);
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

	if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PSPD_1) USB_OTG_HS_HC(idx)->HCCHAR |=  USB_OTG_HCCHAR_LSDEV;
	else                                        USB_OTG_HS_HC(idx)->HCCHAR &= ~USB_OTG_HCCHAR_LSDEV;
	
	// Set the data toggle on this ENDPOINT.
	// To get the data toggle later, the application can use get_pipe_data_toggle().
	// This should be maintained across endpoint+direction changes.
	
	if (data_toggle) data_toggles |= (1 << idx);
	else 			 data_toggles &= ~(1 << idx);

	// Set the state back to inactive
	this->pipe_xfer_buffers[idx] = STATE_TO_BUF(transaction_status::Inactive);
}

bool usb::HostBase::get_pipe_data_toggle(pipe_t idx) {
	return data_toggles & (1 << idx);
}

usb::transaction_status usb::HostBase::submit_xfer(pipe_t idx, uint16_t length, void * buffer, bool is_setup) {
	puts("submit xfer");
	if (check_xfer_state(idx) == transaction_status::Busy) return transaction_status::Busy;
	puts("sub");

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
	USB_OTG_HS_HOST->HAINTMSK |= (1 << idx); // Enable interrupt channel
	USB_OTG_HS_HC(idx)->HCINTMSK = 0b11111111011; // enable all interrupts

	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR) {
		// Begin transferring
		
		this->pipe_xfer_rx_amounts[idx] = 0;
		auto tmp = USB_OTG_HS_HC(idx)->HCCHAR;
		tmp &= ~USB_OTG_HCCHAR_CHDIS;
		USB_OTG_HS_HC(idx)->HCCHAR = tmp | USB_OTG_HCCHAR_CHENA; // BLAST
		// IN return now
		return transaction_status::InProgress;
	}

	// Otherwise, begin filling up the DFIFO
	uint16_t next_packet_size = mps / 4;
	if (length < mps) next_packet_size = (length + 3) / 4;

	this->pipe_xfer_rx_amounts[idx] = length; // remaining length. This overwrites the previous value but whatever
	auto tmp = USB_OTG_HS_HC(idx)->HCCHAR;
	tmp &= ~USB_OTG_HCCHAR_CHDIS;
	USB_OTG_HS_HC(idx)->HCCHAR = tmp | USB_OTG_HCCHAR_CHENA; // BLAST

	printf("hcchar chdis %d\n", USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_CHDIS);

	while ((is_periodic ? (USB_OTG_HS_HOST->HPTXSTS & 0xFFFF) : (USB_OTG_HS->HNPTXSTS & 0xFFFF)) >= next_packet_size && length != 0 &&
		   (is_periodic ? (USB_OTG_HS_HOST->HPTXSTS & USB_OTG_HPTXSTS_PTXQSAV) : (USB_OTG_HS->HNPTXSTS & USB_OTG_GNPTXSTS_NPTQXSAV))) {
		// Load next_packet_size into the DFIFO
		for (int i = 0; i < next_packet_size; ++i) {
			USB_OTG_HS_DFIFO(idx) = *((uint32_t *)this->pipe_xfer_buffers[idx]);
			puts("loaded word");
			this->pipe_xfer_buffers[idx] = (uint32_t *)(this->pipe_xfer_buffers[idx]) + 1;
			if (length < 4) {
				length = 0;
				break;
			}
			length -= 4;
			printf("hcchar chdis %d\n", USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_NPTXFE);
			printf("hcchar itx %d\n", USB_OTG_HS_HC(idx)->HCINT);
		}

		next_packet_size = mps / 4;
		if (length < mps) next_packet_size = (length + 3) / 4;
		puts("loaded pkt");

		data_toggles ^= (1 << idx);
	}

	return transaction_status::InProgress;
}

// USB GLOBAL INTERRUPT
//
// This routine has a lot of responsibilities, such as managing port power status, and dealing with ongoing xfers.
void usb::HostBase::usb_global_irq() {
	// Check what triggered the interrupt
	
	// Was it a port interrupt?
	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_HPRTINT) {
		puts("hprt");
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
		else if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_POCCHNG) {
			// Port overcurrent detected, already handled
			USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_POCCHNG;
		}
	}
	// Was it a channel interrupt?
	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_HCINT) {
		// Scroll through all channels to check which one it was
		for (pipe_t i = 0; i < 12; ++i) {
			if (!(USB_OTG_HS_HOST->HAINT & (1 << i))) continue;
			if (i > 8) { /* ignore interrupt */ USB_OTG_HS_HC(i)->HCINT = 0xff; }

			// Otherwise, we check the interrupt type.
			// The system will set a XRFC/state/error flag before it sets a CHH interrupt.
			// Or at least, that's how we treat it. We set the return code of the buffer when
			// we get one, which has the side effect of disabling that channel from the 
			// perspective of the rest of the handler.

			if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_ACK) {
				// The transfer has completed with an ACK status code.
				// Mark it as such in the buffer...
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::Ack);
				// Clear the ACK flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_ACK;
			}
			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_NAK) {
				// The transfer has completed with an NAK status code.
				// Mark it as such in the buffer...
				
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::Nak);
				// Clear the flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_NAK;
			}
			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_STALL) {
				// The transfer has completed with an STALL status code.
				// Mark it as such in the buffer...
				
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::Stall);
				// Clear the flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_STALL;
			}
			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_NYET) {
				// The transfer has completed with an NYET status code.
				// Mark it as such in the buffer...
				
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::Nyet);
				// Clear the flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_NYET;
			}
			// AHBERR will never occur
			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_NYET) {
				// The transfer has completed with an NYET status code.
				// Mark it as such in the buffer...
				
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::Nyet);
				// Clear the flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_NYET;
			}
			else if (USB_OTG_HS_HC(i)->HCINT & (USB_OTG_HCINT_FRMOR | USB_OTG_HCINT_TXERR | USB_OTG_HCINT_BBERR)) {
				// The transfer has encountered an error
				this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::XferError);
				// Clear all flags
				USB_OTG_HS_HC(i)->HCINT |= (USB_OTG_HCINT_FRMOR | USB_OTG_HCINT_TXERR | USB_OTG_HCINT_BBERR);
			}
			
			// Alright, now we can begin checking for XRFC / CHH
			//
			// In theory (based on the if/elif structure and LAWS OF PHYSICS) we should already have the target state
			// ready.
			//
			// If not, we can provide a half decent idea of what happened otherwise. Additionally, if the channel
			// is in shutting-down state, we can finish that process here

			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_XFRC) {
				// The transfer has completed "without errors" -- the state is definitely set rn.
				// Call the transfer callback
				this->pipe_callbacks[i](i, check_xfer_state(i));
				// Destroy the pipe callback
				new (&this->pipe_callbacks[i]) pipe::Callback{};

				// The channel is now disabled correctly, so clear this flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_XFRC;

				// We also no longer need to hear interrupts from this channel, so clear its msk
				USB_OTG_HS_HOST->HAINTMSK &= ~(1 << i);
			}
			else if (USB_OTG_HS_HC(i)->HCINT & USB_OTG_HCINT_CHH) {
				// Check what state we were in beforehand
				switch (check_xfer_state(i)) {
				case transaction_status::ShuttingDown:
					// Finish shutting down the channel.
					pipe_xfer_buffers[i] = 0;
					// Disable interrupts on this channel
					USB_OTG_HS_HOST->HAINTMSK &= ~(1 << i);
					// Unassert disable enable
					USB_OTG_HS_HC(i)->HCCHAR &= ~(USB_OTG_HCCHAR_CHDIS);
					break;
				case transaction_status::Busy:
					this->pipe_xfer_buffers[i] = STATE_TO_BUF(transaction_status::XferError);
				default:
					// The trzansfe6r is in progress, but has failed.
					//
					// Still, the state should be located 
					this->pipe_callbacks[i](i, check_xfer_state(i));
					// Destroy the pipe callback
					new (&this->pipe_callbacks[i]) pipe::Callback{};
					break;
				case transaction_status::NotAllocated:
				case transaction_status::Inactive:
					// Just do nothing
					break;
				}

				// The channel is now disabled correctly, so clear this flag.
				USB_OTG_HS_HC(i)->HCINT |= USB_OTG_HCINT_CHH;

				// We also no longer need to hear interrupts from this channel, so clear its msk
				USB_OTG_HS_HOST->HAINTMSK &= ~(1 << i);
			}
			else {
				// Who knows, so let's clear all interrupts just to be safe
				USB_OTG_HS_HC(i)->HCINT = 0xff;
			}
		}
		// Was it a RX ~~FLEGHM~~ FLEVL interrupt?
		if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_RXFLVL) {
			// Disable this interrupt temporarily
			USB_OTG_HS->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;
			// Check what has happened
			uint32_t tmp = USB_OTG_HS->GRXSTSP; // Pops the top thing off of the RXfifo / RXqueue

			// Have we got a fresh off the proverbial oven IN pkt?
			if ((tmp & USB_OTG_GRXSTSP_PKTSTS) == (2 << USB_OTG_GRXSTSP_PKTSTS_Pos)) {
				// Where should it be sent?

				pipe_t place = tmp & USB_OTG_GRXSTSP_EPNUM; // no shift at end of reg
				uint16_t count = (tmp & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;

				this->pipe_xfer_rx_amounts[place] += count;

				// Read that many words (+ some extra padding) into the target buffer.

				if (count % 4 != 0) count += 4 - count % 4;
				for (int i = 0; i < (count / 4); ++i) {
					*((uint32_t *)this->pipe_xfer_buffers[place]) = USB_OTG_HS_DFIFO(place);
					this->pipe_xfer_buffers[place] = (uint32_t *)(this->pipe_xfer_buffers[place]) + 1;
				}
				
				// Update dtoggle
				this->data_toggles ^= (1 << place);

				// Check if we should read more packets

				if (USB_OTG_HS_HC(place)->HCTSIZ & USB_OTG_HCTSIZ_PKTCNT) {
					// Keep it going
					//
					// TODO: do we need to disable disable?
					USB_OTG_HS_HC(place)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
				}
			}

			// Unmask
			USB_OTG_HS->GINTSTS |= (USB_OTG_GINTSTS_RXFLVL);
			USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
		}
	}
}

bool usb::MidiDevice::handles(uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol) {
	return (bClass == 0x01 /* AUDIO */) && (bSubClass == 0x03 /* MIDISTREAMING */);
}

void usb::MidiDevice::init(uint8_t ep0_mps, uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol, usb::HostBase *hb) {
	puts("MidiDevice is initing");

	// TODO
}
