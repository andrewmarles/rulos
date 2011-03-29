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
#include <stdbool.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "network.h"
#include "sim.h"
#include "serial_console.h"


#if !SIM
#include "hardware.h"
#endif // !SIM

//////////////////////////////////////////////////////////////////////////////

// output appears on pin PC0. (other PC pins are divided by 2^k)
// Use it to switch the gate of a mosfet.
// (The computer sends 2V; a transistor might work, too.)

// Invocation arguments
#define CUR_MILES			26.9
#define GOAL_MILES			(96+274)

// Configuration parameters
#define CM_PER_REVOLUTION	(216) /* wheel diameter in cm */
// Measurement indicates that 69mph is about as fast as the
// calculator can tolerate.
#define MPH		(69)

// Math.
#define CM_PER_MILE	(2.54*12*5280)
#define REV_PER_MILE (CM_PER_MILE/CM_PER_REVOLUTION)

#define	HR_PER_MILE	(1.0/MPH)
#define	HR_PER_REV	(HR_PER_MILE/REV_PER_MILE)
#define	SEC_PER_REV	(HR_PER_REV*3600)
#define REV_PERIOD	(1000000*SEC_PER_REV)
#define REV_HALF_PERIOD	(REV_PERIOD/2)

#define RUN_MILES	(GOAL_MILES-CUR_MILES)
#define RUN_REVS	RUN_MILES*REV_PER_MILE

//////////////////////////////////////////////////////////////////////////////

SerialConsole *g_console;

#define SYNCDEBUG()	syncdebug(0, 'T', __LINE__)
void syncdebug(uint8_t spaces, char f, uint16_t line)
{
	char buf[32], hexbuf[6];
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
	serial_console_sync_send(g_console, buf, strlen(buf));
}

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint8_t val;
	Time period;
	uint32_t revs_remaining;
} BlinkAct;

void _update_blink(BlinkAct *ba)
{
	if (ba->revs_remaining>0)
	{
		schedule_us(ba->period, (ActivationFuncPtr) _update_blink, ba);
	}

	ba->val += 1;
#if !SIM
	PORTC = ba->val;
#endif
	if (ba->val & 1)
	{
		ba->revs_remaining -= 1;
	}
}

void blink_init(BlinkAct *ba)
{
	ba->val = 0;
	ba->period = REV_HALF_PERIOD;
	ba->revs_remaining = RUN_REVS;

#if !SIM
	gpio_make_output(GPIO_C0);
	gpio_make_output(GPIO_C1);
	gpio_make_output(GPIO_C2);
	gpio_make_output(GPIO_C3);
	gpio_make_output(GPIO_C4);
	gpio_make_output(GPIO_C5);
	gpio_make_output(GPIO_C6);
	gpio_make_output(GPIO_C7);
#endif // !SIM

	schedule_us(100000, (ActivationFuncPtr) _update_blink, ba);
}

//////////////////////////////////////////////////////////////////////////////
typedef struct {
	SerialConsole console;
	BlinkAct ba;
} Shell;

void shell_func(Shell *shell);
void print_func(Shell *shell);

void shell_init(Shell *shell)
{
	serial_console_init(&shell->console, (ActivationFuncPtr) shell_func, shell);
	g_console = &shell->console;
	print_func(shell);
	blink_init(&shell->ba);
	SYNCDEBUG();
}

void print32(uint32_t v)
{
	syncdebug(2, 'h', (v)>>16);
	syncdebug(6, 'l', (v)&0xffff);
}

void print_func(Shell *shell)
{
	serial_console_sync_send(&shell->console, "period\n", 7);
	print32(shell->ba.period);
	serial_console_sync_send(&shell->console, "revs_remaining\n", 14);
	print32(shell->ba.revs_remaining);
	serial_console_sync_send(&shell->console, "REV_PER_MILE\n", 13);
	print32(REV_PER_MILE);
	serial_console_sync_send(&shell->console, "REV_PERIOD\n", 11);
	print32(REV_PERIOD);
	serial_console_sync_send(&shell->console, "REV_HALF_PERIOD\n", 16);
	print32(REV_HALF_PERIOD);
}

void shell_func(Shell *shell)
{
	char *line = shell->console.line;

	if (strncmp(line, "per ", 4)==0)
	{
		SYNCDEBUG();
		shell->ba.period = atoi_hex(&line[4]);
	}
	else if (strcmp(line, "print\n")==0)
	{
		print_func(shell);
	}
	line++;
	serial_console_sync_send(&shell->console, "OK\n", 3);
}

//////////////////////////////////////////////////////////////////////////////

int main()
{

	hal_init();
	init_clock(1000, TIMER1);

	Shell shell;
	shell_init(&shell);
		// syncdebug (uart) ready here.

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase
	cpumon_main_loop();
	while (1) { }

	return 0;
}

