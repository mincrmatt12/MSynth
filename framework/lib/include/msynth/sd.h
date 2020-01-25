#pragma once
// sd.h -- SD card driver interface
//
// Loosely based on the one in the HAL, but designed for simpler use
// 
// Also contains a _SUPER BASIC_ filesystem driver.

#include <stdint.h>

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
		NotInitialized, // The card has not been inited
	};

	// Holds information about initialized SD card
	struct Card {
		// Init status
		init_status status;

		// Card type
		enum {
			CardTypeSD,
			CardTypeSDHC
		} card_type;

		// Card length (bytes)
		uint64_t length;

		// Card sector length (maximum 512, only smaller than values are placed here)
		//
		// This is to support bizarro devices that have sectors smaller than 512 bytes.
		uint16_t sector_length;
	};

	// Perform card initialization
	Card init_card();
}
