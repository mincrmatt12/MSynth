#include <usb.h>
#include <stdio.h>

bool usb::MidiDevice::handles(uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol) {
	return (bClass == 0x01 /* AUDIO */) && (bSubClass == 0x03 /* MIDISTREAMING */);
}

// CS_ENDPOINT

bool usb::MidiDevice::init(uint8_t ep0_mps, uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol, uint8_t* config_descriptor, usb::HostBase *hb) {
	this->hb = hb;
	puts("MidiDevice is initing");

	// Our implementation of the USB midi spec is fairly limited, since we only _really_ want to deal with midi keyboards/adapters.
	// Still, it's useful to allow the user to select an endpoint, but that functionality will come later.
	//
	// Right now, the approach will be to find the endpoint associated with midi IN jacks. If there are multiple ones, tough.
	
	const raw::ConfigurationDescriptor& cd = *(raw::ConfigurationDescriptor *)config_descriptor;
	
	int mode = 0;
	int epNum;
	int epMps;

	for (int i = 0; i < cd.wTotalLength; i += ((raw::DescriptorHead *)(config_descriptor + i))->bLength) {
		switch (mode) {
			case 0:
				{
					if (((raw::DescriptorHead *)(config_descriptor + i))->bDescriptorType != raw::DescriptorHead::TypeINTERFACE) continue;
					const raw::InterfaceDescriptor& id = *(raw::InterfaceDescriptor *)(config_descriptor + i);

					if (id.bInterfaceClass == bClass && id.bInterfaceSubClass == bSubClass) {
						mode = 1;
					}
					break;
				}
			// i now points to the interface descriptor. find the relevant endpoints
			case 1:
				{
					if (((raw::DescriptorHead *)(config_descriptor + i))->bDescriptorType != raw::DescriptorHead::TypeENDPOINT) continue;
					const raw::EndpointDescriptor& id = *(raw::EndpointDescriptor *)(config_descriptor + i);
					
					// Is this an IN and Bulk?
					if (id.bEndpointAddress & (1 << 7) && (id.bmAttributes & 0b1111) == 0b10) {
						epNum = id.bEndpointAddress & 0xF;
						epMps = id.wMaxPacketSize;
						mode = 2;
					}

					break;
				}
			case 2:
				{
					// Is this a CS_ENDPOINT of type MS_GENERAL?
					if (((raw::DescriptorHead *)(config_descriptor + i))->bDescriptorType != 0x25) {
						mode = 1;
						continue;
					}
					if (config_descriptor[i + 2] /* subtype */ != 0x1) {
						mode = 1;
						continue;
					}

					// This is the endpoint.
					mode = 3;
				}
			default:
				break;
		}
	}

	// if mode == 3, this is the dnpoint.
	if (mode != 3) {
		// oh no
		return false;
	}
	
	// We can now configure the device
	
	auto ep0_pipe = hb->allocate_pipe();
	ep_in = hb->allocate_pipe();

	hb->set_retry_behavior(ep0_pipe, retry_behavior::CPUDelayed);
	hb->set_retry_behavior(ep_in, retry_behavior::DeferToNextFrame);

	// Send out a SET_CONFIGURATION request
	
	raw::SetupData req;

	req.bRequest = 9; // SET_CONFIGURATION
	req.bmRequestType = 0;
	req.wIndex = 0;
	req.wLength = 0;
	req.wValue = cd.bConfigurationValue;

	if (!helper::standard_control_request(hb, req, ep0_pipe)) return false;

	// Get rid of the ep0 pipe, since we don't need it anymore.
	
	hb->destroy_pipe(ep0_pipe);

	// Configure the pipe
	hb->configure_pipe(ep_in, 0x10, epNum, epMps, pipe::EndpointDirectionIn, pipe::EndpointTypeBulk, false);

	return true;
}

void usb::MidiDevice::update() {
	// Is an update ongoing?
	if (hb->check_xfer_state(ep_in) == transaction_status::InProgress) return; // Let it finish first.

	// Start a new transfer of max size 64 (since that's our buffer size)
	hb->submit_xfer(ep_in, 64, rx_buffer, pipe::Callback::create(*this, &MidiDevice::xfer_cb));
}

void usb::MidiDevice::xfer_cb(pipe_t idx, transaction_status status) {
	if (idx != ep_in) return;
	if (!(status == transaction_status::Ack || (status == transaction_status::Nak && hb->check_received_amount(ep_in)))) return;

	// We have received some data
	auto amt = hb->check_received_amount(ep_in);
	if (amt % 4) {
		return;
	}

	for (int i = 0; i < amt / 4; ++i) {
		EventType event = rx_buffer[i];
		switch (/* event type */ (event.cable_code & 0xF)) {
			case 0x2:
			case 0x6:
			case 0xC:
			case 0xD:
				cb(event.midi, 2);
				break;
			case 0x3:
			case 0x4:
			case 0x7:
			case 0x8:
			case 0x9:
			case 0xA:
			case 0xB:
			case 0xE:
				cb(event.midi, 3);
				break;
			case 0x5:
			case 0xF:
				cb(event.midi, 1);
				break;
		}
	}
}

void usb::MidiDevice::set_callback(EventCallback cb) {
	if (hb) {
		// prevent race conditions.
		while (hb->check_xfer_state(ep_in) == transaction_status::InProgress) {;}
	}
	this->cb = cb;
}

bool usb::HID::handles(uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol) {
	return false;
}

bool usb::HID::init(uint8_t ep0_mps, uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol, uint8_t* config_descriptor, usb::HostBase *hb) {return false;}
