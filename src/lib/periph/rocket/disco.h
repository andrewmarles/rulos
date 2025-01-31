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

#include "periph/audio/audio_client.h"
#include "periph/rocket/rocket.h"
#include "periph/rocket/screenblanker.h"
#include "periph/audio/music_metadata_message.h"
#include "periph/rocket/display_scroll_msg.h"

typedef struct s_disco_handler {
  UIEventHandler uieh;
  struct s_disco *disco;
} DiscoHandler;

typedef struct s_disco {
  ScreenBlanker *screenblanker;
  DiscoHandler handler;
  AudioClient *audioClient;
  bool focused;

  uint8_t recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(MusicMetadataMessage))];
  AppReceiver app_receiver;
  MusicMetadataMessage music_metadata;  // Buffer to hold receieved metadata string; sized to match whatever was sent.
  DScrollMsgAct scroll_metadata;
} Disco;

void disco_init(Disco *disco, AudioClient *audioClient,
                ScreenBlanker *screenblanker, IdleAct *idle, Network* network);
