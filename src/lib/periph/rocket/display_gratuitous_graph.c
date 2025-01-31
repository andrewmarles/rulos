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

#include "periph/rocket/display_gratuitous_graph.h"

#include "periph/rocket/rocket.h"

void dgg_update(DGratuitousGraph *act);
void draw_skinny_bars(DGratuitousGraph *dgg, DriftAnim *da, SSBitmap bm);
void draw_fat_bars(DGratuitousGraph *dgg, DriftAnim *da, SSBitmap bm0,
                   SSBitmap bm1);

void dgg_init(DGratuitousGraph *dgg, uint8_t board, char *name,
              Time impulse_frequency_us) {
  board_buffer_init(&dgg->bbuf DBG_BBUF_LABEL("dgg"));
  board_buffer_push(&dgg->bbuf, board);
  int i;
  for (i = 0; i < 3; i++) {
    drift_anim_init(&dgg->drift[i], 10, 4, 0, 16, 5);
  }
  dgg->name = name;
  dgg->impulse_frequency_us = impulse_frequency_us;
  dgg->last_impulse = 0;
  schedule_us(1, (ActivationFuncPtr)dgg_update, dgg);
}

void dgg_update(DGratuitousGraph *dgg) {
  schedule_us(Exp2Time(16), (ActivationFuncPtr)dgg_update, dgg);
  Time t = clock_time_us();
  if (t - dgg->last_impulse > dgg->impulse_frequency_us) {
    dgg->last_impulse = t;
    int i;
    for (i = 0; i < 3; i++) {
      da_random_impulse(&dgg->drift[i]);
    }
  }
#if 0  // no LCD label here -- that's what LASERABLES is for!
	if (((t >> 7) & 7) == 0)
	{
		// display label
		char str[8];
		int_to_string2(str, 4, 0, da_read(&dgg->drift));
		//LOG("dgg int str '%s'", str);
		ascii_to_bitmap_str(dgg->bbuf.buffer, 4, str);
		ascii_to_bitmap_str(dgg->bbuf.buffer, NUM_DIGITS, dgg->name);
	}
	else
	{
		// display bar graph
		int v = da_read(&dgg->drift);
		int d;
		for (d=0; d<NUM_DIGITS; d++)
		{
			uint8_t b = 0;
			if (d*2+1<v) b |= 0b0110000;
			if (d*2  <v) b |= 0b0000110;
			dgg->bbuf.buffer[d] = b;
		}
	}
#endif
  //	LOG("da = %d, %d, %d",
  //		da_read(&dgg->drift[0]),
  //		da_read(&dgg->drift[1]),
  //		da_read(&dgg->drift[2])
  //		);

  int d;
  for (d = 0; d < NUM_DIGITS; d++) {
    dgg->bbuf.buffer[d] = 0;
  }
  draw_skinny_bars(dgg, &dgg->drift[0], 0b1000000);
  draw_skinny_bars(dgg, &dgg->drift[1], 0b0000001);
  draw_skinny_bars(dgg, &dgg->drift[2], 0b0001000);

  board_buffer_draw(&dgg->bbuf);
}

void draw_skinny_bars(DGratuitousGraph *dgg, DriftAnim *da, SSBitmap bm) {
  int d;
  int v = da_read(da);
  for (d = 0; d < NUM_DIGITS; d++) {
    if (d * 2 < v) {
      dgg->bbuf.buffer[d] |= bm;
    }
  }
}

void draw_fat_bars(DGratuitousGraph *dgg, DriftAnim *da, SSBitmap bm0,
                   SSBitmap bm1) {
  int d;
  int v = da_read(da);
  for (d = 0; d < NUM_DIGITS; d++) {
    if (d * 2 + 1 < v) {
      dgg->bbuf.buffer[d] |= bm1;
    }  // 0b0000110;
    if (d * 2 < v) {
      dgg->bbuf.buffer[d] |= bm0;
    }  // 0b0110000;
  }
}
