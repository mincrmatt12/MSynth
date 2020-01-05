#include <lcd.h>
#include <util.h>
#include <stm32f4xx.h>
#include <stm32f4xx_ll_rcc.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_spi.h>
#include <string.h>
#include <stdio.h>
#include <utility>

void lcd::init() {
	// Clear the framebuffer
	memset(framebuffer_data, 0, sizeof(framebuffer_data));
	// Setup the GPIO
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_LTDC);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOI);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
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
#define VFP 6
	LTDC->SSCR = (40 << LTDC_SSCR_HSW_Pos) | (9 << LTDC_SSCR_VSH_Pos); // 1 - v/hsync
	LTDC->BPCR = ((41 + 2 - 1) << LTDC_BPCR_AHBP_Pos) | ((10 + VFP - 1) << LTDC_BPCR_AVBP_Pos); // sync + back porch - 1
	LTDC->AWCR = ((41 + 2 + 480 - 1) << LTDC_AWCR_AAW_Pos) | ((10 + VFP + 272 - 1) << LTDC_AWCR_AAH_Pos);
	LTDC->TWCR = (524 << LTDC_TWCR_TOTALW_Pos) | (10 + VFP + 272 + 2 - 1 << LTDC_TWCR_TOTALH_Pos);

	// Setup clock polarity
	LTDC->GCR &= ~(LTDC_GCR_PCPOL | LTDC_GCR_HSPOL | LTDC_GCR_VSPOL | LTDC_GCR_DEPOL);

	// Background color is black by default, don't change it

	// Setup layer 1
	LTDC_Layer1->WHPCR = ((41 + 2) << LTDC_LxWHPCR_WHSTPOS_Pos) | ((41 + 2 + 480 - 1) << LTDC_LxWHPCR_WHSPPOS_Pos); // full line
	LTDC_Layer1->WVPCR = ((10 + VFP) << LTDC_LxWVPCR_WVSTPOS_Pos) | ((10 + VFP + 272 - 1) << LTDC_LxWVPCR_WVSPPOS_Pos); // full col
	LTDC_Layer1->PFCR = 0b101; // L8 format

	// Framebuffer
	LTDC_Layer1->CFBAR = (uint32_t)&framebuffer_data[0];
	LTDC_Layer1->CFBLR = ((480 + 3) << LTDC_LxCFBLR_CFBLL_Pos) | (480 << LTDC_LxCFBLR_CFBP_Pos);
	LTDC_Layer1->CFBLNR = 272;

	// Setup the CLUT
	for (uint32_t i = 0; i < 64; ++i) {
		int r = (i & 3) << 6;
		int g = (i & (3 << 2)) << 4;
		int b = (i & (3 << 4)) << 2;

		LTDC_Layer1->CLUTWR = (i + 64*3) << 24 | (r << 16) | (r << 8) | b;
	}

	// Clear out the other parts
	for (uint32_t i = 0; i < (64*3); ++i) {
		LTDC_Layer1->CLUTWR = i << 24;
	}

	// Enable Layer 1 
	LTDC_Layer1->CR |= LTDC_LxCR_LEN | LTDC_LxCR_CLUTEN;

	// Flush registers
	LTDC->SRCR |= LTDC_SRCR_IMR;

	// Enable LTDC
	LTDC->GCR |= LTDC_GCR_LTDCEN;

	// Setup SPI1

	// Setup GPIO
	gpio_init.Pin = LL_GPIO_PIN_5|LL_GPIO_PIN_6|LL_GPIO_PIN_7;
	gpio_init.Mode = LL_GPIO_MODE_ALTERNATE;
	gpio_init.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;
	gpio_init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio_init.Pull = LL_GPIO_PULL_NO;
	gpio_init.Alternate = LL_GPIO_AF_5;
	LL_GPIO_Init(GPIOA, &gpio_init);

	// CS
	gpio_init.Pin = LL_GPIO_PIN_2;
	gpio_init.Mode = LL_GPIO_MODE_OUTPUT;
	LL_GPIO_Init(GPIOC, &gpio_init);

	// IRQ
	gpio_init.Pin = LL_GPIO_PIN_1;
	gpio_init.Mode = LL_GPIO_MODE_INPUT;
	gpio_init.Pull = LL_GPIO_PULL_UP;
	LL_GPIO_Init(GPIOC, &gpio_init);

	// Set CS inactive
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_2);

	LL_SPI_InitTypeDef spi_init = {0};
	spi_init.BaudRate = LL_SPI_BAUDRATEPRESCALER_DIV128;
	spi_init.BitOrder = LL_SPI_MSB_FIRST;
	spi_init.ClockPhase = LL_SPI_PHASE_1EDGE;
	spi_init.ClockPolarity = LL_SPI_POLARITY_LOW;
	spi_init.CRCCalculation = LL_SPI_CRCCALCULATION_DISABLE;
	spi_init.DataWidth = LL_SPI_DATAWIDTH_8BIT;
	spi_init.Mode = LL_SPI_MODE_MASTER;
	spi_init.NSS = LL_SPI_NSS_SOFT;
	spi_init.TransferDirection = LL_SPI_FULL_DUPLEX;

	LL_SPI_Init(SPI1, &spi_init);
	LL_SPI_SetStandard(SPI1, LL_SPI_PROTOCOL_MOTOROLA);
	LL_SPI_Enable(SPI1);
}

inline const uint8_t read_x = 0xD0; // S, ADDR 101, 12-bits
inline const uint8_t read_y = 0x90;
inline const uint8_t read_z = 0x90;

uint16_t read_xpt2046(uint8_t cmd) {
	// Request conversion
	LL_SPI_TransmitData8(SPI1, cmd);
	while (!LL_SPI_IsActiveFlag_TXE(SPI1)) {;}
	LL_SPI_ReceiveData8(SPI1); // ignore this data

	// Send a dummy value
	LL_SPI_TransmitData8(SPI1, 0x00);
	// read the high byte of a sample
	while (!LL_SPI_IsActiveFlag_RXNE(SPI1)) {;}
	uint8_t x_low = LL_SPI_ReceiveData8(SPI1);

	// Send a dummy value
	LL_SPI_TransmitData8(SPI1, 0x00);
	// read the low byte of a sample
	while (!LL_SPI_IsActiveFlag_RXNE(SPI1)) {;}
	uint8_t x_high = LL_SPI_ReceiveData8(SPI1);

	return (x_high << 8) | (x_low);
}

struct MedianFilter {
	int operator()(int x)
	{
		s[0] = s[1];
		s[1] = s[2];
		s[2] = s[3];
		s[3] = s[4];
		s[4] = x;

		uint16_t t[5] = {s[0], s[1], s[2], s[3], s[4]};

		cmp_swap(t[0], t[1]);
		cmp_swap(t[2], t[3]);
		cmp_swap(t[0], t[2]);
		cmp_swap(t[1], t[4]);
		cmp_swap(t[0], t[1]);
		cmp_swap(t[2], t[3]);
		cmp_swap(t[1], t[2]);
		cmp_swap(t[3], t[4]);
		cmp_swap(t[2], t[3]);

		return t[2];
	}

private:
	static inline void cmp_swap(uint16_t& a, uint16_t& b)
	{
		if (a > b)
			std::swap(a, b);
	}

	uint16_t s[5] = {0};
};

struct LCDFilter {
	using LowPassFilter = util::LowPassFilter<7, 10>;
	static const inline int P = 6;

	bool operator()(bool p, uint16_t x, uint16_t y, uint16_t& xout, uint16_t &yout) {
		if (!p) debounce = 0;
		else if (debounce < P) ++debounce;
		
		bool ok = debounce >= P;
		xout = x_l(x_m(x), !ok);
		yout = y_l(y_m(y), !ok);
		return ok;
	}
	
private:
	LowPassFilter x_l, y_l;
	MedianFilter x_m, y_m;

	int debounce = 0;
} lcd_filter;

bool lcd::poll(uint16_t &xout, uint16_t &yout) {
	// Check to make sure the IRQ pin is low
	if (LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_1)) {
		// Update the filter with a zero
		lcd_filter(false, 0, 0, xout, yout);
		return false;  // nothing is being selected
	}
	
	uint32_t avg_x = 0, avg_y = 0; // todo: pressure
	uint16_t nsamples = 0;

	// Activate the XPT2046
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_2);

	// Storage (RAW)
	uint16_t x_read[3];
	uint16_t y_read[3];
	bool     on_read[3];

	// Dummy X read
	read_xpt2046(read_x);

	for (int i = 0; i < 3; ++i) {
		y_read[i] = read_xpt2046(read_y);
		x_read[i] = read_xpt2046(read_x);
		on_read[i] = !LL_GPIO_IsInputPinSet(GPIOC, LL_GPIO_PIN_1);
	}

	// Disable CS
	LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_2);

	bool result;

	// Update filters
	for (int i = 0; i < 3; ++i) {
		result = lcd_filter(on_read[i], x_read[i], y_read[i], xout, yout);
	}

	return result;
}
