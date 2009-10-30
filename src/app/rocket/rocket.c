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
#include "hal.h"
#include "cpumon.h"
#include "idle_display.h"
#include "sequencer.h"
#include "rasters.h"
#include "pong.h"
#include "lunar_distance.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	FocusManager fa;
	focus_init(&fa);

	InputControllerAct ia;
	input_controller_init(&ia, (UIEventHandler*) &fa);

	DRTCAct dr;
	drtc_init(&dr, 0, clock_time_us()+20000000);

	DScrollMsgAct thruster_actuation_placeholder;
	dscrlmsg_init(&thruster_actuation_placeholder, 7, " -29  73", 0);

/*
	LabeledDisplayHandler ldh;
	labeled_display_init(&ldh, 0, &fa);

	DScrollMsgAct da1;
	dscrlmsg_init(&da1, 0, "x", 0);	// overwritten by idle display
	
	IdleDisplayAct idisp;
	idle_display_init(&idisp, &da1, &cpumon);
	*/

/*
	char buf[50], *p;
	strcpy(buf, "calib_spin ");
	p = buf+strlen(buf);
	p+=int_to_string2(p, 0, 0, cpumon.calibration_spin_counts);
	strcpy(p, " interval ");
	p = buf+strlen(buf);
	p+=int_to_string2(p, 0, 0, cpumon.calibration_interval);
	strcpy(p, "  ");

	DScrollMsgAct da2;
	dscrlmsg_init(&da2, 3, "This is a test sequence with original scroll. ", 100);
	*/

/*

	// scroll our ascii set.
	DScrollMsgAct da2;
	char buf[129-32];
	{
		int i;
		for (i=0; i<128-32; i++)
		{
			buf[i] = i+32;
		}
		buf[i] = '\0';
	}
	dscrlmsg_init(&da2, 3, buf, 200);

	NumericInputAct ni;
	BoardBuffer bbuf;
	board_buffer_init(&bbuf);
	board_buffer_push(&bbuf, 4);
	RowRegion region = { &bbuf, 3, 4 };

	numeric_input_init(&ni, region, NULL, &fa, "numeric");

	DCompassAct dc;
	dcompass_init(&dc, 4, &fa);


*/
#if !MCUatmega8

/*
	DDockAct ddock;
	ddock_init(&ddock, 0, &fa);

	RasterBigDigit rdigit;
	raster_big_digit_init(&rdigit, 2);
	Pong pong;
	pong_init(&pong, 2, &fa);
*/
	DGratuitousGraph dgg;
	dgg_init(&dgg, 2, "pres", 1000000);

	Calculator calc;
	calculator_init(&calc, 5, &fa);
#endif

/*
	Launch launch;
	launch_init(&launch, 4, &fa);
*/

	LunarDistance ld;
	lunar_distance_init(&ld, 3, 4);


	cpumon_main_loop();

	return 0;
}

