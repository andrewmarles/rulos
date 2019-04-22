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

#include "core/rulos.h"
#include "periph/i2s/i2s.h"

//////////////////////////////////////////////////////////////////////////////

#define SAMPLE_BUF_COUNT 4096

typedef struct {
  int num_buffers_filled;
  uint8_t i2s_storage[I2S_STATE_SIZE(SAMPLE_BUF_COUNT)];
  i2s_t* i2s;
  uint16_t samplebuffer[SAMPLE_BUF_COUNT];
} AudioState;

static void start_audio(void* user_data) {
  LOG("starting audio");
  AudioState* as = (AudioState*)user_data;
  as->num_buffers_filled = 0;

  i2s_start(as->i2s);
}

static void fill_buffer_cb(void* user_data, uint16_t* buffer_to_fill) {
  AudioState* as = (AudioState*)user_data;
  if (++as->num_buffers_filled == 11) {
    i2s_buf_filled(as->i2s, buffer_to_fill, 0);
  } else {
    memcpy(buffer_to_fill, as->samplebuffer, sizeof(as->samplebuffer));
    i2s_buf_filled(as->i2s, buffer_to_fill, SAMPLE_BUF_COUNT);
  }
}

static void audio_done_cb(void* user_data) {
  LOG("audio done");
  schedule_us(500000, start_audio, user_data);
}

void init_samples(AudioState* as) {
  int sample = 0;
  // float scalefactor = (1 << 14);
  float scalefactor = (1 << 13);
  for (int i = 0; i < SAMPLE_BUF_COUNT / 2; i++) {
    float sinewave =
        (sinf(((float)3.14159 * 2 * i * 32) / (SAMPLE_BUF_COUNT / 2)));
    int32_t intsinewave = sinewave * scalefactor;
    int16_t value = ((intsinewave >> 8) & 0xff) | ((intsinewave & 0xff) << 8);
    as->samplebuffer[sample++] = value;
    as->samplebuffer[sample++] = value;
  }
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

#ifdef LOG_TO_SERIAL
  HalUart uart;
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  AudioState as;
  memset(&as, 0, sizeof(as));
  init_samples(&as);
  as.i2s = i2s_init(SAMPLE_BUF_COUNT, 48000, &as, fill_buffer_cb, audio_done_cb,
                    as.i2s_storage, sizeof(as.i2s_storage));
  schedule_now((ActivationFuncPtr)start_audio, &as);
  cpumon_main_loop();

  return 0;
}
