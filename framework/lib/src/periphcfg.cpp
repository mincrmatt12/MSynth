#include <periphcfg.h>
#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>
#include <stm32f4xx_ll_usart.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_adc.h>
#include <util.h>

void periph::setup_dbguart() {
	LL_USART_SetBaudRate(USART3, 42000000, LL_USART_OVERSAMPLING_16, 115200);
	LL_USART_SetOverSampling(USART3, LL_USART_OVERSAMPLING_16);
	LL_USART_SetParity(USART3, LL_USART_PARITY_NONE);
	LL_USART_SetTransferDirection(USART3, LL_USART_DIRECTION_TX_RX);
	LL_USART_SetHWFlowCtrl(USART3, LL_USART_HWCONTROL_NONE);
	LL_USART_SetDataWidth(USART3, LL_USART_DATAWIDTH_8B);
	LL_USART_ConfigAsyncMode(USART3);
	LL_USART_Enable(USART3);

	// GPIOD clock is forced on at bootloader

	LL_GPIO_SetPinMode(GPIOD, LL_GPIO_PIN_8, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinSpeed(GPIOD, LL_GPIO_PIN_8, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinOutputType(GPIOD, LL_GPIO_PIN_8, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(GPIOD, LL_GPIO_PIN_8, LL_GPIO_PULL_UP);
	LL_GPIO_SetAFPin_8_15(GPIOD, LL_GPIO_PIN_8, LL_GPIO_AF_7);

	LL_GPIO_SetPinMode(GPIOD, LL_GPIO_PIN_9, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinSpeed(GPIOD, LL_GPIO_PIN_9, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinOutputType(GPIOD, LL_GPIO_PIN_9, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(GPIOD, LL_GPIO_PIN_9, LL_GPIO_PULL_UP);
	LL_GPIO_SetAFPin_8_15(GPIOD, LL_GPIO_PIN_9, LL_GPIO_AF_7);
}

void periph::setup_midiuart() {
	// Enable clocks
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART6);

	LL_USART_SetBaudRate(USART6, 42000000, LL_USART_OVERSAMPLING_16, 31250);
	LL_USART_SetOverSampling(USART6, LL_USART_OVERSAMPLING_16);
	LL_USART_SetParity(USART6, LL_USART_PARITY_NONE);
	LL_USART_SetTransferDirection(USART6, LL_USART_DIRECTION_TX_RX);
	LL_USART_SetHWFlowCtrl(USART6, LL_USART_HWCONTROL_NONE);
	LL_USART_SetDataWidth(USART6, LL_USART_DATAWIDTH_8B);
	LL_USART_ConfigAsyncMode(USART6);
	LL_USART_Enable(USART6);

	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_8, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinSpeed(GPIOC, LL_GPIO_PIN_8, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinOutputType(GPIOC, LL_GPIO_PIN_8, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(GPIOC, LL_GPIO_PIN_8, LL_GPIO_PULL_UP);
	LL_GPIO_SetAFPin_8_15(GPIOC, LL_GPIO_PIN_8, LL_GPIO_AF_8);

	LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_9, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinSpeed(GPIOC, LL_GPIO_PIN_9, LL_GPIO_SPEED_FREQ_VERY_HIGH);
	LL_GPIO_SetPinOutputType(GPIOC, LL_GPIO_PIN_9, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(GPIOC, LL_GPIO_PIN_9, LL_GPIO_PULL_UP);
	LL_GPIO_SetAFPin_8_15(GPIOC, LL_GPIO_PIN_9, LL_GPIO_AF_8);
}

void periph::setup_blpwm() {
	// Setup GPIO
	{
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
		LL_GPIO_InitTypeDef init;

		init.Pin = LL_GPIO_PIN_15;
		init.Mode = LL_GPIO_MODE_ALTERNATE;
		init.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Alternate = LL_GPIO_AF_1;

		LL_GPIO_Init(GPIOA, &init);
	}

	// Enable TIM2 CH1 in OC mode

	{
		LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
		LL_TIM_InitTypeDef init;

		init.Autoreload = 255;
		init.CounterMode = LL_TIM_COUNTERMODE_UP;
		init.Prescaler = 89;
		init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV4;

		LL_TIM_Init(TIM2, &init);

		LL_TIM_OC_SetMode(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
		LL_TIM_OC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_HIGH);
		LL_TIM_OC_SetCompareCH1(TIM2, 0);

		LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);
		LL_TIM_CC_EnablePreload(TIM2);

		LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
		LL_TIM_EnableCounter(TIM2);
	}
}

void periph::bl::set(uint8_t level) {
	LL_TIM_OC_SetCompareCH1(TIM2, level);
}

void periph::ui::set(led which, bool state) {
	if (which == led::ALL) {
		set(led::F1, state);
		set(led::F2, state);
		set(led::F3, state);
		set(led::F4, state);
		set(led::PLAY, state);
		set(led::PATCH, state);
		set(led::REC, state);
		return;
	}
	GPIO_TypeDef *instance;
	int pin;
	switch ((int)which) {
		case 0:
		case 1:
		case 2:
			instance = GPIOE;
			pin = (2 - (int)which) + 7;
			break;
		case 4:
		case 5:
		case 6:
			instance = GPIOF;
			pin = (6 - (int)which) + 13;
			break;
		default:
			instance = GPIOG;
			pin = 1;
			break;
	}

	if (state) {
		instance->BSRR |= (1 << pin);
	}
	else {
		instance->BSRR |= (1 << (16+pin));
	}
}

namespace periph::ui {
	uint32_t buttons_pressed, buttons_held;
};

void periph::ui::poll() {
	#define wait_stable for (int j = 0; j < 16; ++j) {asm volatile ("nop");}
	uint32_t old = buttons_held;
	buttons_held = 0;

	for (int i = 9; i >= 5; --i) {
		GPIOF->ODR &= ~(0b11111ul) << 5;
		GPIOF->ODR |= 1 << i;
		wait_stable;
		buttons_held <<= 5;
		buttons_held |= GPIOF->IDR & 0b11111;
		GPIOF->ODR &= ~(0b11111ul) << 5;
		wait_stable;
	}

	buttons_pressed = ((old ^ buttons_held) & buttons_held) & ~buttons_pressed;
}

void periph::setup_ui() {
	// Init GPIOs
	{
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);

		// Setup LEDS
		LL_GPIO_InitTypeDef init = {0};

		init.Mode = LL_GPIO_MODE_OUTPUT;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Speed = LL_GPIO_SPEED_FREQ_LOW;
		init.Pin = LL_GPIO_PIN_13 | LL_GPIO_PIN_14 | LL_GPIO_PIN_15;  // LEDS 6, 5, 4
		LL_GPIO_Init(GPIOF, &init);

		init.Pin = LL_GPIO_PIN_1; // LED 3
		LL_GPIO_Init(GPIOG, &init);

		init.Pin = LL_GPIO_PIN_7 | LL_GPIO_PIN_8 | LL_GPIO_PIN_9; // LEDS 2, 1, 0
		LL_GPIO_Init(GPIOE, &init);

		// Setup button-matrix
		init.Pin = LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7 | LL_GPIO_PIN_8 | LL_GPIO_PIN_9;
		LL_GPIO_Init(GPIOF, &init);  // button output signals

		init.Mode = LL_GPIO_MODE_INPUT;
		init.Pull = LL_GPIO_PULL_NO;
		init.Pin  = LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2 | LL_GPIO_PIN_3 | LL_GPIO_PIN_4;
		LL_GPIO_Init(GPIOF, &init);

		// Setup adcs
		init.Mode = LL_GPIO_MODE_ANALOG;
		init.Pull = LL_GPIO_PULL_NO;
		init.Pin =  LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2;
		LL_GPIO_Init(GPIOA, &init);
	}

	// Init ADC
	{
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_ADC1);

		// Setup ADC1
		LL_ADC_InitTypeDef init;

		init.Resolution = LL_ADC_RESOLUTION_12B;
		init.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
		init.SequencersScanMode = LL_ADC_SEQ_SCAN_ENABLE;
		LL_ADC_Init(ADC1, &init);
	}

	// Init ADC Channels
	{
		LL_ADC_REG_InitTypeDef init_reg;
		LL_ADC_CommonInitTypeDef init_com;

		init_reg.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
		init_reg.SequencerLength = LL_ADC_REG_SEQ_SCAN_DISABLE;
		init_reg.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
		init_reg.ContinuousMode = LL_ADC_REG_CONV_SINGLE;
		init_reg.DMATransfer = LL_ADC_REG_DMA_TRANSFER_LIMITED;
		LL_ADC_REG_Init(ADC1, &init_reg);
		LL_ADC_REG_SetFlagEndOfConversion(ADC1, LL_ADC_REG_FLAG_EOC_UNITARY_CONV);
		LL_ADC_DisableIT_EOCS(ADC1);
		init_com.CommonClock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
		init_com.Multimode = LL_ADC_MULTI_INDEPENDENT;
		LL_ADC_CommonInit(__LL_ADC_COMMON_INSTANCE(ADC1), &init_com);

		LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_0, LL_ADC_SAMPLINGTIME_3CYCLES);
		LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_1, LL_ADC_SAMPLINGTIME_3CYCLES);
		LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_2, LL_ADC_SAMPLINGTIME_3CYCLES);

		LL_ADC_Enable(ADC1);
	}
}

uint16_t periph::ui::get(periph::ui::knob which) {
	// Set channel for conversion
	LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, (which == knob::VOLUME ?  LL_ADC_CHANNEL_0 : 
				                                          (which == knob::FX1 ? LL_ADC_CHANNEL_1 : 
														   LL_ADC_CHANNEL_2)));

	// Sample 3 times and average
	uint16_t sum = 0;
	for (int i = 0; i < 3; ++i) {
		// Convert
		LL_ADC_REG_StartConversionSWStart(ADC1);

		// Wait for conversion
		while (!LL_ADC_IsActiveFlag_EOCS(ADC1)) {;}
		LL_ADC_ClearFlag_EOCS(ADC1);

		// Read out value
		sum += LL_ADC_REG_ReadConversionData12(ADC1);
	}

	return sum / 3;
}
