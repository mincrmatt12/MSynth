#include <util.h>
#include <stm32f4xx.h>

void util::delay(uint32_t ms)
{
  volatile uint32_t dummyread = SysTick->CTRL;  // get rid of countflag

  while (ms)
  {
	asm volatile ("nop");
    if((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0)
    {
		--ms;
    }
  }
}
