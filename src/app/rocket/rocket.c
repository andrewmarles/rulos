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
#include "display_controller.h"
#include "display_rtc.h"
#include "display_scroll_msg.h"
#include "display_compass.h"
#include "focus.h"
#include "labeled_display.h"
#include "display_docking.h"
#include "display_gratuitous_graph.h"
#include "numeric_input.h"
#include "input_controller.h"
#include "calculator.h"
#include "display_aer.h"
#include "hal.h"
#include "cpumon.h"
#include "idle_display.h"
#include "sequencer.h"
#include "rasters.h"
#include "pong.h"
#include "lunar_distance.h"
#include "sim.h"
#include "display_thrusters.h"
#include "network.h"
#include "remote_keyboard.h"
#include "remote_bbuf.h"
#include "remote_uie.h"
#include "control_panel.h"
#include "autotype.h"
#include "idle.h"
#include "hobbs.h"
#include "screenblanker.h"
#include "slow_boot.h"
#include "potsticker.h"
#include "volume_control.h"
#include "bss_canary.h"


/************************************************************************************/
/************************************************************************************/

typedef struct {
	DRTCAct dr;
	Network network;
	LunarDistance ld;
	AudioClient audio_client;
	ThrusterUpdate *thrusterUpdate[4];
	HPAM hpam;
	ControlPanel cp;
	InputPollerAct ip;
	RemoteKeyboardRecv rkr;
	ThrusterSendNetwork tsn;
	ThrusterState_t ts;
	IdleAct idle;
	Hobbs hobbs;
	ScreenBlanker screenblanker;
	ScreenBlankerSender screenblanker_sender;
	SlowBoot slow_boot;
	PotSticker potsticker;
	VolumeControl volume_control;
	BSSCanary bss_canary;
} Rocket0;

#define THRUSTER_X_CHAN	3

#if defined(BOARD_PCB10)
#define THRUSTER_Y_CHAN	4
#elif defined(BOARD_PCB11) || defined(BOARD_LPEM)
#define THRUSTER_Y_CHAN	2
#elif SIM
#define THRUSTER_Y_CHAN	2
#else
# error Thruster y chan not defined for this board
#endif

#define POTSTICKER_CHANNEL 0
#define VOLUME_POT_CHANNEL 1
//#define USE_LOCAL_KEYPAD

void init_rocket0(Rocket0 *r0)
{
	drtc_init(&r0->dr, 0, clock_time_us()+20000000);
	init_twi_network(&r0->network, 100, ROCKET_ADDR);
	lunar_distance_init(&r0->ld, 1, 2 /*, SPEED_POT_CHANNEL*/);
	init_audio_client(&r0->audio_client, &r0->network);
	memset(&r0->thrusterUpdate, 0, sizeof(r0->thrusterUpdate));
	init_hpam(&r0->hpam, 7, r0->thrusterUpdate);
	init_idle(&r0->idle);
	thrusters_init(&r0->ts, 7, THRUSTER_X_CHAN, THRUSTER_Y_CHAN, &r0->hpam, &r0->idle);
	//r0->thrusterUpdate[2] = (ThrusterUpdate*) &r0->idle.thrusterListener;

	init_screenblanker(&r0->screenblanker, bc_rocket0, &r0->hpam, &r0->idle);
	init_screenblanker_sender(&r0->screenblanker_sender, &r0->network);
	r0->screenblanker.screenblanker_sender = &r0->screenblanker_sender;

	init_control_panel(&r0->cp, 3, 1, &r0->network, &r0->hpam, &r0->audio_client, &r0->idle, &r0->screenblanker, &r0->ts.joystick_state);
	r0->cp.ccl.launch.main_rtc = &r0->dr;
	r0->cp.ccl.launch.lunar_distance = &r0->ld;

	// Local input poller
#ifdef USE_LOCAL_KEYPAD
	input_poller_init(&r0->ip, (InputInjectorIfc*) &r0->cp.direct_injector);
#endif

	// Remote receiver
	init_remote_keyboard_recv(&r0->rkr, &r0->network, (InputInjectorIfc*) &r0->cp.direct_injector, REMOTE_KEYBOARD_PORT);

	r0->thrusterUpdate[1] = (ThrusterUpdate*) &r0->cp.ccdock.dock.thrusterUpdate;
	init_thruster_send_network(&r0->tsn, &r0->network);
	r0->thrusterUpdate[0] = (ThrusterUpdate*) &r0->tsn;

	init_hobbs(&r0->hobbs, &r0->hpam, &r0->idle);

	init_slow_boot(&r0->slow_boot, &r0->screenblanker, &r0->audio_client);

	init_potsticker(&r0->potsticker,
		POTSTICKER_CHANNEL,
		(InputInjectorIfc*) &r0->cp.direct_injector,
		9,
		'p',
		'q');

	volume_control_init(&r0->volume_control, &r0->audio_client, VOLUME_POT_CHANNEL, /*board*/ 0);

	bss_canary_init(&r0->bss_canary);
}

static Rocket0 rocket0;	// allocate obj in .bss so it's easy to count

int main()
{
	hal_init();
	hal_init_rocketpanel(bc_rocket0);
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	board_buffer_module_init();

	init_rocket0(&rocket0);

#define DEBUG_IDLE_BUSY 0
#if DEBUG_IDLE_BUSY
#endif // DEBUG_IDLE_BUSY

/*
	Autotype autotype;
	init_autotype(&autotype, (InputInjectorIfc*) &rocket0.cp.direct_injector,
		//"000aaaaac004671c",
		"000bc",
		(Time) 1300000);
*/

#if MEASURE_CPU_FOR_RULOS_PAPER
	DScrollMsgAct dsm;
	dscrlmsg_init(&dsm, 2, "bong", 100);
	IdleDisplayAct idle;
	idle_display_init(&idle, &dsm, &cpumon);
#endif // MEASURE_CPU_FOR_RULOS_PAPER

	cpumon_main_loop();

	return 0;
}

