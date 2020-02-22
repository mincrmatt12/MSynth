#include <sd.h>
#include <util.h>

#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_exti.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_dma.h>
#include <type_traits>

// Sd card driver

enum struct command_status {
	Ok,
	TimeoutError,
	CRCError,
	WrongResponse
};

// Helper class for bit definitions inside a struct
// You should use this class by placing it in a union with a member of size Total bits
template<std::size_t Total, std::size_t Index, std::size_t LocalSize, typename AccessType=std::conditional_t<LocalSize == 1, bool, uint32_t>>
struct reg_bit {
	typedef AccessType access_type;
	static_assert(Total % 8 == 0, "Total size must align to byte");
	static_assert(LocalSize < sizeof(AccessType) * 8, "Local size must fit inside the access type");
	static_assert(LocalSize < 32, "Local size must be less than 32 bits");
	static_assert(Index < Total, "Index must be inside struct");

	inline operator access_type() const {
		return static_cast<access_type>((*(reinterpret_cast<const uint32_t *>(data + (Index / 8))) >> (Index % 8)) & ((1U << LocalSize) - 1));
	}

	inline reg_bit& operator=(const access_type& other) {
		*(reinterpret_cast<uint32_t *>(data + (Index / 8))) &= ~(((1U << LocalSize) - 1) << (Index % 8));
		*(reinterpret_cast<uint32_t *>(data + (Index / 8))) |= (static_cast<uint32_t>(other) << (Index % 8));
		return *this;
	}
	
private:
	uint8_t data[Total / 8];
};

template<std::size_t Index, std::size_t LocalSize>
using u32_bit = reg_bit<32, Index, LocalSize>;

template<std::size_t Index, std::size_t LocalSize>
using u128_bit = reg_bit<128, Index, LocalSize>;

struct no_response {};
struct card_interface_condition_r7 {
	union {
		uint32_t raw_data = 0;

		u32_bit<0, 8> check_pattern;
		u32_bit<8, 4> voltage_accepted;
	};
};

struct published_rca_r6 {
	uint16_t card_status_bits;
	uint16_t new_rca;
};

struct status_r1 {
	union {
		uint32_t raw_data = 0;

		u32_bit<5, 1> app_cmd;
		u32_bit<6, 1> fx_event;
		u32_bit<8, 1> ready_for_data;
		u32_bit<9, 4> current_state;
		u32_bit<13, 1> erase_reset;
		u32_bit<14, 1> card_ecc_disabled;
		u32_bit<15, 1> wp_erase_skip;

		// ERRORS
		u32_bit<16, 1> csd_override;
		u32_bit<19, 1> general_error;
		u32_bit<20, 1> cc_error;
		u32_bit<21, 1> card_ecc_failed;
		u32_bit<22, 1> illegal_command;
		u32_bit<23, 1> com_crc_error;
		u32_bit<24, 1> lock_unlock_failed;
		u32_bit<25, 1> card_is_locked;
		u32_bit<26, 1> wp_violation;
		u32_bit<27, 1> erase_param_error;
		u32_bit<28, 1> erase_seq_error;
		u32_bit<29, 1> block_len_error;
		u32_bit<30, 1> address_error;
		u32_bit<31, 1> out_of_range;
	};
};

struct send_op_cond_argument {
	union {
		uint32_t raw_data = 0;

		u32_bit<0, 24> vdd_voltage;
		u32_bit<24, 1> switch_v18_req;

		u32_bit<28, 1> xpc;
		u32_bit<29, 1> eSD;
		u32_bit<30, 1> hcs;
	};
};

struct scr_register {
	union {
		uint64_t raw_data;

		// 0 - CMD20
		// 1 - CMD23
		// 2 - CMD48/49
		// 3 - CMD58/59
		reg_bit<64, 32, 4> cmd_support;
		reg_bit<64, 38, 4> sd_specx;
		reg_bit<64, 42, 1> sd_spec4;
		reg_bit<64, 43, 4> ex_security;
		reg_bit<64, 47, 1> sd_spec3;
		reg_bit<64, 48, 4> sd_bus_widths;
		reg_bit<64, 52, 3> sd_security;
		reg_bit<64, 55, 1> data_stat_after_erase;
		reg_bit<64, 46, 4> sd_spec;
		reg_bit<64, 60, 4> scr_structure_ver;
	};
};

struct ocr_register {
	union {
		uint32_t raw_data;
		
		u32_bit<0, 24> vdd_voltage;
		u32_bit<24, 1> switch_v18_acc;
		u32_bit<29, 1> uhsII_card_status;
		u32_bit<30, 1> card_capacity_status;
		u32_bit<31, 1> busy;
	};
};

struct csd_register {
	enum CsdStructureVersion {
		CsdVersion1SC = 0,
		CsdVersion2HCXC = 1
	};

	union {
		uint32_t data[4];

		reg_bit<128, 126, 2, CsdStructureVersion> csd_structure_ver;

		// Common parameters

		u128_bit<112, 8> taac;
		u128_bit<104, 8> nsac;
		u128_bit<96, 8> transfer_speed;
		u128_bit<84, 12> card_command_classes;
		u128_bit<80, 4> read_bl_len;
		u128_bit<79, 1> read_bl_partial;
		u128_bit<78, 1> write_blk_misalign;
		u128_bit<77, 1> read_blk_misalign;
		u128_bit<76, 1> dsr_implemented;

		u128_bit<46, 3> erase_blk_en;
		u128_bit<39, 7> erase_sector_size;

		u128_bit<32, 7> wp_grp_size;
		u128_bit<31, 1> wp_grp_enable;

		u128_bit<26, 3> write_speed_factor;
		u128_bit<22, 4> write_bl_len;
		u128_bit<21, 1> write_bl_partial;

		u128_bit<15, 1> file_format_grp;
		u128_bit<14, 1> copy;
		u128_bit<13, 1> perm_write_protect;
		u128_bit<12, 1> tmp_write_protect;
		u128_bit<10, 2> file_format;

		u128_bit<1, 7> crc7;

		union {
			u128_bit<62, 12> card_size;
			u128_bit<59, 3> vdd_r_curr_min;
			u128_bit<56, 3> vdd_r_curr_max;
			u128_bit<53, 3> vdd_w_curr_min;
			u128_bit<50, 3> vdd_w_curr_max;
			u128_bit<47, 3> card_size_mult;
		} v1;

		union {
			u128_bit<48, 22> card_size;
		} v2;
	};
};

inline void clear_sd_flags() {
	SDIO->ICR = ((uint32_t)(SDIO_STA_CCRCFAIL | SDIO_STA_DCRCFAIL | SDIO_STA_CTIMEOUT |\
		SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_RXOVERR  |\
		SDIO_STA_CMDREND  | SDIO_STA_CMDSENT  | SDIO_STA_DATAEND  |\
		SDIO_STA_DBCKEND));
}

template<typename Argument, typename Response>
command_status send_command(Argument argument, uint32_t index, Response& response, uint32_t timeout=50) {
	if constexpr (!std::is_empty_v<Argument>) {
		static_assert(sizeof(Argument) == 4, "argument must be a 32-bit value");

		SDIO->ARG = *reinterpret_cast<uint32_t *>(&argument);
	}

	static_assert(sizeof(Response) == 4 || sizeof(Response) == 16 || std::is_empty_v<Response>, "response must be either 32bits or 128bits or empty.");
	if constexpr (!std::is_empty_v<Response>) {
		SDIO->CMD = (index | 
				(sizeof(Response) == 4 ? 0b1 << SDIO_CMD_WAITRESP_Pos : 0b11 << SDIO_CMD_WAITRESP_Pos) |
				SDIO_CMD_CPSMEN);
	

		while (!(SDIO->STA & (SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT | SDIO_STA_CMDREND))) {
			if (timeout == 0) {
				return command_status::TimeoutError;
			}
			--timeout;
			util::delay(1);
		}
	}
	else {
		SDIO->CMD = (index | SDIO_CMD_CPSMEN);
	
		while (!(SDIO->STA & (SDIO_STA_CMDSENT))) {
			if (timeout == 0) {
				return command_status::TimeoutError;
			}
			--timeout;
			util::delay(1);
		}

		// Clear flags
		clear_sd_flags();

		return command_status::Ok;
		
	}

	// Copy the response before we error out on CRC / TIMEOUT so that extra error handling can be implemented downstream
	// if desired.
	if constexpr (sizeof(Response) == 4) {
		response = const_cast<const Response &>(*reinterpret_cast<const volatile Response *>(&SDIO->RESP1));
	}
	else {
		((uint32_t *)(&response))[3] = SDIO->RESP1; // 127:96
		((uint32_t *)(&response))[2] = SDIO->RESP2;
		((uint32_t *)(&response))[1] = SDIO->RESP3;
		((uint32_t *)(&response))[0] = SDIO->RESP4;
	}

	if (SDIO->STA & SDIO_STA_CTIMEOUT) {
		clear_sd_flags();

		return command_status::TimeoutError;
	}
	if (SDIO->STA & SDIO_STA_CCRCFAIL) {
		clear_sd_flags();

		return command_status::CRCError;
	}

	// Clear flags
	clear_sd_flags();

	if constexpr (sizeof(Response) == 4) {
		// Only valid when not long response (see RM 31.9.5)
		if ((SDIO->RESPCMD & 0b111111) != (index & 0b111111))  {
			return command_status::WrongResponse;
		}
	}

	return command_status::Ok;
}

template<typename Argument=uint32_t>
inline command_status send_command(Argument argument, uint32_t index, uint32_t timeout=50) {
	no_response x;
	return send_command(argument, index, x, timeout);
}

inline command_status send_command(uint32_t index) {
	return send_command(no_response{}, index, 50);
}

sd::Card sd::card;

void sd::init(bool enable_dma, bool enable_exti) {
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);

	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SDIO);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);

	{
		LL_GPIO_InitTypeDef init;

		// Setup SDIO pins

		init.Pin = LL_GPIO_PIN_8 | LL_GPIO_PIN_9 | LL_GPIO_PIN_10 | LL_GPIO_PIN_11;
		init.Mode = LL_GPIO_MODE_ALTERNATE;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
		init.Pull = LL_GPIO_PULL_UP;
		init.Alternate = LL_GPIO_AF_12;

		LL_GPIO_Init(GPIOC, &init);

		init.Pin = LL_GPIO_PIN_2;

		LL_GPIO_Init(GPIOD, &init);

		init.Pin = LL_GPIO_PIN_12;
		init.Pull = LL_GPIO_PULL_NO;

		LL_GPIO_Init(GPIOC, &init);

		// Setup the inserted pin gpio
		
		init.Pin = LL_GPIO_PIN_0;
		init.Mode = LL_GPIO_MODE_INPUT;
		init.Speed = LL_GPIO_SPEED_FREQ_LOW;
		init.Pull = LL_GPIO_PULL_UP;
		
		LL_GPIO_Init(GPIOG, &init);
	}

	// Enable the EXTI line if the app wanted to
	if (enable_exti) {
		LL_EXTI_InitTypeDef init;

		init.Line_0_31 = LL_EXTI_LINE_0; // PG0
		init.LineCommand = ENABLE;
		init.Mode = LL_EXTI_MODE_IT;
		init.Trigger = LL_EXTI_TRIGGER_RISING_FALLING;

		LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTG, LL_SYSCFG_EXTI_LINE0);
		LL_EXTI_Init(&init);
	}

	// Enable the DMA if the app wants to. Note that the interrupt lines _should_ be setup
	// at this point, so lets also turn on the interrupts
	if (enable_dma) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
		
		LL_DMA_SetStreamPriorityLevel(DMA2, LL_DMA_STREAM_3, LL_DMA_PRIORITY_HIGH);
		LL_DMA_SetChannelSelection(DMA2, LL_DMA_STREAM_3, LL_DMA_CHANNEL_4);
		LL_DMA_SetMode(DMA2, LL_DMA_STREAM_3, LL_DMA_MODE_PFCTRL);
		LL_DMA_SetPeriphAddress(DMA2, LL_DMA_STREAM_3, (uint32_t)&SDIO->FIFO);
		LL_DMA_SetPeriphSize(DMA2, LL_DMA_STREAM_3, LL_DMA_PDATAALIGN_WORD);
		LL_DMA_SetMemorySize(DMA2, LL_DMA_STREAM_3, LL_DMA_MDATAALIGN_WORD);
		LL_DMA_SetPeriphBurstxfer(DMA2, LL_DMA_STREAM_3, LL_DMA_PBURST_INC4);
		LL_DMA_SetMemoryBurstxfer(DMA2, LL_DMA_STREAM_3, LL_DMA_MBURST_INC4);
		LL_DMA_EnableFifoMode(DMA2, LL_DMA_STREAM_3);
		LL_DMA_SetFIFOThreshold(DMA2, LL_DMA_STREAM_3, LL_DMA_FIFOTHRESHOLD_FULL);

		LL_DMA_SetMemoryIncMode(DMA2, LL_DMA_STREAM_3, LL_DMA_MEMORY_INCREMENT);
		LL_DMA_SetPeriphIncMode(DMA2, LL_DMA_STREAM_3, LL_DMA_PERIPH_NOINCREMENT);

		// Enable SDIO global interrupt
		SDIO->MASK = 0; // disable all interrupts from the SDIO

		NVIC_SetPriority(SDIO_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
		NVIC_EnableIRQ(SDIO_IRQn);
	}

	// SD peripheral config is done during init_card
}

bool sd::inserted() {
	return LL_GPIO_IsInputPinSet(GPIOG, LL_GPIO_PIN_0); // TODO: inverted because of my stupidity
}

sd::init_status sd::init_card() {
	// Before doing anything too interesting, check if a card is inserted
	
	if (!inserted()) {
		return init_status::CardNotInserted;
	}

	// Begin SD power sequence:
	// 	1. Setup SDIO settings to default for init
	// 	   400khz clock
	// 	   rising edge
	// 	   no clock bypass
	// 	   1bit bus width
	// 	   no flow ctrl
	//
	// 	   and enable the SD clk power
	
	SDIO->CLKCR = (0x76 << SDIO_CLKCR_CLKDIV_Pos) /* 48mhz / 121 < 400khz) */ |
			// no clken
			// no bypass
			// no negedge
			// no hwfc
			// no pwrsav
			// no clken
			0;

	util::delay(1); // there's some mumbo-jumbo about wait times, and although I _could_ calculate them, a 1ms delay won't kill anything here

	// Stop clock
	SDIO->CLKCR &= ~SDIO_CLKCR_CLKEN;

	util::delay(1);
	
	// Enable SD power
	
	SDIO->POWER = 0b11;

	util::delay(1);
	
	// Enable SD CLK
	
	SDIO->CLKCR |= SDIO_CLKCR_CLKEN;

	// Wait 2ms (datasheet says to wait 2ms before starting init procedure -- see SD spec)
	
	util::delay(20); // give it a bit to start itself

	//  2. Send CMD0 (SET_TO_IDLE)
	if (send_command(0) != command_status::Ok) return init_status::InternalPeripheralError;

	//  3. Try to send CMD8
	{
		card_interface_condition_r7 response;
		card_interface_condition_r7 argument;
		argument.check_pattern = 0xAA;
		argument.voltage_accepted = 1;
		auto result = send_command(argument, 0x8, response);

		if (result == command_status::Ok) {
			// Version 2 or greater card
			
			// Check validity of response
			if (response.check_pattern != 0xAA || response.voltage_accepted != 1) {
				// Unusable card
				return init_status::NotSupported;
			}

			// Continue in busy checking loop
			
			// Try to send ACMD41
			ocr_register response;

			int count = 0;
			do {
				send_op_cond_argument arg{}; 
				arg.hcs = 1; // We support high capcity cards
				arg.vdd_voltage = (1 << 20); // 3.2 - 3.3V
				if (++count == 0xFFF) return init_status::CardNotResponding;
				if (send_command<uint32_t>(0 /* broadcast 0 rca */, 55 /* APP_CMD */) != command_status::Ok) return init_status::InternalPeripheralError;
				if (auto result = send_command(arg, 41, /* AppOperCond */ response); result != command_status::Ok && result != command_status::CRCError) { // CRCs are allowed to fail here
					return init_status::CardUnusable;
				}
			} while (!response.busy);

			if (!(response.vdd_voltage & (1 << 20))) {
				return init_status::NotSupported;
			}

			// Card is good, does it want to be referred to by SDHC?
			
			if (response.card_capacity_status)
				card.card_type = Card::CardTypeSDHC;
			else
				card.card_type = Card::CardTypeSDVer2OrLater;
		}
		else if (result == command_status::TimeoutError) {
			// Version 1 card
			
			// Try to send ACMD41
			ocr_register response;

			int count = 0;
			do {
				send_op_cond_argument arg{}; 
				arg.hcs = 0;
				arg.vdd_voltage = (1 << 20); // 3.2 - 3.3V
				if (++count == 0xFFF) return init_status::CardNotResponding;
				if (send_command<uint32_t>(0, 55 /* APP_CMD */) != command_status::Ok) return init_status::InternalPeripheralError;
				if (auto result = send_command(arg, 41, /* AppOperCond */ response); result != command_status::Ok && result != command_status::CRCError) { // CRCs are allowed to fail for ACMD41
					if (result == command_status::TimeoutError) {
						return init_status::CardNotResponding; // this is the path that we will get if no responses happen whatsoever
					}
					return init_status::CardUnusable;
				}
			} while (!response.busy); // active low

			if (!(response.vdd_voltage & (1 << 20))) {
				return init_status::NotSupported;
			}

			// Otherwise, this is an SD type 1 card
			card.card_type = Card::CardTypeSDVer1;
		}
		else {
			// Invalid card
			return init_status::CardUnusable;
		}
	}

	// Tell the card to send us it's CID (we don't care about it, but we still need to tell it
	// to send it so it goes to the right state)
	
	uint32_t CID[4];
	
	if (send_command(0xD00FBA4F /* stuff bits */, 2 /* ALL_SEND_CID */, CID) != command_status::Ok) {
		// Failed
		return init_status::CardNotResponding;
	}

	util::delay(5); // give the card some time to respond to that amazingly large 128-bit response

	// Alright, we're almost there
	
	{
		published_rca_r6 response;
		if (send_command(0xD00FBA4F /* stuff bits */, 3 /* SEND_RELATIVE_ADDR */, response) != command_status::Ok) {
			return init_status::CardNotResponding;
		}

		// Store the RCA
		card.RCA = response.new_rca;
	}

	// Begin STAGE 2: grabbing the CSD
	
	{
		csd_register result;
		
		if (send_command((uint32_t)card.RCA << 16, 9 /* SEND_CSD */, result) != command_status::Ok) {
			// Card was unitialized
			return init_status::CardNotResponding;
		}

		// Alright, now let's do the following:
		//  - Check "glued" status
		if (result.perm_write_protect) return init_status::CardIsSuperGluedShut;
		if (result.tmp_write_protect) return init_status::CardIsElmersGluedShut;
		// 	- Set the card length
		// 	- (TODO): Set the card speed (used to determine what sample rates are reasonable)
		// 	- (TODO): other stuff with the CSD?
		
		if (result.csd_structure_ver == csd_register::CsdVersion1SC) {
			// Check card size from C_SIZE and C_SIZE_MULT and READ_BL_LEN
			card.length = (uint64_t(result.v1.card_size + 1) * (1 << (result.v1.card_size_mult + 2))) * (1 << result.read_bl_len);
		}
		else {
			// Check card size using a much more sane method
			card.length = (uint64_t(result.v2.card_size + 1) * 512 * 1024);
		}
	}
	
	util::delay(5);
	// STAGE 3: select the card and move to faster bus speed
	
	{
		status_r1 status;

		if (send_command((uint32_t)card.RCA << 16, 7 /* SELECT_DESELECT_CARD */, status) != command_status::Ok) {
			return init_status::CardNotResponding;
		}

		util::delay(1);

		// Set bus width
		if (send_command<uint32_t>(card.RCA << 16, 55 /* APP_CMD */) != command_status::Ok) return init_status::InternalPeripheralError;
		if (send_command<uint32_t>(2 /* 0b10 4 bit */, 6 /* ACMD6 */, status) != command_status::Ok) return init_status::NotSupported;

		// Change bus speed settings
		
		SDIO->CLKCR = 0x00 | SDIO_CLKCR_CLKEN | SDIO_CLKCR_WIDBUS_0;

		util::delay(5);
	}

	if (card.card_type != Card::CardTypeSDHC) {
		// (OPTIONAL) STAGE 4: Set block length
		status_r1 status;
		if (send_command(512, 16, status) != command_status::Ok) return init_status::CardNotResponding;
		if (status.block_len_error) return init_status::NotSupported;
	}
	else {
		// (OPTIONAL) STAGE 4: check support for cmd23
	}

	card.status = init_status::Ok;
	card.active_state = Card::ActiveStateInactive;
	return init_status::Ok;
}

void sd::reset() {
	// Is sd card init?
	if (card.status != init_status::Ok) return;

	// Deinit card
	card.status = init_status::NotInitialized;

	// TODO: Send shutdown command
	
	// Set card to IDLE and disable power
	send_command(0);

	util::delay(5);

	SDIO->CLKCR &= ~SDIO_CLKCR_CLKEN;

	util::delay(5);

	SDIO->POWER = 0;

	util::delay(5);

	return;
}

// Blocking read
sd::access_status sd::read(uint32_t address, void * result_buffer, uint32_t length_in_sectors) {
	// POLLING gets stuck a lot, use a slower clock speed
	
	SDIO->CLKCR = 0x6E | SDIO_CLKCR_WIDBUS_0 | SDIO_CLKCR_CLKEN;

	uint32_t * out_buffer = static_cast<uint32_t *>(result_buffer);
	if (card.status != init_status::Ok) return access_status::NotInitialized;

	// TODO: SUPPORT MULTIBLOCK LENGTH
	
	if (length_in_sectors == 0) return access_status::Ok;

	if (card.card_type != Card::CardTypeSDHC) address *= 512;

	// Program the DPSM for this transfer
	SDIO->DLEN = length_in_sectors * 512;
	SDIO->DTIMER = 0xFFF;
	SDIO->DCTRL = (0b1001u /* 512 */ << SDIO_DCTRL_DBLOCKSIZE_Pos) |
		SDIO_DCTRL_DTDIR /* card -> host */; 

	util::delay(1);

	status_r1 status;
	if (length_in_sectors == 1) {
		// Start transfer
		SDIO->DCTRL |= SDIO_DCTRL_DTEN;
		if (send_command(address, 17, status) != command_status::Ok) {
			clear_sd_flags();

			return sd::access_status::CardNotResponding;
		}
	}
	else {
		// Start transfer (multiblock)
		SDIO->DCTRL |= SDIO_DCTRL_DTEN;
		if (send_command(address, 18, status) != command_status::Ok) {
			clear_sd_flags();

			return sd::access_status::CardNotResponding;
		}
	}

	// Check status flags
	if (status.address_error) {clear_sd_flags(); return access_status::InvalidAddress;}
	if (status.card_is_locked) {clear_sd_flags(); return access_status::CardLockedError;}

	// Begin transferring
	
	while (!(SDIO->STA & (SDIO_STA_RXOVERR | SDIO_STA_DCRCFAIL | SDIO_STA_DTIMEOUT | SDIO_STA_DATAEND | SDIO_STA_STBITERR))) {
		// Read lots of FIFO if possible
		while (SDIO->STA & SDIO_STA_RXFIFOHF) {
			*out_buffer++ = SDIO->FIFO; 
		}

		// TODO: Read timeout
	}

	// Send data end
	if (SDIO->STA & SDIO_STA_DATAEND && length_in_sectors > 1) {
		// Send data end
		if (send_command(0xDEAD9009, 12, status) != command_status::Ok) {
			clear_sd_flags();

			return access_status::CardWillForeverMoreBeStuckInAnEndlessWaltzSendingData;
		}
	}

	if (SDIO->STA & SDIO_STA_RXOVERR) {
		// fifo overrun (kill this flag and pray nothing too bad happens)
		clear_sd_flags();

		return access_status::DMATransferError;
	}

	else if (SDIO->STA & SDIO_STA_DCRCFAIL) {
		clear_sd_flags();

		return access_status::CRCError;
	}

	else if (SDIO->STA & SDIO_STA_DTIMEOUT) {
		clear_sd_flags();

		return access_status::CardNotResponding;
	}

	else if (!(SDIO->STA & SDIO_STA_DATAEND)) {
		clear_sd_flags();

		return access_status::CardNotResponding;
	}

	while (SDIO->STA & SDIO_STA_RXDAVL) {
		*out_buffer++ = SDIO->FIFO;

		// TODO: READ TIMEOUT
	}

	clear_sd_flags();

	SDIO->CLKCR = 0x00 | SDIO_CLKCR_WIDBUS_0 | SDIO_CLKCR_CLKEN;

	return access_status::Ok;
}

sd::access_status sd::read(uint32_t address, void *result_buffer, uint32_t length_in_sectors, void (*callback)(void *, sd::access_status), void *argument) {
	// DMA uses <electroboomvoice>FULL CLOCK SPEED</electroboomvoice>, start by instead checking if there is an ongoing
	// transmission
	
	if (card.active_state != sd::Card::ActiveStateInactive) {
		return access_status::Busy;
	}
	if (card.status != init_status::Ok) {
		return access_status::NotInitialized;
	}

	card.active_state = Card::ActiveStateReading;
	card.active_callback = callback;
	card.argument = argument;

	// Begin by setting up the DMA transfer
	
	LL_DMA_SetMemoryAddress(DMA2, LL_DMA_STREAM_3, (uint32_t)result_buffer);
	LL_DMA_SetDataTransferDirection(DMA2, LL_DMA_STREAM_3, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
	LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_3, length_in_sectors * 128);
	LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_3);

	// Send the read command
	if (card.card_type != Card::CardTypeSDHC) address *= 512;

	clear_sd_flags();

	SDIO->DTIMER = 0xFFFFF;
	SDIO->DLEN = length_in_sectors * 512;
	SDIO->DCTRL = (0b1001u << SDIO_DCTRL_DBLOCKSIZE_Pos) | SDIO_DCTRL_DTDIR | SDIO_DCTRL_DMAEN | SDIO_DCTRL_DTEN;
	SDIO->MASK = SDIO_MASK_RXOVERRIE | SDIO_MASK_DCRCFAILIE | SDIO_MASK_DTIMEOUTIE | SDIO_MASK_DATAENDIE | SDIO_MASK_STBITERRIE;

	status_r1 status;
	if (length_in_sectors == 1) {
		if (send_command(address, 17, status) != command_status::Ok) {
			clear_sd_flags();
			
			LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
			return access_status::CardNotResponding;
		}
	}
	else {
		if (send_command(address, 18, status) != command_status::Ok) {
			clear_sd_flags();
			
			LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
			return access_status::CardNotResponding;
		}
	}


	// Check status flags
	if (status.address_error) {clear_sd_flags(); LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3); return access_status::InvalidAddress;}
	if (status.card_is_locked) {clear_sd_flags(); LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3); return access_status::CardLockedError;}

	// Operation in progress
	return access_status::InProgress;
}

// Interrupt handler

void sd::sdio_interrupt() {
	if (card.active_state == Card::ActiveStateReading) {
		access_status mode = access_status::Ok;
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
		LL_DMA_ClearFlag_TC3(DMA2);
		
		SDIO->MASK = 0;
		if (SDIO->STA & SDIO_STA_DATAEND) {
			if (SDIO->DLEN > 512) {
				// Send a stop command
				status_r1 status;
				if (send_command(0xDA15C00L, 12, status) != command_status::Ok) {
					clear_sd_flags();

					mode = access_status::CardWillForeverMoreBeStuckInAnEndlessWaltzSendingData;
				}
			}
		}
		else if (SDIO->STA & SDIO_STA_DCRCFAIL) {
			mode = access_status::CRCError;
		}
		else if (SDIO->STA & SDIO_STA_DTIMEOUT) {
			mode = access_status::CardNotResponding;
		}
		else if (SDIO->STA & SDIO_STA_RXOVERR) {
			mode = access_status::DMATransferError;
		}
		if (LL_DMA_IsActiveFlag_TE3(DMA2)) {
			// DMA TE, todo
			mode = access_status::DMATransferError;
			LL_DMA_ClearFlag_TE3(DMA2);
		}

		// Call the function
		card.active_callback(card.argument, mode);
		card.active_state = Card::ActiveStateInactive;

		SDIO->DCTRL = 0;
	}

	clear_sd_flags();
}
