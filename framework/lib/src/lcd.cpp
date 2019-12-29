#include <lcd.h>
#include <stm32f4xx.h>
#include <stm32f4xx_ll_rcc.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_system.h>
#include <string.h>

void lcd::init() {
	// Clear the framebuffer
	memset(framebuffer_data, 0, sizeof(framebuffer_data));
	// Setup the GPIO
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_LTDC);

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOI);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
	/**LTDC GPIO Configuration    
	  PE4     ------> LTDC_B0
	  PE5     ------> LTDC_G0
	  PE6     ------> LTDC_G1
	  PI9     ------> LTDC_VSYNC
	  PI10     ------> LTDC_HSYNC
	  PF10     ------> LTDC_DE
	  PH2     ------> LTDC_R0
	  PH3     ------> LTDC_R1
	  PA3     ------> LTDC_B5
	  PB0     ------> LTDC_R3
	  PB1     ------> LTDC_R6
	  PE11     ------> LTDC_G3
	  PE12     ------> LTDC_B4
	  PE14     ------> LTDC_CLK
	  PE15     ------> LTDC_R7
	  PB10     ------> LTDC_G4
	  PB11     ------> LTDC_G5
	  PH8     ------> LTDC_R2
	  PH10     ------> LTDC_R4
	  PH11     ------> LTDC_R5
	  PD10     ------> LTDC_B3
	  PH13     ------> LTDC_G2
	  PI1     ------> LTDC_G6
	  PI2     ------> LTDC_G7
	  PD6     ------> LTDC_B2
	  PG12     ------> LTDC_B1
	  PB8     ------> LTDC_B6
	  PB9     ------> LTDC_B7 
	  */

	LL_GPIO_InitTypeDef gpio_init = {0};
	gpio_init.Pin = LL_GPIO_PIN_4|LL_GPIO_PIN_5|LL_GPIO_PIN_6|LL_GPIO_PIN_11 
		|LL_GPIO_PIN_12|LL_GPIO_PIN_14|LL_GPIO_PIN_15;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOE, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_9|LL_GPIO_PIN_10|LL_GPIO_PIN_1|LL_GPIO_PIN_2;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOI, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_10;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOF, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_2|LL_GPIO_PIN_3|LL_GPIO_PIN_8|LL_GPIO_PIN_10 
		|LL_GPIO_PIN_11|LL_GPIO_PIN_13;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOH, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_3;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOA, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_0|LL_GPIO_PIN_1;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_9;
	LL_GPIO_Init(GPIOB, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_10|LL_GPIO_PIN_11|LL_GPIO_PIN_8|LL_GPIO_PIN_9;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOB, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_10|LL_GPIO_PIN_6;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOD, &gpio_init);

	gpio_init.Pin = LL_GPIO_PIN_12;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio_init.Alternate = LL_GPIO_AF_14;
	LL_GPIO_Init(GPIOG, &gpio_init);

	// Setup the LTDC

	// Setup VSYNC, HSYNC, VPB, HPB, etc.
	LTDC->SSCR = (40 << LTDC_SSCR_HSW_Pos) | (9 << LTDC_SSCR_VSH_Pos); // 1 - v/hsync
	LTDC->BPCR = ((41 + 2 - 1) << LTDC_BPCR_AHBP_Pos) | ((10 + 2 - 1) << LTDC_BPCR_AVBP_Pos); // sync + back porch - 1
	LTDC->AWCR = ((41 + 2 + 480 - 1) << LTDC_AWCR_AAW_Pos) | ((10 + 2 + 272 - 1) << LTDC_AWCR_AAH_Pos);
	LTDC->TWCR = (524 << LTDC_TWCR_TOTALW_Pos) | (285 << LTDC_TWCR_TOTALH_Pos);

	// Setup clock polarity
	LTDC->GCR &= ~(LTDC_GCR_PCPOL | LTDC_GCR_HSPOL | LTDC_GCR_VSPOL | LTDC_GCR_DEPOL);

	// Background color is black by default, don't change it

	// Setup layer 1
	LTDC_Layer1->WHPCR = ((41 + 2) << LTDC_LxWHPCR_WHSTPOS_Pos) | ((41 + 2 + 480 - 1) << LTDC_LxWHPCR_WHSPPOS_Pos); // full line
	LTDC_Layer1->WVPCR = ((10 + 2) << LTDC_LxWVPCR_WVSTPOS_Pos) | ((10 + 2 + 272 - 1) << LTDC_LxWVPCR_WVSPPOS_Pos); // full col
	LTDC_Layer1->PFCR = 0b101; // L8 format

	// Framebuffer
	LTDC_Layer1->CFBAR = (uint32_t)&framebuffer_data[0];
	LTDC_Layer1->CFBLR = ((480 + 3) << LTDC_LxCFBLR_CFBLL_Pos) | (480 << LTDC_LxCFBLR_CFBP_Pos);
	LTDC_Layer1->CFBLNR = 272;

	// Enable Layer 1 
	LTDC_Layer1->CR |= LTDC_LxCR_LEN;

	// Flush registers
	LTDC->SRCR |= LTDC_SRCR_IMR;

	// Enable LTDC
	LTDC->GCR |= LTDC_GCR_LTDCEN;
}
