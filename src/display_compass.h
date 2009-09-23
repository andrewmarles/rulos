#ifndef display_compass_h
#define display_compass_h

#include "clock.h"
#include "board_buffer.h"
#include "focus.h"

struct s_dcompassact;

typedef struct {
	UIEventHandlerFunc func;
	struct s_dcompassact *act;
} DCompassHandler;

typedef struct s_dcompassact {
	ActivationFunc func;
	BoardBuffer bbuf;
	uint8_t offset;
	DCompassHandler handler;
	uint8_t focused;
} DCompassAct;

void dcompass_init(DCompassAct *act, uint8_t board, FocusAct *focus);

#endif // display_compass_h
