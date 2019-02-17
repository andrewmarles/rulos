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

#include "periph/remote_keyboard/remote_keyboard.h"

void rk_send(InputInjectorIfc *injector, Keystroke key);
void rk_send_complete(SendSlot *sendSlot);
void rk_recv(MessageRecvBuffer *msg);

void init_remote_keyboard_send(RemoteKeyboardSend *rk, Network *network,
                               Addr addr, Port port) {
  rk->network = network;
  rk->port = port;

  rk->sendSlot.func = NULL;
  rk->sendSlot.wire_msg = (WireMessage *)rk->send_msg_alloc;
  rk->sendSlot.dest_addr = addr;
  rk->sendSlot.wire_msg->dest_port = rk->port;
  rk->sendSlot.payload_len = sizeof(KeystrokeMessage);
  rk->sendSlot.sending = FALSE;

  rk->forwardLocalStrokes.func = rk_send;
  rk->forward_this = rk;
}

void rk_send(InputInjectorIfc *injector, Keystroke key) {
  RemoteKeyboardSend *rk = *(RemoteKeyboardSend **)(injector + 1);
  KeystrokeMessage *km = (KeystrokeMessage *)&rk->sendSlot.wire_msg->data;

  if (rk->sendSlot.sending) {
    LOG("RemoteKeyboard drops a message due to full send queue.");
    return;
  }

  km->key = key;
  net_send_message(rk->network, &rk->sendSlot);
}

void init_remote_keyboard_recv(RemoteKeyboardRecv *rk, Network *network,
                               InputInjectorIfc *acceptNetStrokes, Port port) {
  rk->app_receiver.recv_complete_func = rk_recv;
  rk->app_receiver.port = port;
  rk->app_receiver.num_receive_buffers = 1;
  rk->app_receiver.payload_capacity = sizeof(KeystrokeMessage);
  rk->app_receiver.message_recv_buffers = rk->recv_ring_alloc;
  rk->app_receiver.user_data = rk;

  rk->acceptNetStrokes = acceptNetStrokes;

  net_bind_receiver(network, &rk->app_receiver);
}

void rk_recv(MessageRecvBuffer *msg) {
  RemoteKeyboardRecv *rk = (RemoteKeyboardRecv *)msg->app_receiver->user_data;
  KeystrokeMessage *km = (KeystrokeMessage *)msg->data;
  assert(msg->payload_len == sizeof(KeystrokeMessage));
  LOG("remote key: %c", km->key.key);
  rk->acceptNetStrokes->func(rk->acceptNetStrokes, km->key);
  net_free_received_message_buffer(msg);
}
