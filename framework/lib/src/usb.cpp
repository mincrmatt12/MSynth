#include <algorithm>
#include <usb.h>

// This file contains various device types.
// as well as HostBase

void usb::HostBase::enable() {
	// Enable the HPRT interrupts
	USB_OTG_HS->GINTMSK |= USB_OTG_GINTMSK_PRTIM | USB_OTG_GINTMSK_SOFM;
	// Set FS
	USB_OTG_HS_HOST->HCFG = 0b101; // Set 48mhz FS (will adjust)

	// Turn on port power
	LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2); // dwrrrrrrrrrn
	USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PPWR;
}

void usb::HostBase::disable() {
	// Check the state of the USB
	if (inserted()) {
		// Device was inserted, so let's disable the port
		USB_OTG_HS_HPRT0 |= USB_OTG_HPRT_PENA;

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
	//puts("chspeD");
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

		//USB_OTG_HS_HOST->HFIR = 47999U;
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
	//printf("pena: %d\n", USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PENA);

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
	// puts("bulk");
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
	NVIC_SetPriority(OTG_HS_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 8, 0)); // USB has low priority on the msynth since it can call delay.
	NVIC_EnableIRQ(OTG_HS_IRQn);

	// Enable interrupts
	USB_OTG_HS->GAHBCFG |= USB_OTG_GAHBCFG_GINT; // Setup for half-empty interrupts.

	// We are now inited. To continue host initilaization,
	// the user must call enable();
}

// PIPE STATE INFO
//
// pipe_xfer_buffers contains buffer pointers
// pipe_statuses contains the state of each channel
// pipe_repeat_counts contains the retry counter for each transfer
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
		if (pipe_statuses[i] == transaction_status::NotAllocated) {
			// allocate this pipe
			pipe_statuses[i] = transaction_status::Inactive;
			pipe_callbacks[i].target = nullptr;
			pipe_repeat_counts[i] = 0;
			set_retry_behavior(i, retry_behavior::Instantaneous);
			// disable all interrupts
			USB_OTG_HS_HC(i)->HCINT = 0xff; // Kill off interrupts, an enable HAINT
			USB_OTG_HS_HOST->HAINT |= (1 << i); // Stop interrupt
			USB_OTG_HS_HOST->HAINTMSK |= (1 << i); // Enable interrupt channel

			//printf("alloc %d\n", i);

			return i;
		}
	}

	return pipe::Busy;
}

// HELPER ROUTINES
inline void enable_channel(usb::pipe_t idx) {
	auto tmp = USB_OTG_HS_HC(idx)->HCCHAR;
	tmp &= ~USB_OTG_HCCHAR_CHDIS;
	USB_OTG_HS_HC(idx)->HCCHAR = tmp | USB_OTG_HCCHAR_CHENA; // BLAST
}

inline void disable_channel(usb::pipe_t idx) {
	USB_OTG_HS_HC(idx)->HCCHAR |= USB_OTG_HCCHAR_CHENA | USB_OTG_HCCHAR_CHDIS; // BLAST
}

inline uint32_t available_fifo_space(usb::pipe_t idx) {
	bool is_periodic = ((USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPTYP) >> USB_OTG_HCCHAR_EPTYP_Pos) & 0x1;
	return is_periodic ? (USB_OTG_HS_HOST->HPTXSTS & 0xFFFF) : (USB_OTG_HS->HNPTXSTS & 0xFFFF);
}

inline uint32_t available_request_space(usb::pipe_t idx) {
	bool is_periodic = ((USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPTYP) >> USB_OTG_HCCHAR_EPTYP_Pos) & 0x1;
	return is_periodic ? (USB_OTG_HS_HOST->HPTXSTS & USB_OTG_HPTXSTS_PTXQSAV) : (USB_OTG_HS->HNPTXSTS & USB_OTG_GNPTXSTS_NPTQXSAV);
}

void usb::HostBase::blast_next_packet(usb::pipe_t idx) {
	auto tmp = USB_OTG_HS_HC(idx)->HCCHAR;
	int mps = (tmp & USB_OTG_HCCHAR_MPSIZ) >> USB_OTG_HCCHAR_MPSIZ_Pos;
	tmp = USB_OTG_HS_HC(idx)->HCTSIZ;
	int length = (tmp & USB_OTG_HCTSIZ_XFRSIZ) >> USB_OTG_HCTSIZ_XFRSIZ_Pos;
	//printf("l %d; mps %d\n", length, mps);
	enable_channel(idx);
	if (!length) {
		data_toggles ^= (1 << idx);
	}
	else {
		int pktremain = (tmp & USB_OTG_HCTSIZ_PKTCNT) >> USB_OTG_HCTSIZ_PKTCNT_Pos;
		int next_packet_word_count = (pktremain != 1) ? mps / 4 : (length % mps) / 4;
		if (pktremain == 1 && next_packet_word_count == 0 && !(pipe_zlp_enable & (1 << idx))) next_packet_word_count = mps / 4;
		//printf("pktremain %d\n", pktremain);
		//printf("zlp %d\n", pipe_zlp_enable);
		for (int word = 0; word < next_packet_word_count; ++word) {
			//puts("load");
			USB_OTG_HS_DFIFO(idx) = *((uint32_t *)this->pipe_xfer_buffers[idx]);
			this->pipe_xfer_buffers[idx] = (void *)((uint32_t)(this->pipe_xfer_buffers[idx]) + 4);
		}
		this->pipe_xfer_rx_amounts[idx] = next_packet_word_count;
		data_toggles ^= (1 << idx);
	}
}

usb::transaction_status usb::HostBase::check_xfer_state(pipe_t i) {
	if (USB_OTG_HS_HC(i)->HCCHAR & USB_OTG_HCCHAR_CHENA) return transaction_status::InProgress;
	return pipe_statuses[i];
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
		USB_OTG_HS_HC(idx)->HCCHAR |= USB_OTG_HCCHAR_CHDIS | USB_OTG_HCCHAR_CHENA; // see RM
		// Don't immediately say the channel is ded, but mark the channel as "shutting down"
		pipe_statuses[idx] = transaction_status::ShuttingDown;
	}
	else {
		// The channel is inactive, end the channel immediately
		pipe_statuses[idx] = transaction_status::NotAllocated;
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
		((unsigned(ed) << USB_OTG_HCCHAR_EPDIR_Pos) & USB_OTG_HCCHAR_EPDIR) |
		((unsigned(et) << USB_OTG_HCCHAR_EPTYP_Pos) & USB_OTG_HCCHAR_EPTYP)
	);

	if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PSPD_1) USB_OTG_HS_HC(idx)->HCCHAR |=  USB_OTG_HCCHAR_LSDEV;
	else                                        USB_OTG_HS_HC(idx)->HCCHAR &= ~USB_OTG_HCCHAR_LSDEV;
	
	// Set the data toggle on this ENDPOINT.
	// To get the data toggle later, the application can use get_pipe_data_toggle().
	// This should be maintained across endpoint+direction changes.
	
	if (data_toggle) data_toggles |= (1 << idx);
	else 			 data_toggles &= ~(1 << idx);

	// Set the state back to inactive
	this->pipe_statuses[idx] = transaction_status::Inactive;
}

bool usb::HostBase::get_pipe_data_toggle(pipe_t idx) {
	return data_toggles & (1 << idx);
}

usb::transaction_status usb::HostBase::submit_xfer(pipe_t idx, uint16_t length, void * buffer, bool is_setup) {
	//puts("submit xfer");
	if (check_xfer_state(idx) == transaction_status::Busy) return transaction_status::Busy;
	//puts("sub");

	// Come up with the value for PID
	
	uint8_t dpid = 0;
	bool is_periodic = false;

	if (is_setup) {
		// Set dpid to 0b11 - MDATA/SETUP
		dpid = 0b11;
		pipe_zlp_enable &= ~(1 << idx);
	}
	else {
		// Check which type of transfer we are using
		switch (((pipe::EndpointType)((USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPTYP) >> USB_OTG_HCCHAR_EPTYP_Pos))) {
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
						pipe_zlp_enable &= ~(1 << idx);
					}
					break;
				}
			case pipe::EndpointTypeIso:
				pipe_zlp_enable &= ~(1 << idx);
				is_periodic = true;
				// Start with DATA0
				dpid = 0b00; // DATA0
				break;
			case pipe::EndpointTypeInterrupt:
				is_periodic = true;
			case pipe::EndpointTypeBulk:
				// Check the data toggle
				dpid = (data_toggles & (1 << idx)) ? 0b10 : 0;
				pipe_zlp_enable |= (1 << idx);
				break;
		}
	}

	// Compute the number of packets
	
	uint16_t mps = (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_MPSIZ) >> USB_OTG_HCCHAR_MPSIZ_Pos;
	uint16_t pkts = 0;
	if (length) {
		if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR && length % mps != 0) {
			length += mps - (length % mps);
			pkts = length / mps;
		}
		else if (length % mps != 0) {
			pkts = (length / mps) + 1;
		}
		else pkts = length / mps;
	}
	else {
		pipe_zlp_enable |= (1 << idx);
		pkts = 1;
	}

	// Setup the transfer
	this->pipe_xfer_buffers[idx] = buffer;
	USB_OTG_HS_HC(idx)->HCTSIZ = (dpid << USB_OTG_HCTSIZ_DPID_Pos) |
		((pkts << USB_OTG_HCTSIZ_PKTCNT_Pos) & USB_OTG_HCTSIZ_PKTCNT) |
		((length << USB_OTG_HCTSIZ_XFRSIZ_Pos) & USB_OTG_HCTSIZ_XFRSIZ);
	
	// Setup interrupts
	USB_OTG_HS_HOST->HAINT |= (1 << idx);
	USB_OTG_HS_HOST->HAINTMSK |= (1 << idx); // Enable interrupt channel
	USB_OTG_HS_HC(idx)->HCINT = 0b11111111011; // clear all interrupts
	USB_OTG_HS_HC(idx)->HCINTMSK = 0b11111111011; // enable all interrupts

	// Set state flags
	this->pipe_statuses[idx] = transaction_status::InProgress;

	//printf("%d %d\n", idx, USB_OTG_HS_HC(idx)->HCCHAR);

	if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR) {
		// Begin transferring
		
		//puts("rxstart");
		this->pipe_xfer_rx_amounts[idx] = 0;

		enable_channel(idx);
		// IN return now
		return transaction_status::InProgress;
	}

	// Otherwise, try to load the first packet.
	// Is this a ZLP?
	
	if (!length) {
		// Simply enable the channel.
		this->pipe_xfer_rx_amounts[idx] = 0; 

		data_toggles ^= (1 << idx);
		enable_channel(idx);
	}
	else {
		// Load packets while able, and defer to the space available interrupts
		if (available_fifo_space(idx) && available_request_space(idx)) {
			//puts("blast");
			blast_next_packet(idx);
		}
		else {
			//puts("oops");
			// Enable the TXFIFO interrupt
			USB_OTG_HS->GINTMSK |= is_periodic ? USB_OTG_GINTMSK_PTXFEM : USB_OTG_GINTMSK_NPTXFEM;
		}
	}

	return transaction_status::InProgress;
}

// USB HELPER INTERRUPT SUBROUTINES
//
// This is structured similarly (but not exactly) like the official one
void usb::HostBase::channel_irq_in(usb::pipe_t idx) {
	// Interrupts:
	//
	// XRFC will occur when all transfers are completed succesfully. ACK will occur at the same time, because ACK is the only ok status code
	// CHH  will occur when a channel is halted.
	//
	// NAK will occur when there is a NAK. When we NAK we have to retry the transmission up to 3 times. In this case we
	// don't disable the channel because then the NAK would show through to the end-user and it's unnecesarry. What we do instead is re-enable the channel to send another
	// token packet.

	if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_XFRC) {
		//puts("xrfc");
		// The transfer has completed OK. Due to the order of interrupt handling, this will occur after the relevant data is read off of the fifo.
		//
		// We clear the interrupt, and then finish the transfer normally

		USB_OTG_HS_HC(idx)->HCINT |= (USB_OTG_HCINT_XFRC | USB_OTG_HCINT_ACK); // Also clear ACK since it will get set at the same time.
		this->pipe_statuses[idx] = transaction_status::Ack;

		// Call the callback if set

		this->pipe_callbacks[idx](idx, transaction_status::Ack);
		// Destroy the pipe callback
		new (&this->pipe_callbacks[idx]) pipe::Callback{};

		// Ignore interrupts from this channel as well

		USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
		
		// Apparently we're supposed to disable the channel here as well?
		// I don't think so...
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_ACK) {
		//puts("ack");
		// A packet was succesfully received, but there is more to receive. This case is already handled during the rxflvl interrupt.
		// We simply clear the flag and continue on.
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_ACK;
		USB_OTG_HS_HC(idx)->HCINTMSK = 0b11111111011; // enable all interrupts
		// Keep it going
		enable_channel(idx); // Continue reading the next packet (sends a new IN token)
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_NAK) {
		//puts("nak");
		// Ack the interrupt first
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_NAK;
		// The device is not ready to send data yet, but is open for more attempts. Check if we have exceeded the retry counter.
		if (++this->pipe_repeat_counts[idx] < 3) {
			//puts("nakr");
			// We are allowed to send more requests, so send another token
			if (pipe_nak_defer & (1 << idx)) {
				pipe_nak_defer_waiting |= (1 << idx);
			}
			else {
				if (pipe_nak_delay & (1 << idx)) util::delay(2);
				enable_channel(idx);
			}
		}
		else {
			//puts("nakc");
			// We are unable to send more requests, cancel and halt channel
			this->pipe_statuses[idx] = transaction_status::Nak;
			disable_channel(idx);
		}
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_STALL) {
		//puts("stall");
		// Stall cancels immediately, so halt and set status
		// Same behavior for the next few errors.
		this->pipe_statuses[idx] = transaction_status::Stall;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_STALL;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_BBERR) { // babble balbbalb balbab bbab bib bab boop
		this->pipe_statuses[idx] = transaction_status::XferError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_BBERR;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_TXERR) { // txn error
		this->pipe_statuses[idx] = transaction_status::XferError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_TXERR;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_DTERR) { // data toggle erro
		this->pipe_statuses[idx] = transaction_status::DataToggleError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_DTERR;
		disable_channel(idx);
	}

	// Now check for CHH _after_ everything else (to ensure the state is set first.)
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_CHH) {
		//puts("chh");
		// Acknowledge the interrupt
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_CHH;
		USB_OTG_HS_HC(idx)->HCCHAR &= ~USB_OTG_HCCHAR_CHDIS;

		// What state were we in? Are we trying to deallocate this channel from earlier
		if (this->pipe_statuses[idx] == transaction_status::ShuttingDown) {
			// Finish shutting down the channel
			pipe_statuses[idx] = transaction_status::NotAllocated;
			// Disable interrupts on this channel
			USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
		}
		else {
			// If there (somehow) isn't a valid excuse for stopping, set one
			if (this->pipe_statuses[idx] == transaction_status::InProgress) this->pipe_statuses[idx] = transaction_status::XferError;
			this->pipe_callbacks[idx](idx, this->pipe_statuses[idx]);
			// Destroy the pipe callback
			new (&this->pipe_callbacks[idx]) pipe::Callback{};
		}

		USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
	}
}

void usb::HostBase::channel_irq_out(usb::pipe_t idx) {
	// Interrupts:
	//
	// XRFC will occur when all transfers are completed succesfully. ACK will occur at the same time, because ACK is the only ok status code
	// ACK will occur on succesful transmission of a packet. If there are more (and there are since XRFC clears ack) we send out another packet.
	//
	// NAK will occur on failure to send a packet. We therefore either rewind the pointer and try again or defer to error
	// Other errors are handled as they are in input packets.

	if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_XFRC) {
		// The transfer has completed OK. Due to the order of interrupt handling, this will occur after the relevant data is read off of the fifo.
		//
		// We clear the interrupt, and then finish the transfer normally

		USB_OTG_HS_HC(idx)->HCINT |= (USB_OTG_HCINT_XFRC | USB_OTG_HCINT_ACK); // Also clear ACK since it will get set at the same time.
		this->pipe_statuses[idx] = transaction_status::Ack;
		this->pipe_repeat_counts[idx] = 0;

		// Call the callback if set

		this->pipe_callbacks[idx](idx, transaction_status::Ack);
		// Destroy the pipe callback
		new (&this->pipe_callbacks[idx]) pipe::Callback{};

		// Ignore interrupts from this channel as well

		USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
		
		// Update data toggle
		this->data_toggles ^= (1 << idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_ACK) {
		// A packet was succesfully received, but there is more to send. Blast out another packet
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_ACK;
		// Reset error count
		this->pipe_repeat_counts[idx] = 0;
		blast_next_packet(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_NAK) {
		// Ack the interrupt first
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_NAK;
		// The device is not ready to get data yet, but is open for more attempts. Check if we have exceeded the retry counter.
		if (++this->pipe_repeat_counts[idx] < 3) {
			this->pipe_statuses[idx] = transaction_status::InProgress;
			disable_channel(idx);
		}
		else {
			// We are unable to send more requests, cancel and halt channel
			this->pipe_statuses[idx] = transaction_status::Nak;
			disable_channel(idx);
		}
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_STALL) {
		// Stall cancels immediately, so halt and set status
		// Same behavior for the next few errors.
		this->pipe_statuses[idx] = transaction_status::Stall;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_STALL;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_BBERR) { // babble balbbalb balbab bbab bib bab boop
		this->pipe_statuses[idx] = transaction_status::XferError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_BBERR;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_TXERR) { // txn error
		this->pipe_statuses[idx] = transaction_status::XferError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_TXERR;
		disable_channel(idx);
	}
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_DTERR) { // data toggle erro
		this->pipe_statuses[idx] = transaction_status::DataToggleError;
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_DTERR;
		disable_channel(idx);
	}

	// Now check for CHH _after_ everything else (to ensure the state is set first.)
	else if (USB_OTG_HS_HC(idx)->HCINT & USB_OTG_HCINT_CHH) {
		// Acknowledge the interrupt
		USB_OTG_HS_HC(idx)->HCINT |= USB_OTG_HCINT_CHH;
		USB_OTG_HS_HC(idx)->HCCHAR &= ~USB_OTG_HCCHAR_CHDIS;

		// What state were we in? Are we trying to deallocate this channel from earlier
		if (this->pipe_statuses[idx] == transaction_status::ShuttingDown) {
			// Finish shutting down the channel
			pipe_statuses[idx] = transaction_status::NotAllocated;
			// Disable interrupts on this channel
			USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
		}
		else {
			// If the excuse for stopping wasn't set, then we assume that we're coming from a NAK and should restart the channel.
			if (this->pipe_statuses[idx] == transaction_status::InProgress) {
				// Try to re-do it.

				// Update the DPID (setup case is forbidden from sending a NAK)
				if (data_toggles & (1 << idx)) USB_OTG_HS_HC(idx)->HCTSIZ |= USB_OTG_HCTSIZ_DPID_1;
				else                           USB_OTG_HS_HC(idx)->HCTSIZ &= ~USB_OTG_HCTSIZ_DPID_1;

				// Rewind the buffer ptr
				this->pipe_xfer_buffers[idx] = (void *)((uint32_t)(this->pipe_xfer_buffers[idx]) - this->pipe_xfer_rx_amounts[idx] * 4);

				if (pipe_nak_delay & (1 << idx)) util::delay(2);
				if (pipe_nak_defer & (1 << idx)) {
					pipe_nak_defer_waiting |= (1 << idx);
				}
				else blast_next_packet(idx);
			}
			else {
				this->pipe_callbacks[idx](idx, this->pipe_statuses[idx]);
				// Destroy the pipe callback
				new (&this->pipe_callbacks[idx]) pipe::Callback{};
			}
		}

		USB_OTG_HS_HOST->HAINTMSK &= ~(1 << idx);
	}
}

// USB GLOBAL INTERRUPT
// This routine has a lot of responsibilities, such as managing port power status, and dealing with ongoing xfers.
void usb::HostBase::usb_global_irq() {
	// Check what triggered the interrupt
	
	// Was it a port interrupt?
	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_HPRTINT) {
		auto hprt = USB_OTG_HS_HPRT0;
		hprt &= ~USB_OTG_HPRT_PENA;
		////puts("hprt");
		// Host port interrupt
		if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PENCHNG) {
			// Port enable status changed
			got_penchng = 1;
			////puts("penchng");
			////printf("pena: %d\n", USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PENA);
			hprt |= USB_OTG_HPRT_PENCHNG;
		}
		else if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PCDET) {
			// Port connection detected, for now just a no-op.
			hprt |= USB_OTG_HPRT_PCDET; // clear
			//puts("pcdet");
		}
		else if (USB_OTG_HS_HPRT0 & USB_OTG_HPRT_POCCHNG) {
			// Port overcurrent detected, already handled
			hprt |= USB_OTG_HPRT_POCCHNG;
			//puts("pocchng");
		}

		USB_OTG_HS_HPRT0 = hprt;
	}

	if ((USB_OTG_HS->GINTSTS & USB_OTG_HS->GINTMSK) & (USB_OTG_GINTSTS_PTXFE | USB_OTG_GINTSTS_NPTXFE)) {
		// Disable these interrupts
		USB_OTG_HS->GINTMSK &= ~(USB_OTG_GINTSTS_PTXFE | USB_OTG_GINTSTS_NPTXFE);
		//puts("txflvl");
		// Check all busy pipes.
		for (pipe_t idx = 0; idx < 8; ++idx) {
			if (check_xfer_state(idx) == transaction_status::InProgress && !(USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPTYP) && !(USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_CHENA)) {
				// This is an OUT transfer which is still in progress (not errored, otherwise it would read nak / etc.), and is not enabled.
				//
				// Either:
				// 	- The transfer is done and we haven't realized it yet
				//printf("todo out irq %d\n", idx);
				// TODO TODO TODO
			}
		}
	}

	// Was it a RX ~~FLEGHM~~ FLEVL interrupt?
	while (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_RXFLVL) {
		//puts("rxflvl");
		// Disable this interrupt temporarily
		USB_OTG_HS->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;
		// Check what has happened
		uint32_t tmp = USB_OTG_HS->GRXSTSP; // Pops the top thing off of the RXfifo / RXqueue

		// Have we got a fresh off the proverbial oven IN pkt?
		if ((tmp & USB_OTG_GRXSTSP_PKTSTS) == (2 << USB_OTG_GRXSTSP_PKTSTS_Pos)) {
			// Where should it be sent?

			pipe_t place = tmp & USB_OTG_GRXSTSP_EPNUM; // no shift at end of reg
			if (place > 8) goto end;
			//printf("got rx pkt %d\n", place);
			uint16_t count = (tmp & USB_OTG_GRXSTSP_BCNT) >> USB_OTG_GRXSTSP_BCNT_Pos;

			this->pipe_xfer_rx_amounts[place] += count;

			// Read that many words (+ some extra padding) into the target buffer.

			if (count % 4 != 0) count += 4 - count % 4;
			for (int i = 0; i < (count / 4); ++i) {
				//printf("rxfifo @ %p\n", this->pipe_xfer_buffers[place]);
				*((uint32_t *)this->pipe_xfer_buffers[place]) = USB_OTG_HS_DFIFO(place);
				//printf("rxfifo = %u\n", *((uint32_t *)this->pipe_xfer_buffers[place]));
				this->pipe_xfer_buffers[place] = ((uint32_t *)(this->pipe_xfer_buffers[place])) + 1;
			}
			
			// Update dtoggle
			this->data_toggles ^= (1 << place);

			// Reset error count
			this->pipe_repeat_counts[place] = 0;
		}

end:
		// Unmask
		USB_OTG_HS->GINTSTS |= (USB_OTG_GINTSTS_RXFLVL);
	}
	
	// Was it a channel interrupt?
	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_HCINT) {
		// Scroll through all channels to check which one it was
		for (pipe_t i = 0; i < 12; ++i) {
			if (!(USB_OTG_HS_HOST->HAINT & (1 << i))) continue;
			if (i > 8) { /* ignore interrupt */ USB_OTG_HS_HC(i)->HCINT = 0xff; }

			// Is it an in/out
			if (USB_OTG_HS_HC(i)->HCCHAR & USB_OTG_HCCHAR_EPDIR) channel_irq_in(i);
			else 												 channel_irq_out(i);
		}
	}

	if (USB_OTG_HS->GINTSTS & USB_OTG_GINTSTS_SOF) {
		USB_OTG_HS->GINTSTS |= USB_OTG_GINTSTS_SOF;

		// Scan through all active channels
		for (pipe_t idx = 0; idx < 8; ++idx) {
			if (pipe_nak_defer_waiting & (1 << idx)) {
				pipe_nak_defer_waiting &= ~(1 << idx);
				// check what we have to do
				if (USB_OTG_HS_HC(idx)->HCCHAR & USB_OTG_HCCHAR_EPDIR) {
					// IN, enable channel
					enable_channel(idx);
				}
				else {
					// Rewinding handled earlier
					blast_next_packet(idx);
				}
			}
		}
	}
}
