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

#include "periph/hpam/hpam.h"
#include "periph/rocket/idle.h"
#include "periph/rocket/rocket.h"

struct s_screen_blanker;
struct s_screenblanker_sender;

typedef enum {
  sb_inactive = 1,  // not operating.
  sb_blankdots,     // all digits showing only SB_DECIMAL
  sb_black,         // all LED segments off; lights unaffected
  sb_disco,         // flicker panels & lights by color
  sb_flicker,       // mostly on, with segments vanishing intermittently
  sb_borrowed  // nothing; buffers borrowed by slow_boot. (memory-savings hack)
} ScreenBlankerMode;

typedef enum {
  DISCO_GREEN = 1,
  DISCO_RED = 2,
  DISCO_YELLOW = 3,
  DISCO_BLUE = 4,
  DISCO_WHITE = 5,
} DiscoColor;

typedef struct s_screen_blanker {
  UIEventHandlerFunc func;
  BoardBuffer buffer[NUM_TOTAL_BOARDS];
  uint8_t hpam_max_alpha[NUM_TOTAL_BOARDS];
  uint8_t num_buffers;
  ScreenBlankerMode mode;
  HPAM *hpam;
  const uint32_t *tree;
  DiscoColor disco_color;
  struct s_screenblanker_sender *screenblanker_sender;
} ScreenBlanker;

void init_screenblanker(ScreenBlanker *screenblanker, HPAM *hpam,
                        IdleAct *idle);
void screenblanker_setmode(ScreenBlanker *screenblanker,
                           ScreenBlankerMode newmode);
void screenblanker_setdisco(ScreenBlanker *screenblanker,
                            DiscoColor disco_color);

//////////////////////////////////////////////////////////////////////////////

typedef struct s_screenblanker_payload {
  ScreenBlankerMode mode;
  DiscoColor disco_color;
} ScreenblankerPayload;

typedef struct s_screenblanker_listener {
  ScreenBlanker screenblanker;
  uint8_t
      app_receiver_storage[RECEIVE_RING_SIZE(1, sizeof(ScreenblankerPayload))];
  AppReceiver app_receiver;
} ScreenBlankerListener;

void init_screenblanker_listener(ScreenBlankerListener *sbl, Network *network);

typedef struct s_screenblanker_sender {
  Network *network;
  uint8_t send_slot_storage[sizeof(WireMessage) + sizeof(ScreenblankerPayload)];
  SendSlot sendSlot;
} ScreenBlankerSender;

void init_screenblanker_sender(ScreenBlankerSender *sbs, Network *network);
void sbs_send(ScreenBlankerSender *sbs, ScreenBlanker *sb);
