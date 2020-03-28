#include <stm32f4xx.h>
#include "ui.h"

void ui_init() {
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
	uint32_t tmp = RCC->AHB1ENR;
	(void)tmp;

	GPIOF->MODER = (GPIO_MODER_MODE5_0 | GPIO_MODER_MODE6_0 | GPIO_MODER_MODE7_0 |
					GPIO_MODER_MODE8_0 | GPIO_MODER_MODE9_0);
}

uint32_t ui_get_button() {
#define wait_stable for (int j = 0; j < 16; ++j) {asm volatile ("nop");}
	uint32_t buttons_held = 0;

	for (int i = 9; i >= 5; --i) {
		GPIOF->ODR &= ~(31ul) << 5;
		GPIOF->ODR |= 1 << i;
		wait_stable;
		buttons_held <<= 5;
		buttons_held |= GPIOF->IDR & 0b11111;
		GPIOF->ODR &= ~(31ul) << 5;
		wait_stable;
	}
#undef wait_stable

	return buttons_held;
}


