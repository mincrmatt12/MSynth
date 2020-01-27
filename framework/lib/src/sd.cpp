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

struct no_response {};
struct card_interface_condition_r7 {
	uint32_t check_pattern : 8;
	uint32_t voltage_accepted : 4;

	// 20 stuff bits
};

struct published_rca_r6 {
	uint32_t card_status_bits : 16;
	uint32_t new_rca : 16;
};

struct status_r1 {
	private: uint32_t reserved1 : 5;
	public:
	uint32_t app_cmd : 1;
	uint32_t fx_event : 1;
	private: uint32_t reserved2 : 1;
	public:
	uint32_t ready_for_data : 1;
	uint32_t current_state : 4;
	uint32_t erase_reset : 1;
	uint32_t card_ecc_disabled : 1;
	uint32_t wp_erase_skip : 1;

	// ERRORS
	uint32_t csd_override : 1;
	private: uint32_t reserved3 : 2;
	public:
	
	uint32_t general_error : 1;
	uint32_t cc_error : 1;
	uint32_t card_ecc_failed : 1;
	uint32_t illegal_command : 1;
	uint32_t com_crc_error : 1;
	uint32_t lock_unlock_failed : 1;
	uint32_t card_is_locked : 1;
	uint32_t wp_violation : 1;
	uint32_t erase_param_error : 1;
	uint32_t erase_seq_error : 1;
	uint32_t block_len_error : 1;
	uint32_t address_error : 1;
	uint32_t out_of_range : 1;
};

struct send_op_cond_argument {
	uint32_t vdd_voltage : 24;
	uint32_t switch_v18_req : 1;
	private: uint32_t reserved : 3;
	public:
	uint32_t xpc : 1;
	uint32_t eSD : 1;
	uint32_t hcs : 1;
};

struct ocr_register {
	uint32_t vdd_voltage : 24;
	uint32_t switch_v18_acc : 1;
	private: uint32_t reserved2 : 4;
	public:
	uint32_t uhsII_card_status : 1;
	uint32_t card_capacity_status : 1;
	uint32_t busy : 1;
};

struct csd_register {

};

template<typename Argument, typename Response>
inline command_status send_command(Argument argument, uint32_t index, Response& response, uint32_t timeout=1) {
	if constexpr (!std::is_empty_v<Argument>) {
		static_assert(sizeof(Argument) == 4, "argument must be a 32-bit value");

		SDIO->ARG = reinterpret_cast<uint32_t>(argument);
	}

	static_assert(sizeof(Response) == 4 || sizeof(Response) == 16 || std::is_empty_v<Response>, "response must be either 32bits or 128bits or empty.");
	if constexpr (!std::is_empty_v<Response>) {
		SDIO->CMD = (index | 
				(sizeof(Response) == 4 ? SDIO_CMD_WAITRESP_0 : SDIO_CMD_WAITRESP) |
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

	if (sizeof(Response) == 4) {
		response = reinterpret_cast<Response>(SDIO->RESP1);
	}
	else {
		((uint32_t *)(&response))[0] = SDIO->RESP1;
		((uint32_t *)(&response))[1] = SDIO->RESP2;
		((uint32_t *)(&response))[2] = SDIO->RESP3;
		((uint32_t *)(&response))[3] = SDIO->RESP4;
	}

	return command_status::Ok;
}

template<typename Argument=uint32_t>
inline command_status send_command(Argument argument, uint32_t index, uint32_t timeout=1) {
	no_response x;
	return send_command(argument, index, x, timeout);
}

command_status send_command(uint32_t index) {
	return send_command(no_response{}, index, 1);
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

		init.Pin = LL_GPIO_PIN_8 | LL_GPIO_PIN_9 | LL_GPIO_PIN_10 | LL_GPIO_PIN_11 |
				   LL_GPIO_PIN_12;
		init.Mode = LL_GPIO_MODE_ALTERNATE;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
		init.Pull = LL_GPIO_PULL_NO;
		init.Alternate = LL_GPIO_AF_12;

		LL_GPIO_Init(GPIOC, &init);

		init.Pin = LL_GPIO_PIN_2;

		LL_GPIO_Init(GPIOD, &init);

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
		auto result = send_command(card_interface_condition_r7{0x3F, 1}, 0x8, response);

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
	
	uint32_t CID[16];
	
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
	// First, select the card
	
	{
		status_r1 response;
		if (send_command((uint32_t)card.RCA << 16 /* due to format this works */, 7, response) != command_status::Ok) {
			return init_status::CardNotResponding;
		}
	}

	// Now grab the CSD
}
