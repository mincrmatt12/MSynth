#pragma once
// sd.h -- SD card driver interface
//
// Loosely based on the one in the HAL, but designed for simpler use

namespace sd {
	// INITIALIZATION ROUTINES
	
	// Turn on the RCC to the SD peripheral, and setup the DETECTED gpio.
	// Enabling the EXTi line is optional
	void init(bool enable_exti=false);

	// Card initiailzation status
	enum struct init_status {
		Ok, // Inited correctly, card is ready for read-write and card parameters can be probed
		NotSupported, // The driver does not support this card.
		CardNotResponding, // We couldn't find a card (no response)
		MultipleCardsDetected, // There are two or more cards on the bus
		CardNotInserted, // The card insert switch was not set
	};

	// Perform card initialization
	init_status init_card();

	// Get card 
}
