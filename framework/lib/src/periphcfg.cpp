#include <periphcfg.h>
#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>
#include <stm32f4xx_ll_usart.h>
#include <stm32f4xx_ll_bus.h>

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

		init.Autoreload = 256;
		init.CounterMode = LL_TIM_COUNTERMODE_UP;
		init.Prescaler = 69;
		init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV4;

		LL_TIM_Init(TIM2, &init);

		LL_TIM_OC_SetMode(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
		LL_TIM_OC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_HIGH);
		LL_TIM_OC_SetCompareCH1(TIM2, 0);

		LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH1);

		LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
		LL_TIM_EnableCounter(TIM2);
	}
}

void periph::bl::set(uint8_t level) {
	LL_TIM_OC_SetCompareCH1(TIM2, level);
}
