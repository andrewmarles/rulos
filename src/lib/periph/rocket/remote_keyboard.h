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

#ifndef _remote_keyboard_h
#define _remote_keyboard_h

#include "core/network.h"
#include "core/network_ports.h"
#include "periph/input_controller/input_controller.h"
#include "periph/rocket/rocket.h"

typedef struct {
	char key;
} KeystrokeMessage;

typedef struct s_remote_keyboard_send {
	Network *network;
	Port port;
	uint8_t send_msg_alloc[sizeof(Message)+sizeof(KeystrokeMessage)];
	SendSlot sendSlot;

	InputInjectorIfc forwardLocalStrokes;
	struct s_remote_keyboard_send *forward_this;
} RemoteKeyboardSend;

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network, Addr addr, Port port);

typedef struct s_remote_keyboard_recv {
	uint8_t recv_msg_alloc[sizeof(Message)+sizeof(KeystrokeMessage)];
	RecvSlot recvSlot;
	InputInjectorIfc *acceptNetStrokes;
} RemoteKeyboardRecv;

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network, InputInjectorIfc *acceptNetStrokes, Port port);

#endif // _remote_keyboard_h
