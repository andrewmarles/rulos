#ifndef __heap_h__
#define __heap_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

// had to move this from clock.h to here to resolve circular dependency
typedef int32_t Time;	// in units of usec

struct s_activation;
typedef void (*ActivationFunc)(struct s_activation *act);
typedef struct s_activation {
	ActivationFunc func;
} Activation;

typedef struct {
	Time key;
	Activation *activation;
} HeapEntry;

void heap_init();
void heap_insert(Time key, Activation *act);
int heap_peek(/*out*/ Time *key, /*out*/ Activation **act);
	/* rc nonzero => heap empty */
void heap_pop();

#endif // heap_h
