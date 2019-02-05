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

#include "core/rulos.h"
#include "periph/7seg_panel/7seg_panel.h"
#include "periph/display_rtc/display_rtc.h"
#include "periph/input_controller/input_controller.h"

typedef struct {
  Time time;
  const char *msg;
} BooRec;
BooRec boos[] = {
    {5000000, ""},
    {0250000, "  boo!"},
    {5000000, ""},
    {1000000, "Come see"},
    {1000000, "our new "},
    {1000000, " rocket!"},
    {0, NULL},
};
BooRec *booPtr = boos;
BoardBuffer bbuf;

void boofunc(void *f) {
  booPtr += 1;
  if (booPtr->msg == NULL) {
    booPtr = boos;
  }
  memset(bbuf.buffer, 0, 8);
  ascii_to_bitmap_str(bbuf.buffer, 8, booPtr->msg);
  LOG("msg: %s", booPtr->msg);
  schedule_us(booPtr->time, (ActivationFuncPtr)boofunc, NULL);
  board_buffer_draw(&bbuf);
}

/************************************************************************************/
/************************************************************************************/

int main() {
  hal_init();
  hal_init_rocketpanel();
  init_clock(10000, TIMER1);

  CpumonAct cpumon;
  cpumon_init(&cpumon);  // includes slow calibration phase

  // install_handler(ADC, adc_handler);

  board_buffer_module_init();

  /*
          FocusManager fa;
          focus_init(&fa);
  */

  InputPollerAct ia;
  input_poller_init(&ia, NULL);

  board_buffer_init(&bbuf DBG_BBUF_LABEL("boo"));
  board_buffer_push(&bbuf, 0);
  schedule_us(1, boofunc, NULL);

  DRTCAct dr;
  drtc_init(&dr, 1, clock_time_us() + 20000000);

  cpumon_main_loop();

  return 0;
}
