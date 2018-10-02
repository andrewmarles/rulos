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

#define TAIL_ON_TIME_MS  100
#define TAIL_OFF_TIME_MS 300
#define WHEEL_PERIOD_MS  10
#define ANIM_TIME_SEC    5

/////////////////////////////////////////////////////////////////

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <avr/power.h>

#include "chip/avr/periph/usi_twi_master/usi_twi_master.h"
#include "core/clock.h"
#include "core/rulos.h"
#include "core/util.h"
#include "hardware.h"

#define BUTTON GPIO_B2
#define TAILLIGHT GPIO_B1

#define L_WHEEL_ADDR 0b1110100
#define R_WHEEL_ADDR 0b1110111

#define JIFFY_TIME_US 2500

typedef struct {
  // timeout
  Time shutoff_time;

  // wheels
  uint8_t light_on_l;
  uint8_t light_on_r;
  Time wheels_next_move_time;
  
  // taillight
  r_bool tail_on;
  Time tail_next_toggle_time;
} BikeState_t;

BikeState_t bike;

void bike_sleep();
void bike_wake(BikeState_t* bike);

ISR(INT0_vect) {
  bike_wake(&bike);
}

static void init_led_drivers(uint8_t sevbit_addr)
{
  char buf[17];

  buf[0] = 0x10; // Set address to 0x10, first PWM register.
  for (int i = 1; i <= 16; i++) {
    buf[i] = 0xff;
  }
  usi_twi_master_send(sevbit_addr, buf, 17);

  buf[0] = 0xB0; // Set address to 0xB0, "take PWM settings" register
  buf[1] = 0;
  usi_twi_master_send(sevbit_addr, buf, 2);
}

static void shutdown_led_driver(uint8_t sevbit_addr)
{
  char buf[4];

  buf[0] = 0;  // start address is address 0
  buf[1] = 0b10000000;  // register 0: set shutdown bit
  buf[2] = 0;  // register 1: no LEDs on
  buf[3] = 0;  // register 2: no LEDs on

  usi_twi_master_send(sevbit_addr, buf, 4);
}

static void shift_in_one_config(uint8_t sevbit_addr, uint8_t led_number)
{
  char buf[4];

  buf[0] = 0;  // start address is address 0
  buf[1] = 0;  // address 0: data 0

  if (led_number < 8) {
    buf[2] = 0;
    buf[3] = 1 << led_number;
  } else {
    buf[2] = 1 << (led_number - 8);
    buf[3] = 0;
  }

  usi_twi_master_send(sevbit_addr, buf, 4);
}

static void shift_in_config(BikeState_t* bike)
{
  shift_in_one_config(L_WHEEL_ADDR, bike->light_on_l);
  shift_in_one_config(R_WHEEL_ADDR, bike->light_on_r);
}

static void bike_update(BikeState_t* bike) {
  schedule_us(JIFFY_TIME_US, (ActivationFuncPtr)bike_update, bike);
  Time now = clock_time_us();

#if 0
  if (gpio_is_clr(BUTTON)) {
    bike_wake(bike);
  }
#endif
  
  // If we've reached the end of the animation time, shut everything down.
  if (later_than(now, bike->shutoff_time)) {
    bike_sleep();
    return;
  }
  
  // If we've reached the next blink state for the tail light, toggle it
  if (later_than(now, bike->tail_next_toggle_time)) {
    if (bike->tail_on) {
      gpio_clr(TAILLIGHT);
      bike->tail_on = false;
      bike->tail_next_toggle_time = now + (TAIL_OFF_TIME_MS * (uint32_t)1000);
    } else {
      gpio_set(TAILLIGHT);
      bike->tail_on = true;
      bike->tail_next_toggle_time = now + (TAIL_ON_TIME_MS * (uint32_t)1000);
    }
  }

  // If we've reached the next update time for the bicycle lights, update them
  if (later_than(now, bike->wheels_next_move_time)) {
    bike->light_on_l = (bike->light_on_l + 1) % 16;
    bike->light_on_r = (bike->light_on_r + 1) % 16;
    shift_in_config(bike);
    bike->wheels_next_move_time = now + (WHEEL_PERIOD_MS * (uint32_t)1000);
  }
}

void bike_wake(BikeState_t* bike) {
  Time now = clock_time_us();
  bike->light_on_l = 0;
  bike->light_on_r = 0;
  bike->wheels_next_move_time = now;
  bike->tail_on = false;
  bike->tail_next_toggle_time = now;
  bike->shutoff_time = now + time_sec(ANIM_TIME_SEC);

  // Set up USI.
  power_usi_enable();
  usi_twi_master_init();
  
  // Initialize LED drivers.
  init_led_drivers(L_WHEEL_ADDR);
  init_led_drivers(R_WHEEL_ADDR);

  // set up output pins as drivers
  gpio_make_output(TAILLIGHT);
}

void bike_sleep() {
  // Shut down LED drivers
  shutdown_led_driver(L_WHEEL_ADDR);
  shutdown_led_driver(R_WHEEL_ADDR);

  // Disable USI
  USICR = 0;

  // Set ports as inputs
  DDRA = 0;
  DDRB = 0;

  hal_deep_sleep();
}

int main() {
  hal_init();

  init_clock(JIFFY_TIME_US, TIMER1);

  gpio_make_input_enable_pullup(BUTTON);

  // Configure INT0 to generate interrupts at low level.
  MCUCR &= ~(_BV(ISC01) | _BV(ISC00));

  // Configure INT0 to generate interrupts.
  GIMSK |= _BV(INT0);
  sei();

  // Init bike state.
  memset(&bike, 0, sizeof(bike));
  bike_wake(&bike);

  // Set up periodic update.
  schedule_now((ActivationFuncPtr)bike_update, &bike);

  // Loop forever.
  cpumon_main_loop();
  assert(FALSE);
}
