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

#pragma once

#include "core/rulos.h"

// 5 -> known choppy

// 6 -> known choppy
//  11624	    128	   1125	  12877	   324d	audioboard.atmega328p.CUSTOM.elf
// 7 -> known good
//  11624	    128	   1317	  13069	   330d	audioboard.atmega328p.CUSTOM.elf
#define AO_BUFLENLG2 (7)
#define AO_BUFLEN (((uint16_t)1) << AO_BUFLENLG2)
#define AO_BUFMASK ((((uint16_t)1) << AO_BUFLENLG2) - 1)
#define AO_NUMBUFS 2

typedef struct {
  uint8_t buffers[AO_NUMBUFS][AO_BUFLEN];
  uint8_t sample_index;
  uint8_t play_buffer;

  uint8_t fill_buffer;  // the one fill_act should populate when called.
  ActivationRecord fill_act;
} AudioOut;

void init_audio_out(AudioOut *ao, uint8_t timer_id, ActivationFuncPtr fill_func,
                    void *fill_data);
