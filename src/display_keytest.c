
#include <string.h>

#include "board_buffer.h"
#include "clock.h"
#include "hal.h"
#include "heap.h"
#include "display_keytest.h"
#include "util.h"

static void update(KeyTestActivation_t *kta)
{
	schedule(50, (Activation *) kta);
	char k = hal_scan_keyboard();

	LOGF((logfp, "in update for keytest\n"))
	if (!k)
		return;

	LOGF((logfp, "got char %c\n", k))

	memmove(kta->bbuf.buffer, kta->bbuf.buffer+1, NUM_DIGITS-1);
	kta->bbuf.buffer[NUM_DIGITS-1] = ascii_to_bitmap(k);
	board_buffer_draw(&kta->bbuf);

}

void display_keytest_init(KeyTestActivation_t *kta, uint8_t board)
{
	kta->f = (ActivationFunc) update;
	board_buffer_init(&kta->bbuf);
	board_buffer_push(&kta->bbuf, board);
	schedule(1, (Activation *) kta);
}

