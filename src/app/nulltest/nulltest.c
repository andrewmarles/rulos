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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/bss_canary/bss_canary.h"

#define FREQ_USEC 50000

#if defined(RULOS_ARM_LPC)
#define TEST_PIN GPIO0_08
#elif defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A8
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

void test_func(void *data) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);

  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);
}

HalUart uart;

int main() {
  hal_init();

  hal_uart_init(&uart, 38400, true, /* uart_id= */ 0);
  for (int i = 0; i < 255; i++) {
    LOG("Log output running %d", i);
  }

  init_clock(10000, TIMER1);
  gpio_make_output(TEST_PIN);

  bss_canary_init();

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
