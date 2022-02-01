/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "core/clock.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "core/logging.h"

#ifdef TIMING_DEBUG
#include "core/hardware.h"
#endif

#ifndef LOG_CLOCK_STATS
#define LOG_CLOCK_STATS 0
#endif

#ifndef SCHEDULER_NOW_QUEUE_CAPACITY
#define SCHEDULER_NOW_QUEUE_CAPACITY 4
#endif

void schedule_us_internal(Time offset_us, ActivationFuncPtr func, void *data);

// usec between timer interrupts
static Time g_rtc_interval_us;

// Clock updated by timer interrupt.  Should not be accessed without a
// lock since it is updated at interrupt time.
static volatile Time g_interrupt_driven_jiffy_clock_us;

uint32_t g_spin_counter;

extern volatile uint8_t run_scheduler_now;

typedef struct {
  Heap heap;
  ActivationRecord now_queue[SCHEDULER_NOW_QUEUE_CAPACITY];
  uint8_t now_queue_size;

#if LOG_CLOCK_STATS
  // Scheduler stats collection
  Time min_period;
  ActivationFuncPtr min_period_func;
  Time max_period;
  ActivationFuncPtr max_period_func;
  uint8_t peak_heap;
  uint8_t peak_now;
#endif
} SchedulerState_t;
static SchedulerState_t sched_state;

#if LOG_CLOCK_STATS
static void reset_stats() {
  sched_state.min_period = 0;
  sched_state.min_period_func = NULL;
  sched_state.max_period = 0;
  sched_state.max_period_func = NULL;
  sched_state.peak_heap = 0;
  sched_state.peak_now = 0;
}
#endif

void clock_log_stats() {
#if LOG_CLOCK_STATS
  LOG("peak %d scheduled, range %d (%p) to %d (%p); now peak %d",
      sched_state.peak_heap, sched_state.min_period,
      sched_state.min_period_func, sched_state.max_period,
      sched_state.max_period_func, sched_state.peak_now);

  reset_stats();
#endif
}

static void clock_handler(void *data) {
#ifdef TIMING_DEBUG
#error TIMING_DEBUG is on. Just wanted you to know.
  gpio_set(GPIO_D5);
#endif
  // NB we assume this runs in interrupt context and is hence
  // automatically atomic.
  g_interrupt_driven_jiffy_clock_us += g_rtc_interval_us;
  run_scheduler_now = TRUE;
#ifdef TIMING_DEBUG
  gpio_clr(GPIO_D5);
#endif
}

void init_clock(Time interval_us, uint8_t timer_id) {
  heap_init(&sched_state.heap);
  sched_state.now_queue_size = 0;

#if LOG_CLOCK_STATS
  reset_stats();
#endif

#ifdef TIMING_DEBUG
  gpio_make_output(GPIO_D4);
  gpio_make_output(GPIO_D5);
  gpio_make_output(GPIO_D6);
#endif
  // Initialize the clock to 20 seconds before rollover time so that
  // rollover bugs happen quickly during testing
  g_interrupt_driven_jiffy_clock_us = (Time)INT32_MAX - 20000000;
  g_rtc_interval_us =
      hal_start_clock_us(interval_us, clock_handler, NULL, timer_id);
  g_spin_counter = 0;
}

void schedule_us(Time offset_us, ActivationFuncPtr func, void *data) {
  // warning: scheduling something for "now" will re-run the scheduler
  // immediately, which may not be what you want
  assert(offset_us >= 0);
  schedule_us_internal(offset_us, func, data);
}

void scheduler_insert(Time key, ActivationFuncPtr func, void *data) {
#if LOG_CLOCK_STATS
  uint8_t heap_count =
#endif
      heap_insert(&sched_state.heap, key, func, data);

#if LOG_CLOCK_STATS
  if (heap_count > sched_state.peak_heap) {
    sched_state.peak_heap = heap_count;
  }
#endif
}

void schedule_now(ActivationFuncPtr func, void *data) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  if (sched_state.now_queue_size < SCHEDULER_NOW_QUEUE_CAPACITY) {
    sched_state.now_queue[sched_state.now_queue_size].func = func;
    sched_state.now_queue[sched_state.now_queue_size].data = data;
    sched_state.now_queue_size++;

#if LOG_CLOCK_STATS
    if (sched_state.now_queue_size > sched_state.peak_now) {
      sched_state.peak_now = sched_state.now_queue_size;
    }
#endif
  } else {
    scheduler_insert(clock_time_us(), func, data);
  }
  hal_end_atomic(old_interrupts);
  run_scheduler_now = TRUE;
}

void schedule_us_internal(Time offset_us, ActivationFuncPtr func, void *data) {
#if LOG_CLOCK_STATS
  if (sched_state.min_period == 0 || sched_state.min_period > offset_us) {
    sched_state.min_period = offset_us;
    sched_state.min_period_func = func;
  }
  if (sched_state.max_period < offset_us) {
    sched_state.max_period = offset_us;
    sched_state.max_period_func = func;
  }
#endif

  schedule_absolute(clock_time_us() + offset_us, func, data);
}

void schedule_absolute(Time at_time, ActivationFuncPtr func, void *data) {
  // LOG("scheduling act %08x func %08x", (int) act, (int) act->func);

#ifdef TIMING_DEBUG
  gpio_set(GPIO_D6);
#endif
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  scheduler_insert(at_time, func, data);
  hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
  gpio_clr(GPIO_D6);
#endif
}

// the cheap but less precise way to get time -- returns the jiffy clock
Time clock_time_us() {
  Time retval;
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  retval = g_interrupt_driven_jiffy_clock_us;
  hal_end_atomic(old_interrupts);
  return retval;
}

// this is the expensive one, with a lock
Time precise_clock_time_us() {
  // Getting precise time is tricky because we have to read the hardware
  // timer. If it rolled over recently, an interrupt to increment the jiffy
  // timer might be pending.
  //
  // We can check if there's an interrupt pending. But we have to get the
  // hardware timer value twice: both before and after checking if there's an
  // interrupt pending. If there is an interrupt pending, we use the post-check
  // value: it is guaranteed to be post-rollover and we need to add an extra
  // jiffy. If none is pending, we use the pre-check value; it's guaranteed to
  // be pre-rollover.
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  uint16_t milliintervals_precheck = hal_elapsed_milliintervals();
  bool int_pending = hal_clock_interrupt_is_pending();
  uint16_t milliintervals_postcheck = hal_elapsed_milliintervals();
  Time t = g_interrupt_driven_jiffy_clock_us;
  hal_end_atomic(old_interrupts);

  if (int_pending) {
    t += (g_rtc_interval_us * (1000 + milliintervals_postcheck)) / 1000;
  } else {
    t += (g_rtc_interval_us * milliintervals_precheck) / 1000;
  }
  return t;
}

void spin_counter_increment() {
  g_spin_counter += 1;
}

uint32_t read_spin_counter() {
  return g_spin_counter;
}

void scheduler_run_once() {
  Time now = clock_time_us();

  while (1)  // run until nothing to do for this time
  {
    ActivationRecord act;

    bool valid = FALSE;

#ifdef TIMING_DEBUG
    gpio_set(GPIO_D6);
#endif
    rulos_irq_state_t old_interrupts = hal_start_atomic();
    if (sched_state.now_queue_size > 0) {
      act = sched_state.now_queue[0];
      sched_state.now_queue_size -= 1;
      memmove(&sched_state.now_queue[0], &sched_state.now_queue[1],
              sizeof(sched_state.now_queue[0])* sched_state.now_queue_size);
      valid = TRUE;
    } else {
      Time due_time;
      int rc = heap_peek(&sched_state.heap, &due_time, &act);
      if (!rc && !later_than(due_time, now)) {
        valid = TRUE;
        heap_pop(&sched_state.heap);
      }
    }
    hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
    gpio_clr(GPIO_D6);
#endif

    if (!valid) {
      break;
    }

    // LOG("popping act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);

    act.func(act.data);
    // LOG("returned act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);
  }
}
