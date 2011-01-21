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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "network.h"
#include "sim.h"
#include "audio_driver.h"
#include "audio_server.h"
#include "audio_streamer.h"
#include "sdcard.h"
#include "serial_console.h"

//////////////////////////////////////////////////////////////////////////////
void audioled_init();
void audioled_set(r_bool red, r_bool yellow);
//////////////////////////////////////////////////////////////////////////////

SerialConsole *g_serial_console = NULL;


#define SYNCDEBUG()	syncdebug(0, 'A', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line)
{
	char buf[16], hexbuf[6];
	int i;
	for (i=0; i<spaces; i++)
	{
		buf[i] = ' ';
	}
	buf[i++] = f;
	buf[i++] = ':';
	buf[i++] = '\0';
	if (f >= 'a')	// lowercase -> hex value; so sue me
	{
		debug_itoha(hexbuf, line);
	}
	else
	{
		itoda(hexbuf, line);
	}
	strcat(buf, hexbuf);
	strcat(buf, "\n");
	serial_console_sync_send(g_serial_console, buf, strlen(buf));
}

void syncdebug32(uint8_t spaces, char f, uint32_t line)
{
	syncdebug(spaces, f, line>>16);
	syncdebug(spaces, '~', line&0x0ffff);
}

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

#define AUDIO_LED_RED		GPIO_D2
#define AUDIO_LED_YELLOW	GPIO_D3

#ifndef SIM
#include "hardware.h"
#endif // SIM


void audioled_init()
{
#ifndef SIM
	gpio_make_output(AUDIO_LED_RED);
	gpio_make_output(AUDIO_LED_YELLOW);
#endif // SIM
}

void audioled_set(r_bool red, r_bool yellow)
{
#ifndef SIM
	gpio_set_or_clr(AUDIO_LED_RED, !red);
	gpio_set_or_clr(AUDIO_LED_YELLOW, !yellow);
#endif // SIM
}

//////////////////////////////////////////////////////////////////////////////


struct s_CmdProc;

typedef struct s_CmdProc {
	Activation act;
	SerialConsole sca;
	Network *network;
	AudioServer *audio_server;
	CpumonAct cpumon;
} CmdProc;

void cmdproc_update(Activation *act)
{
	CmdProc *cp = (CmdProc *) act;
	char *buf = cp->sca.line;

	SYNCDEBUG();
#if !SIM
	syncdebug(4, 'd', (uint16_t) (&cp->audio_server->audio_streamer.sdc));
#endif

	if (strcmp(buf, "init\n")==0)
	{
		init_audio_server(cp->audio_server, cp->network, TIMER2);
	}
	else if (strcmp(buf, "fetch\n")==0)
	{
		_aserv_fetch_start(&cp->audio_server->fetch_start);
	}
	else if (strncmp(buf, "play ", 5)==0)
	{
		SYNCDEBUG();
		SoundCmd skip = {(buf[5]-'b')};
		SoundCmd loop = {(buf[6]-'b')};
		syncdebug(0, 's', skip.token);
		syncdebug(0, 'l', loop.token);
		_aserv_skip_to_clip(cp->audio_server, skip, loop);
	}
	else if (strncmp(buf, "vol ", 4)==0)
	{
		uint8_t v = atoi_hex(&buf[4]);
		as_set_music_volume(&cp->audio_server->audio_streamer, v);
		syncdebug(0, 'V', v);
	}
	else if (strncmp(buf, "spiact", 4)==0)
	{
		syncdebug(3, 'a', (int) (cp->audio_server->audio_streamer.sdc.spi.spiact.act.func)<<1);
	}
	else if (strncmp(buf, "idle", 4)==0)
	{
		syncdebug(0, 'I', cpumon_get_idle_percentage(&cp->cpumon));
	}
	else if (strcmp(buf, "led on\n")==0)
	{
		audioled_set(1, 1);
	}
	else if (strcmp(buf, "led off\n")==0)
	{
		audioled_set(0, 0);
	}
	else
	{
		char reply_buf[80];
		strcpy(reply_buf, "error: \"");
		strcat(reply_buf, buf);
		strcat(reply_buf, "\"\n");
		serial_console_sync_send(&cp->sca, reply_buf, strlen(reply_buf));
	}
}

void cmdproc_init(CmdProc *cp, AudioServer *audio_server, Network *network)
{
	cp->audio_server = audio_server;
	cp->network = network;
	serial_console_init(&cp->sca, &cp->act);
	g_serial_console = &cp->sca;
	SYNCDEBUG();
	cp->act.func = cmdproc_update;
	SYNCDEBUG();
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} BlinkAct;

void _update_blink(Activation *act)
{
	BlinkAct *ba = (BlinkAct *) act;
	ba->val = !ba->val;
	audioled_set(ba->val, 0);
#ifndef SIM
//	gpio_set_or_clr(AUDIO_LED_RED, !ba->val);
#endif //!SIM
	schedule_us(1000000, &ba->act);
}

void blink_init(BlinkAct *ba)
{
	ba->act.func = _update_blink;
	schedule_us(1000000, &ba->act);
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} DACTest;

void _dt_update(Activation *act);

void dt_init(DACTest *dt)
{
	dt->act.func = _dt_update;
	dt->val = 0;
	hal_audio_init();
	schedule_us(1, &dt->act);
}

void _dt_update(Activation *act)
{
	DACTest *dt = (DACTest *) act;
	hal_audio_fire_latch();
	audioled_set((dt->val & 0x80)!=0, (dt->val & 0x40)!=0);
	if (dt->val==0) { dt->val = 128; }
	else if (dt->val==128) { dt->val = 255; }
	else { dt->val = 0; }
	schedule_us(1000000, &dt->act);
	hal_audio_shift_sample(dt->val);
}


//////////////////////////////////////////////////////////////////////////////

typedef struct {
	AudioServer aserv;
	Network network;
	CmdProc cmdproc;
} MainContext;
MainContext mc;

int main()
{
	audioled_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

	audioled_init();

	audioled_set(0, 0);

#if 1
	// needs to be early, because it initializes uart, which at the
	// moment I'm using for SYNCDEBUG(), including in init_audio_server.
	cmdproc_init(&mc.cmdproc, &mc.aserv, &mc.network);

	init_twi_network(&mc.network, AUDIO_ADDR);

	init_audio_server(&mc.aserv, &mc.network, TIMER2);
#else
	DACTest dt;
	dt_init(&dt);
#endif

	BlinkAct ba;
	blink_init(&ba);

	cpumon_init(&mc.cmdproc.cpumon);	// includes slow calibration phase


#if 0
	board_buffer_module_init();
#endif
	cpumon_main_loop();

	return 0;
}

