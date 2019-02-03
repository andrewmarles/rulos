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

#pragma once

#include <stdint.h>

typedef uint8_t Addr;

struct s_MediaRecvSlot;
typedef void (*MediaRecvDispatcher)(struct s_MediaRecvSlot *recvSlot);
typedef struct s_MediaRecvSlot {
  MediaRecvDispatcher func;
  uint8_t capacity;
  uint8_t packet_len;  // 0 tells hardware handler this slot is empty.
  void *user_data;     // storage for a pointer back to your state structure
  char data[0];
} MediaRecvSlot;

typedef void (*MediaSendDoneFunc)(void *user_data);
typedef struct s_MediaStateIfc {
  void (*send)(struct s_MediaStateIfc *media, Addr dest_addr,
               const unsigned char *data, uint8_t len,
               MediaSendDoneFunc sendDoneCB, void *sendDoneCBData);
} MediaStateIfc;
