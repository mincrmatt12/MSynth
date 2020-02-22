#pragma once
// sd.h -- SD card driver interface
//
// Loosely based on the one in the HAL, but designed for simpler use
// 
// Also contains a decently time efficient and very space efficient FAT32 implementation.
// This impl is based (as much as possible) around ephemeral state wrappers as opposed to glutinous global globs of data.

#include <stdint.h>

namespace sd {
	// INITIALIZATION ROUTINES
	
	// Turn on the RCC to the SD peripheral, and setup the DETECTED gpio.
	// Enabling the EXTi line is optional
	void init(bool enable_dma=true, bool enable_exti=false);

	// Card initiailzation status
	enum struct init_status {
		Ok, // Inited correctly, card is ready for read-write and card parameters can be probed
		NotSupported, // The driver does not support this card.
		CardNotResponding, // We couldn't find a card (no response)
		MultipleCardsDetected, // There are two or more cards on the bus
		CardNotInserted, // The card insert switch was not set
		NotInitialized, // The card has not been inited
		InternalPeripheralError, // The SD peripheral failed to do something, was init() called first?
		CardUnusable, // The card is unusable due to it sending invalid or unrecognizable data
		CardIsSuperGluedShut, // The card is perm-write protected
		CardIsElmersGluedShut, // The card is temp-write protected, please fix it manually elsewhere
	};

	// Card access status
	enum struct access_status {
		Ok, // Data is present in the buffer / on the card and the operation was succesful
		InProgress, // The data is being transferred with no current errors
		Busy, // The thing is already going
		InvalidAddress, // The data is at an invalid address
		CardLockedError, // The card was placed in a password lock state
		CardNotResponding, // The card is not responding / timeout error
		NotInitialized, // The card is not initialized
		CardNotInserted, // The card was removed
		CRCError, // The checksum was bad
		DMATransferError, // There was a transfer error in the STM32 DMA -- most likely the buffer was not located in physical RAM
		CardWillForeverMoreBeStuckInAnEndlessWaltzSendingData, // The card failed to respond to a stop transmission
	};

	// Holds information about initialized SD card
	struct Card {
		// Init status
		init_status status = init_status::NotInitialized;

		// Card type
		enum {
			CardTypeSDVer1,
			CardTypeSDVer2OrLater,
			CardTypeSDHC,
			CardTypeUndefined
		} card_type = CardTypeUndefined;

		// Card length (sectors)
		uint64_t length = 0;

		// Busy status
		enum {
			ActiveStateInactive,
			ActiveStateWriting,
			ActiveStateReading,
			ActiveStateErasing,
			ActiveStateActiveUnknown,
			ActiveStateUndefined
		} active_state = ActiveStateUndefined;

		// Callback info
		void (*active_callback)(void *, access_status);
		void * argument;

		// Card info 
		uint16_t RCA;
		uint8_t  NSAC; // data timeout * 100

		// Various card info flags
	};

	// Perform card initialization
	init_status init_card();

	// Reset SD subsystem
	void reset();

	// STATE QUERY ROUTINES

	// Is an SD card currently inserted?
	bool inserted();

	// CARD ACCESS ROUTINES

	// Read `length_in_sectors` sectors starting at address `address` into `result_buffer`, without DMA.
	//
	// This function will block until the data is transferred. There are no restrictions on the location of the result buffer
	access_status read(uint32_t address, void * result_buffer, uint32_t length_in_sectors);

	// Read `length_in_sectors` sectors starting at address `address` into `result_buffer` asynchronously with DMA,
	// calling `callback` with argument `argument` and an access_status on completion/error.
	//
	// The result buffer must be located in the RAM segment; note that the stack is usually placed into CCMRAM which will cause a 
	// DMATransferError.
	access_status read(uint32_t address, void * result_buffer, uint32_t length_in_sectors, void (*callback)(void *, access_status), void * argument);

	// Read `length_in_sectors` sectors starting at address `address` into `result_buffer` asynchronously with DMA,
	// calling the method `callback` of the instance `instance`
	//
	// The result buffer must be located in the RAM segment; note that the stack is usually placed into CCMRAM which will cause a 
	// DMATransferError.
	//
	// This will not work properly with virtual methods.
	template<typename T>
	access_status read(uint32_t address, void * result_buffer, uint32_t length_in_sectors, void (T::* callback)(access_status), T& instance) {
		// This is technically non-standard, however the ARM abi works in such a way that this will function as intended without
		// needing some arcane usage of the ->* or .* operators.
		return read(address, result_buffer, length_in_sectors, (void (*)(void *, access_status))callback, &instance);
	}

	// Write `length_in_sectors` sectors starting at address `address` from `source_buffer`, without DMA.
	//
	// This function will block until the data is transferred. There are no restrictions on the location of the result buffer
	access_status write(uint32_t address, const void * source_buffer, uint32_t length_in_sectors);

	// Write `length_in_sectors` sectors starting at address `address` from `source_buffer` asynchronously with DMA,
	// calling `callback` with argument `argument` and an access_status on completion/error.
	//
	// The source buffer must be located in the RAM segment; note that the stack is usually placed into CCMRAM which will cause a 
	// DMATransferError.
	access_status write(uint32_t address, const void * source_buffer, uint32_t length_in_sectors, void (*callback)(void *, access_status), void * argument);

	// Write `length_in_sectors` sectors starting at address `address` from `source_buffer` asynchronously with DMA,
	// calling the method `callback` of the instance `instance`
	//
	// The source buffer must be located in the RAM segment; note that the stack is usually placed into CCMRAM which will cause a 
	// DMATransferError.
	//
	// This will not work properly with virtual methods.
	template<typename T>
	access_status write(uint32_t address, const void * source_buffer, uint32_t length_in_sectors, void (T::* callback)(access_status), T& instance) {
		// This is technically non-standard, however the ARM abi works in such a way that this will function as intended without
		// needing some arcane usage of the ->* or .* operators.
		return write(address, source_buffer, length_in_sectors, (void (*)(void *, access_status))callback, &instance);
	}

	// Erase `length_in_sectors` sectors starting at the address `address`
	access_status erase(uint32_t address, uint32_t length_in_sectors);

	// Erase the entire card
	access_status erase();

	// CARD EXTERN
	extern Card card;

	// INTERRUPT ROUTINES
	void sdio_interrupt(); // Call from SDIO_IRQHandler if using DMA mode.
}
