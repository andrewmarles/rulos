#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "display_controller.h"
#include "display_rtc.h"
#include "display_scroll_msg.h"
#include "display_compass.h"
#include "focus.h"
#include "labeled_display.h"
#include "display_docking.h"
#include "display_gratuitous_graph.h"
#include "numeric_input.h"
#include "input_controller.h"
#include "calculator.h"
#include "display_aer.h"
#include "hal.h"
#include "cpumon.h"
#include "idle_display.h"
#include "sequencer.h"
#include "rasters.h"
#include "pong.h"
#include "lunar_distance.h"
#include "sim.h"
#include "display_thrusters.h"
#include "network.h"
#include "remote_keyboard.h"
#include "remote_bbuf.h"
#include "remote_uie.h"
#include "control_panel.h"

/****************************************************************************/

#ifndef SIM

#include "hardware.h"
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>


#define LED0		GPIO_C1
#define LED1		GPIO_C3
#define LED2		GPIO_C5
#define POT			GPIO_C4
#define SERVO		GPIO_B1
#define PUSHBUTTON	GPIO_B0

#define BUTTON_SCAN_PERIOD	20000
#define BUTTON_REFRACTORY_PERIOD	40000

#define SERVO_PERIOD	20000
#define POT_ADC_CHANNEL 4
#define OPT0_ADC_CHANNEL 2
#define OPT1_ADC_CHANNEL 0

/****************************************************************************/

typedef struct s_servo {
	ActivationFunc func;
	uint8_t pwm_width;	// 0..125
	r_bool state;
} ServoAct;

void servo_set_pwm(ServoAct *servo, uint8_t pwm_width);

ISR(TIMER2_COMP_vect)
{
	TIMSK &= ~_BV(OCIE2);
	gpio_clr(SERVO);
}

void servo_update(ServoAct *servo)
{
	schedule_us(SERVO_PERIOD-100, (Activation*) servo);

	cli();
	OCR2 = 43+servo->pwm_width;
	TCNT2 = 0;
	TIFR |= _BV(OCF2);	// clear OCF2 (inverted sense)
	TIMSK |= _BV(OCIE2);
	sei();

	gpio_set(SERVO);
//	servo->state = !servo->state;
//	gpio_set_or_clr(SERVO, servo->state);
}

void init_servo(ServoAct *servo)
{
	gpio_make_output(SERVO);

	// CTC mode ; prescaler 64 (8MHz => 8us ticks)
	TCCR2 =_BV(WGM21) | _BV(CS22);

	servo_set_pwm(servo, 1);
	servo->func = (ActivationFunc) servo_update;
	schedule_us(1, (Activation*) servo);
}

void servo_set_pwm(ServoAct *servo, uint8_t pwm_width)
{
	servo->pwm_width = pwm_width;
}

/****************************************************************************/

typedef struct s_quadrature {
	uint16_t threshhold;
	uint8_t oldState;
	int16_t position;
} Quadrature;

void init_quadrature(Quadrature *quad)
{
	quad->threshhold = 400;
	quad->oldState = 0;
	quad->position = 0;
}

#define XX	0	/* invalid transition */
static int8_t quad_state_machine[16] = {
	 0,	//0000
	+1,	//0001
	-1,	//0010
	XX,	//0011
	-1,	//0100
	 0,	//0101
	XX,	//0110
	+1,	//0111
	+1,	//1000
	XX,	//1001
	 0,	//1010
	-1,	//1011
	XX,	//1100
	-1,	//1101
	+1,	//1110
	 0	//1111
};
	
void quadrature_handler(Quadrature *quad)
{
	r_bool c0 = hal_read_adc(OPT0_ADC_CHANNEL) > quad->threshhold;
	r_bool c1 = hal_read_adc(OPT1_ADC_CHANNEL) > quad->threshhold;
	uint8_t newState = (c1<<1) | c0;
	uint8_t transition = (quad->oldState<<2) | newState;
	int8_t delta = quad_state_machine[transition];
	quad->position += delta;
	quad->oldState = newState;
}

int16_t quad_get_position(Quadrature *quad)
{
	return quad->position;
}

void quad_set_position(Quadrature *quad, int16_t pos)
{
	quad->position = pos;
}

/****************************************************************************/

// atmega8 manual pages 22-23
// sigh. already defined in avr .h file.

#if 0
void eeprom_write(uint16_t address, uint8_t data)
{
	while (EECR & (1<<EEWE))
		{}

	EEAR = address;
	EEDR = data;
	EECR |= (1<<EEMWE);
	EECR |= (1<<EEWE);
}

uint8_t eeprom_read(uint16_t address)
{
	while (EECR & (1<<EEWE))
		{}

	EEAR = address;
	EECR |= (1<<EERE);
	return EEDR;
}

void eeprom_write_block(uint16_t ee_dest, uint8_t *src, uint16_t length)
{
	uint16_t off;
	for (off=0; off<length; off++)
	{
		eeprom_write(ee_dest+off, src[off]);
	}
}

void eeprom_read_block(uint8_t *dest, uint16_t ee_src, uint16_t length)
{
	uint16_t off;
	for (off=0; off<length; off++)
	{
		dest[off] = eeprom_read(ee_src+off);
	}
}
#endif

/****************************************************************************/

r_bool get_pushbutton()
{
	gpio_make_input(PUSHBUTTON);
	return gpio_is_clr(PUSHBUTTON);
}

/****************************************************************************/

typedef struct s_config {
	uint8_t magic;	// to detect a valid eeprom record
	uint16_t doorMax;
	uint8_t servoPos[2];
} Config;

#define EEPROM_CONFIG_BASE 0
#define CONFIG_MAGIC 0xc3

r_bool init_config(Config *config)
{
	r_bool valid;

	if (get_pushbutton())
	{
		// hold button on boot to clear memory
		valid = FALSE;
	}
	else
	{
		eeprom_read_block((void*) config, EEPROM_CONFIG_BASE, sizeof(*config));
		valid = (config->magic == CONFIG_MAGIC);
	}
	
	if (!valid)
	{
		// install default meaningless values
		config->magic = CONFIG_MAGIC;
		config->doorMax = 100;
		config->servoPos[0] = 0;
		config->servoPos[1] = 212;
	}
	return valid;
}

uint16_t config_clip(Config *config, Quadrature *quad)
{
	int16_t newDoor = quad_get_position(quad);
	newDoor = min(newDoor, config->doorMax);
	newDoor = max(newDoor, 0);
	quad_set_position(quad, newDoor);
	return newDoor;
}

void config_update_servo(Config *config, ServoAct *servo, uint16_t doorPos)
{
	// Linearly interpolate
	uint32_t servoPos = (config->servoPos[1]-config->servoPos[0]);
	servoPos *= doorPos;
	servoPos /= config->doorMax;
	servoPos += config->servoPos[0];

	servo_set_pwm(servo, (uint8_t) servoPos);
}

void config_write_eeprom(Config *config)
{
	/* note arg order is src, dest; reversed from avr eeprom_read_block */
	eeprom_write_block((void*) config, EEPROM_CONFIG_BASE, sizeof(*config));
}

/****************************************************************************/

typedef enum {
	cm_run,
	cm_setLeft,
	cm_setRight,
} ControlMode;

typedef struct s_control_act {
	ActivationFunc func;
	ControlMode mode;
	Quadrature quad;
	Config config;
	ServoAct servo;
	uint16_t pot_servo;
	struct s_control_event_handler {
		UIEventHandlerFunc handler_func;
		struct s_control_act *controlAct;
	} handler;
} ControlAct;

static void control_update(ControlAct *ctl);
static UIEventDisposition control_handler(
	struct s_control_event_handler *handler, UIEvent evt);

void init_control(ControlAct *ctl)
{
	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);

	ctl->func = (ActivationFunc) control_update;
	init_quadrature(&ctl->quad);
	r_bool valid = init_config(&ctl->config);
	if (valid)
	{
		ctl->mode = cm_run;
	}
	else
	{
		ctl->mode = cm_setLeft;
	}
	init_servo(&ctl->servo);
	ctl->pot_servo = 0;
		// not important; gets immediately overwritten.
		// Just trying to have good constructor hygiene.
	ctl->handler.handler_func = (UIEventHandlerFunc) control_handler;
	ctl->handler.controlAct = ctl;
	schedule_us(100, (Activation*) ctl);
}

static void control_servo_from_pot(ControlAct *ctl)
{
	uint32_t pot = hal_read_adc(POT_ADC_CHANNEL);
	ctl->pot_servo = pot*212/1024;
	servo_set_pwm(&ctl->servo, ctl->pot_servo);
}

static void control_update(ControlAct *ctl)
{
	schedule_us(100, (Activation*) ctl);

	gpio_set(LED0);
	gpio_set(LED1);
	gpio_set(LED2);

	switch (ctl->mode)
	{
	case cm_run:
		{
		gpio_clr(LED0);
		uint8_t pos = config_clip(&ctl->config, &ctl->quad);
	 	config_update_servo(&ctl->config, &ctl->servo, pos);
		break;
		}
	case cm_setLeft:
		{
		gpio_clr(LED1);

		// hold door at "zero"
		quad_set_position(&ctl->quad, 0);

		control_servo_from_pot(ctl);
		
		break;
		}
	case cm_setRight:
		{
		gpio_clr(LED2);

		// let door slide to learn doorMax

		control_servo_from_pot(ctl);

		break;
		}
	}
}

static UIEventDisposition control_handler(
	struct s_control_event_handler *handler, UIEvent evt)
{
	ControlAct *control = handler->controlAct;
	switch (control->mode)
	{
	case cm_run:
		{
			control->mode = cm_setLeft;
			break;
		}
	case cm_setLeft:
		{
			control->config.servoPos[0] = control->pot_servo;
			control->mode = cm_setRight;
			break;
		}
	case cm_setRight:
		{
			control->config.servoPos[1] = control->pot_servo;
			control->config.doorMax = quad_get_position(&control->quad);
			config_write_eeprom(&control->config);
			control->mode = cm_run;
			break;
		}
	}
	return uied_accepted;
}

/****************************************************************************/

typedef struct s_btn_act {
	ActivationFunc func;
	r_bool lastState;
	Time lastStateTime;
	UIEventHandler *handler;
} ButtonAct;

static void button_update(ButtonAct *button);

void init_button(ButtonAct *button, UIEventHandler *handler)
{
	button->func = (ActivationFunc) button_update;
	button->lastState = FALSE;
	button->lastStateTime = clock_time_us();
	button->handler = handler;

	gpio_make_input(PUSHBUTTON);

	schedule_us(1, (Activation*) button);
}

void button_update(ButtonAct *button)
{
	schedule_us(BUTTON_SCAN_PERIOD, (Activation*) button);

	r_bool buttondown = get_pushbutton();

	if (buttondown == button->lastState)
		{ return; }

	if (clock_time_us() - button->lastStateTime >= BUTTON_REFRACTORY_PERIOD)
	{
		// This state transition counts.
		if (!buttondown)
		{
			button->handler->func(button->handler, uie_right);
		}
	}

	button->lastState = buttondown;
	button->lastStateTime = clock_time_us();
}


/****************************************************************************/

typedef struct s_blink_act {
	ActivationFunc func;
	ServoAct *servo;
	r_bool state;
} BlinkAct;

void blink_update(BlinkAct *act)
{
	schedule_us(((Time)1)<<15, (Activation*) act);

	act->state = !act->state;
//	gpio_set_or_clr(LED0, act->state);
//	gpio_set_or_clr(SERVO, act->state);

//	uint8_t phase = (clock_time_us() >> 18) & 0x07;
	uint16_t pot = hal_read_adc(OPT0_ADC_CHANNEL) & 0x03ff;
	servo_set_pwm(act->servo, ((uint32_t) pot)*212 / 1024);

	uint8_t phase = ((uint32_t) pot*7)/1024;

	gpio_set_or_clr(LED0, phase & 1);
	gpio_set_or_clr(LED1, phase & 2);
	gpio_set_or_clr(LED2, phase & 4);

	//servo_set_pwm(act->servo, phase<<5);
	//act->servo->pwmDuty = pot % 5;
}

void init_blink(BlinkAct *act, ServoAct *servo)
{
	act->func = (ActivationFunc) blink_update;
	act->servo = servo;
	act->state = TRUE;
	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);

	schedule_us(((Time)1)<<15, (Activation*) act);
}


/****************************************************************************/

typedef struct s_quad_test {
	ActivationFunc func;
} QuadTest;

static void qt_update(QuadTest *qt);

void init_quad_test(QuadTest *qt)
{
	qt->func = (ActivationFunc) qt_update;

	gpio_make_output(LED0);
	gpio_make_output(LED1);
	gpio_make_output(LED2);

	schedule_us(20000, (Activation*) qt);
}


static void qt_update(QuadTest *qt)
{
	schedule_us(20000, (Activation*) qt);

	gpio_set_or_clr(LED2, !(hal_read_adc(POT_ADC_CHANNEL) > 350));
	gpio_set_or_clr(LED1, !(hal_read_adc(OPT1_ADC_CHANNEL) > 350));
	gpio_set_or_clr(LED0, !(hal_read_adc(OPT0_ADC_CHANNEL) > 350));
	
	/*
	uint16_t v = hal_read_adc(OPT1_ADC_CHANNEL);
	gpio_set_or_clr(LED2, !((v>>9) & 1));
	gpio_set_or_clr(LED1, !((v>>8) & 1));
	gpio_set_or_clr(LED0, !((v>>7) & 1));
	*/
}

/****************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);
	clock_init(SERVO_PERIOD);
	hal_init_adc();
	/*
	hal_init_adc_channel(POT_ADC_CHANNEL);
	hal_init_adc_channel(OPT0_ADC_CHANNEL);
	hal_init_adc_channel(OPT1_ADC_CHANNEL);
	*/

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

/*
	ServoAct servo;
	init_servo(&servo);

	BlinkAct blink;
	init_blink(&blink, &servo);
*/

	QuadTest qt;
	init_quad_test(&qt);

/*
	ControlAct control;
	init_control(&control);

	ButtonAct button;
	init_button(&button, (UIEventHandler*) &control.handler);
*/
	
	cpumon_main_loop();

	return 0;
}

#else
int main()
{
	return 0;
}
#endif // SIM
