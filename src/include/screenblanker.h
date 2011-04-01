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

#ifndef _SCREENBLANKER_H
#define _SCREENBLANKER_H

#include "rocket.h"
#include "idle.h"
#include "hpam.h"

#define SB_MAX_BUFFERS 8

struct s_screen_blanker;
struct s_screenblanker_sender;

typedef enum
{
	sb_inactive=1,	// not operating.
	sb_blankdots,	// all digits showing only SB_DECIMAL
	sb_black,		// all LED segments off; lights unaffected
	sb_disco,		// flicker panels & lights by color
	sb_flicker,		// mostly on, with segments vanishing intermittently
	sb_borrowed		// nothing; buffers borrowed by slow_boot. (memory-savings hack)
} ScreenBlankerMode;

typedef enum {
	DISCO_GREEN=1,
	DISCO_RED=2,
	DISCO_YELLOW=3,
	DISCO_BLUE=4,
	DISCO_WHITE=5,
} DiscoColor;

typedef struct s_screen_blanker {
	UIEventHandlerFunc func;
	BoardBuffer buffer[SB_MAX_BUFFERS];
	uint8_t hpam_max_alpha[SB_MAX_BUFFERS];
	uint8_t num_buffers;
	ScreenBlankerMode mode;
	HPAM *hpam;
	uint32_t *tree;
	DiscoColor disco_color;
	struct s_screenblanker_sender *screenblanker_sender;
} ScreenBlanker;

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle);
void screenblanker_setmode(ScreenBlanker *screenblanker, ScreenBlankerMode newmode);
void screenblanker_setdisco(ScreenBlanker *screenblanker, DiscoColor disco_color);

//////////////////////////////////////////////////////////////////////////////

typedef struct s_screenblanker_payload {
	ScreenBlankerMode mode;
	DiscoColor disco_color;
} ScreenblankerPayload;

typedef struct s_screenblanker_listener {
	ScreenBlanker screenblanker;
	RecvSlot recvSlot;
	uint8_t message_storage[sizeof(Message)+sizeof(ScreenblankerPayload)];
} ScreenBlankerListener;

void init_screenblanker_listener(ScreenBlankerListener *sbl, Network *network, BoardConfiguration bc);

typedef struct s_screenblanker_sender {
	Network *network;
	uint8_t message_storage[sizeof(Message)+sizeof(ScreenblankerPayload)];
	SendSlot sendSlot;
} ScreenBlankerSender;

void init_screenblanker_sender(ScreenBlankerSender *sbs, Network *network);
void sbs_send(ScreenBlankerSender *sbs, ScreenBlanker *sb);

#endif // _SCREENBLANKER_H
