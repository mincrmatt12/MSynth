#include <sd.h>
#include <util.h>

#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_exti.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_bus.h>
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
		return static_cast<access_type>((*(reinterpret_cast<const uint32_t *>(data + (Index / 8))) << (Index % 8)) & ((1U << LocalSize) - 1));
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
		SDIO->ICR = ((uint32_t)(SDIO_STA_CCRCFAIL | SDIO_STA_DCRCFAIL | SDIO_STA_CTIMEOUT |\
			SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_RXOVERR  |\
			SDIO_STA_CMDREND  | SDIO_STA_CMDSENT  | SDIO_STA_DATAEND  |\
			SDIO_STA_DBCKEND));

		return command_status::Ok;
		
	}

	if (SDIO->STA & SDIO_STA_CTIMEOUT) {
		SDIO->ICR = SDIO_STA_CTIMEOUT;

		return command_status::TimeoutError;
	}
	if (SDIO->STA & SDIO_STA_CCRCFAIL) {
		SDIO->ICR = SDIO_STA_CCRCFAIL;

		return command_status::CRCError;
	}

	// Clear flags
	SDIO->ICR = ((uint32_t)(SDIO_STA_CCRCFAIL | SDIO_STA_DCRCFAIL | SDIO_STA_CTIMEOUT |\
		SDIO_STA_DTIMEOUT | SDIO_STA_TXUNDERR | SDIO_STA_RXOVERR  |\
		SDIO_STA_CMDREND  | SDIO_STA_CMDSENT  | SDIO_STA_DATAEND  |\
		SDIO_STA_DBCKEND));

	if ((SDIO->RESPCMD & 0b111111) != (index & 0b111111))  {
		return command_status::WrongResponse;
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

void sd::init(bool enable_exti) {
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

	// SD peripheral config is done during init_card
}

bool sd::inserted() {
	return !LL_GPIO_IsInputPinSet(GPIOG, LL_GPIO_PIN_0);
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
	
	// Enable SD power
	
	SDIO->POWER = SDIO_POWER_PWRCTRL_1;
	
	// Enable SD CLK
	
	SDIO->CLKCR |= SDIO_CLKCR_CLKEN;

	// Wait 2ms (datasheet says to wait 2ms before starting init procedure -- see SD spec)
	
	util::delay(2);

	//  2. Send CMD0 (SET_TO_IDLE)
	if (send_command(0) != command_status::Ok) return init_status::InternalPeripheralError;

	//  3. Try to send CMD8
	{
		card_interface_condition_r7 response;
		card_interface_condition_r7 argument;
		argument.check_pattern = 0x3F;
		argument.voltage_accepted = 1;
		auto result = send_command(argument, 0x8, response);

		if (result == command_status::Ok) {
			// Version 2 or greater card
			
			// Check validity of response
			if (response.check_pattern != 0x3F || response.voltage_accepted != 1) {
				// Unusable card
				return init_status::NotSupported;
			}

			// Continue in busy checking loop
			
			// Try to send ACMD41
			send_op_cond_argument arg{}; 
			arg.hcs = 1; // We support high capcity cards
			arg.vdd_voltage = (1 << 20); // 3.2 - 3.3V
			ocr_register response;

			int count = 0;
			do {
				if (++count == 0xFFFF) return init_status::CardNotResponding;
				if (send_command(55 /* APP_CMD */) != command_status::Ok) return init_status::InternalPeripheralError;
				if (send_command(arg, 41, /* AppOperCond */ response) != command_status::Ok) {
					return init_status::CardUnusable;
				}
			} while (response.busy);

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
			send_op_cond_argument arg{}; 
			arg.hcs = 0;
			arg.vdd_voltage = (1 << 20); // 3.2 - 3.3V
			ocr_register response;

			int count = 0;
			do {
				if (++count == 0xFFFF) return init_status::CardNotResponding;
				if (send_command(55 /* APP_CMD */) != command_status::Ok) return init_status::InternalPeripheralError;
				if (send_command(arg, 41, /* AppOperCond */ response) != command_status::Ok) {
					return init_status::CardUnusable;
				}
			} while (response.busy);

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
			card.length = ((result.v1.card_size + 1) * (1 << (result.v1.card_size_mult + 2))) * (1 << result.read_bl_len);
		}
		else {
			// Check card size using a much more sane method
			card.length = ((result.v2.card_size + 1) * 512 * 1024);
		}
	}

	card.status = init_status::Ok;
	return init_status::Ok;
}
