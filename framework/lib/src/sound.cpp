#include <sound.h>
#include <stm32f4xx_ll_dma.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_spi.h>

void sound::init() {
	// Enable clocks
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2); // I2S2
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOI);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

	// Setup GPIO
	{
		LL_GPIO_InitTypeDef init = {0};
		// common
		init.Alternate = LL_GPIO_AF_5; // SPI2
		init.Mode = LL_GPIO_MODE_ALTERNATE;
		init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		init.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;

		init.Pin = LL_GPIO_PIN_12 | LL_GPIO_PIN_13;
		LL_GPIO_Init(GPIOB, &init);

		init.Pin = LL_GPIO_PIN_6;
		LL_GPIO_Init(GPIOC, &init);

		init.Pin = LL_GPIO_PIN_3;
		LL_GPIO_Init(GPIOI, &init);

		// Setup mute enable/disable
		init.Mode = LL_GPIO_MODE_OUTPUT;
		init.Speed = LL_GPIO_SPEED_FREQ_LOW;
		init.Pin = LL_GPIO_PIN_9;
		LL_GPIO_Init(GPIOG, &init);
	}

	// Setup I2S
	{
		LL_I2S_InitTypeDef init = {0};

		init.AudioFreq = 44100U; // 44.1 kHz
		init.ClockPolarity = LL_I2S_POLARITY_LOW;
		init.DataFormat = LL_I2S_DATAFORMAT_16B_EXTENDED;
		init.Mode = LL_I2S_MODE_MASTER_TX;
		init.MCLKOutput = LL_I2S_MCLK_OUTPUT_ENABLE;
		init.Standard = LL_I2S_STANDARD_PHILIPS;

		LL_I2S_Init(SPI2, &init);

		// Enable interrupts & DMA
		LL_I2S_EnableIT_TXE(SPI2);
		LL_I2S_EnableDMAReq_TX(SPI2);
	}

	// Setup DMA
	{
		LL_DMA_InitTypeDef init;

		// Setup basic parameters
		LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_4, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
		LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_4, LL_DMA_PDATAALIGN_HALFWORD);
		LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_4, LL_DMA_PERIPH_NOINCREMENT);
		LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_4, (uint32_t)&SPI2->DR);
		LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_4, LL_DMA_PRIORITY_HIGH);
		
		LL_DMA_SetChannelSelection(DMA1, LL_DMA_STREAM_4, LL_DMA_CHANNEL_0);
		LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_4, LL_DMA_MEMORY_INCREMENT);
	}

	// Enable I2S
	LL_I2S_Enable(SPI2);
}

void sound::set_mute(bool mute) {
	(mute ? LL_GPIO_SetOutputPin : LL_GPIO_ResetOutputPin)(GPIOG, LL_GPIO_PIN_9);
}

void sound::single_shot(uint16_t *audio_data, uint32_t nsamples) {
	// Setup DMA
	LL_DMA_SetMode(DMA1, LL_DMA_STREAM_4, LL_DMA_MODE_NORMAL);
	LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_4, LL_DMA_MDATAALIGN_HALFWORD);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_4, (uint32_t)audio_data);
	LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_4, nsamples*2); // left and right channels

	// Start sending data
	LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_4);
}

bool sound::finished_sending() {
	return !LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_4);
}

void sound::stop_output() {
	LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_4);
	LL_DMA_DisableIT_TC(DMA1, LL_DMA_STREAM_4);
	LL_DMA_DisableIT_TE(DMA1, LL_DMA_STREAM_4);
}

void sound::continuous_sample(uint16_t *audio_data, uint32_t nsamples) {
	// Setup DMA
	LL_DMA_SetMode(DMA1, LL_DMA_STREAM_4, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_4, LL_DMA_MDATAALIGN_HALFWORD);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_4, (uint32_t)audio_data);
	LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_4, nsamples*2); // left and right channels

	// Start sending data
	LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_4);
}
