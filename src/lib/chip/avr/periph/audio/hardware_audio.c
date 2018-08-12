/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "core/rulos.h"
#include "hardware.h"
#include "core/hal.h"
#include "core/board_defs.h"

#ifdef AUDIO_REGISTER_LATCH

void hal_audio_init(void)
{
	gpio_make_output(AUDIO_REGISTER_LATCH);
	gpio_make_output(AUDIO_REGISTER_DATA);
	gpio_make_output(AUDIO_REGISTER_SHIFT);
}

#if 0
uint8_t g_audio_register_delay_constant = 1;
void audio_register_delay()
{
	uint8_t delay;
	static volatile int x;
	for (delay=0; delay<g_audio_register_delay_constant; delay++)
	{
		x = x+1;
	}
}
#endif

void hal_audio_fire_latch(void)
{
	gpio_set(AUDIO_REGISTER_LATCH);
	gpio_clr(AUDIO_REGISTER_LATCH);
}

void hal_audio_shift_sample(uint8_t sample)
{
	uint8_t i;
	for (i=0; i<8; i++)
	{
		gpio_set_or_clr(AUDIO_REGISTER_DATA, sample&1);
		gpio_set(AUDIO_REGISTER_SHIFT);
		gpio_clr(AUDIO_REGISTER_SHIFT);
		sample=sample>>1;
	}
}

#endif // AUDIO_REGISTER_LATCH
