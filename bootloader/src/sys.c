#include <sys/stat.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_cortex.h>
#include <stm32f4xx_ll_rcc.h>
#include <stm32f4xx_ll_pwr.h>
#include <stm32f4xx_ll_usart.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx.h>

void sys_init() {
	// Turn on PWR clock
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;
	// Perform clock setup
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);

	if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5)
	{
	}

	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
	LL_PWR_DisableOverDriveMode();
	LL_RCC_HSE_Enable();

	/* Wait till HSE is ready */
	while(LL_RCC_HSE_IsReady() != 1)
	{

	}
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_24, 336, LL_RCC_PLLP_DIV_2);
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLM_DIV_24, 336, LL_RCC_PLLQ_DIV_7);
	LL_RCC_PLLI2S_ConfigDomain_I2S(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLI2SM_DIV_24, 271, LL_RCC_PLLI2SR_DIV_6);
	LL_RCC_PLLSAI_ConfigDomain_LTDC(LL_RCC_PLLSOURCE_HSE, LL_RCC_PLLSAIM_DIV_24, 200, LL_RCC_PLLSAIR_DIV_2, LL_RCC_PLLSAIDIVR_DIV_2);
	LL_RCC_PLL_Enable();

	/* Wait till PLL is ready */
	while(LL_RCC_PLL_IsReady() != 1)
	{

	}
	LL_RCC_PLLI2S_Enable();

	/* Wait till PLL is ready */
	while(LL_RCC_PLLI2S_IsReady() != 1)
	{

	}
	LL_RCC_PLLSAI_Enable();

	/* Wait till PLL is ready */
	while(LL_RCC_PLLSAI_IsReady() != 1)
	{

	}
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);

	/* Wait till System clock is ready */
	while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
	{

	}
	LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
	LL_RCC_SetI2SClockSource(LL_RCC_I2S1_CLKSOURCE_PLLI2S);
	LL_RCC_SetTIMPrescaler(LL_RCC_TIM_PRESCALER_TWICE);

	// init bkup region
	LL_PWR_EnableBkUpAccess();
	RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;

	// init USART3
	RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

	LL_USART_SetBaudRate(USART3, 42000000, LL_USART_OVERSAMPLING_16, 115200);
	LL_USART_SetOverSampling(USART3, LL_USART_OVERSAMPLING_16);
	LL_USART_SetParity(USART3, LL_USART_PARITY_NONE);
	LL_USART_SetTransferDirection(USART3, LL_USART_DIRECTION_TX_RX);
	LL_USART_SetHWFlowCtrl(USART3, LL_USART_HWCONTROL_NONE);
	LL_USART_SetDataWidth(USART3, LL_USART_DATAWIDTH_8B);
	LL_USART_ConfigAsyncMode(USART3);
	LL_USART_Enable(USART3);

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

int _close(int file) {return -1;}
int _fstat(int file, struct stat *st) {
	st->st_mode = S_IFCHR;
	return 0;
}

int _isatty(int file) {return 1;}

int _lseek(int file, int ptr, int dir) {return 0;}

int _open(const char *name, int flags, int mode) {return -1;}

int _read(int file, char *ptr, int len) {
	// Read len bytes from the debug UART3

	while (len--) {
		while (!LL_USART_IsActiveFlag_RXNE(USART3)) { /* wait until data available */ }
		LL_USART_ClearFlag_RXNE(USART3);
		*ptr++ = LL_USART_ReceiveData8(USART3);
	}

	return 0;
}

int _write(int file, char *ptr, int len) {
	while (len--) {
		LL_USART_TransmitData8(USART3, *ptr++);
		while (!LL_USART_IsActiveFlag_TC(USART3));
		LL_USART_ClearFlag_TC(USART3);
	}

	return 0;
}

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
extern void (*__preinit_array_start[])(void);
extern void (*__preinit_array_end[])(void);

void __wrap___libc_init_array() {
	// Call constructors
	//
	int cpp_size = &(__preinit_array_end[0]) - &(__preinit_array_start[0]);
    for (int cpp_count = 0; cpp_count < cpp_size; ++cpp_count) {
        __preinit_array_start[cpp_count]();
    }
	cpp_size = &(__init_array_end[0]) - &(__init_array_start[0]);
    for (int cpp_count = 0; cpp_count < cpp_size; ++cpp_count) {
        __init_array_start[cpp_count]();
    }
}

void __wrap_atexit() {}
