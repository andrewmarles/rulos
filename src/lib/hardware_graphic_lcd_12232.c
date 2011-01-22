#include "hardware_graphic_lcd_12232.h"

//////////////////////////////////////////////////////////////////////////////
// Board I/O mapping defs
// These are in order down the right side (pins 40-21) of a 1284p PDIP.
#define GLCD_RESET	GPIO_A0
#define GLCD_CS1	GPIO_A1
#define GLCD_CS2	GPIO_A2
#define GLCD_RW		GPIO_A3
// NC - a4
#define GLCD_A0		GPIO_A5
#define GLCD_DB0	GPIO_A6
#define GLCD_DB1	GPIO_A7
// nc - aref
// nc - gnd
// nc - avcc
#define GLCD_DB5	GPIO_C7
#define GLCD_DB6	GPIO_C6
#define GLCD_DB7	GPIO_C5
#define GLCD_VBL	GPIO_C4
#define GLCD_DB2	GPIO_C3
#define GLCD_DB3	GPIO_C2
#define GLCD_DB4	GPIO_C1
// nc - c0
// nc - d7
//////////////////////////////////////////////////////////////////////////////

void glcd_complete_init(Activation *act);

//////////////////////////////////////////////////////////////////////////////
// kudos for reference code from:
// http://en.radzio.dxp.pl/sed1520/sed1520.zip

#define GLCD_EVENT_BUSY		0x80
#define GLCD_EVENT_RESET	0x10

#define GLCD_CMD_DISPLAY_ON	0xAF
#define GLCD_CMD_DISPLAY_OFF	0xAE
#define GLCD_CMD_DISPLAY_START_LINE	0xC0
#define GLCD_CMD_RESET 0xE2

#define GLCD_READ       1
#define GLCD_WRITE      0
void glcd_set_bus_dir(r_bool dir)
{
	// TODO life would be faster if hardware had data bus aligned
	// with an 8-bit port!
	if (dir==GLCD_WRITE)
	{
		gpio_make_output(GLCD_DB0);
		gpio_make_output(GLCD_DB1);
		gpio_make_output(GLCD_DB2);
		gpio_make_output(GLCD_DB3);
		gpio_make_output(GLCD_DB4);
		gpio_make_output(GLCD_DB5);
		gpio_make_output(GLCD_DB6);
		gpio_make_output(GLCD_DB7);
	}
	else
	{
		gpio_make_input(GLCD_DB0);
		gpio_make_input(GLCD_DB1);
		gpio_make_input(GLCD_DB2);
		gpio_make_input(GLCD_DB3);
		gpio_make_input(GLCD_DB4);
		gpio_make_input(GLCD_DB5);
		gpio_make_input(GLCD_DB6);
		gpio_make_input(GLCD_DB7);
	}
}

uint8_t glcd_data_in()
{
	uint8_t val = 0
		| (gpio_is_set(GLCD_DB0) ? (1<<0) : 0)
		| (gpio_is_set(GLCD_DB1) ? (1<<1) : 0)
		| (gpio_is_set(GLCD_DB2) ? (1<<2) : 0)
		| (gpio_is_set(GLCD_DB3) ? (1<<3) : 0)
		| (gpio_is_set(GLCD_DB4) ? (1<<4) : 0)
		| (gpio_is_set(GLCD_DB5) ? (1<<5) : 0)
		| (gpio_is_set(GLCD_DB6) ? (1<<6) : 0)
		| (gpio_is_set(GLCD_DB7) ? (1<<7) : 0)
		;
	return val;
}

void glcd_data_out(uint8_t v)
{
	gpio_set_or_clr(GLCD_DB0, (v & (1<<0)));
	gpio_set_or_clr(GLCD_DB1, (v & (1<<1)));
	gpio_set_or_clr(GLCD_DB2, (v & (1<<2)));
	gpio_set_or_clr(GLCD_DB3, (v & (1<<3)));
	gpio_set_or_clr(GLCD_DB4, (v & (1<<4)));
	gpio_set_or_clr(GLCD_DB5, (v & (1<<5)));
	gpio_set_or_clr(GLCD_DB6, (v & (1<<6)));
	gpio_set_or_clr(GLCD_DB7, (v & (1<<7)));
}

void glcd_wait_status_event(uint8_t event, uint8_t unit)
{
	gpio_set(GLCD_RW);	// read
	gpio_clr(GLCD_A0);	// cmd
	glcd_set_bus_dir(GLCD_READ);
	while (1)
	{
		uint8_t status;
		if (unit == 0) 
		{
			gpio_set(GLCD_CS1);
			asm("nop");asm("nop"); 
			status = glcd_data_in();
			gpio_clr(GLCD_CS1);
		} 
		else 
		{
			gpio_set(GLCD_CS2);
			asm("nop");asm("nop"); 
			status = glcd_data_in();
			gpio_clr(GLCD_CS2);
		}
		if ((status & event)==0)
		{
			break;
		}
	}
	glcd_set_bus_dir(GLCD_WRITE);
}

static inline void glcd_strobe(uint8_t unit)
{
	if (unit)
	{
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS2);
	}
	else
	{
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS1);
	}
}

void glcd_write_cmd(uint8_t cmd, uint8_t unit)
{
	glcd_wait_status_event(GLCD_EVENT_BUSY, unit);

	gpio_clr(GLCD_RW);	// write
	gpio_clr(GLCD_A0);	// cmd
	glcd_data_out(cmd);
	glcd_strobe(unit);
}

#define F_CPU 8000000
#include <util/delay.h>
void glcd_init(GLCD *glcd, Activation *done_act)
{
	glcd->done_act = done_act;

	// set up fixed I/O pins
	glcd_set_bus_dir(GLCD_WRITE);
	gpio_make_output(GLCD_CS1);
	gpio_make_output(GLCD_CS2);
	gpio_make_output(GLCD_RW);
	gpio_make_output(GLCD_A0);
	gpio_make_output(GLCD_RESET);

	// Hardware reset
	gpio_clr(GLCD_RESET);
	_delay_ms(10);
	gpio_set(GLCD_RESET);
	glcd_complete_init(&glcd->act);
/*
	glcd->act.func = glcd_complete_init;
	schedule_us(10000, &glcd->act);
*/
}

void glcd_complete_init(Activation *act)
{
	GLCD *glcd = (GLCD*) act;
	gpio_set(GLCD_RESET);

	glcd_write_cmd(GLCD_CMD_RESET, 0);
	glcd_write_cmd(GLCD_CMD_RESET, 1);
	glcd_wait_status_event(GLCD_EVENT_RESET, 0);
	glcd_wait_status_event(GLCD_EVENT_RESET, 1);
	glcd_write_cmd(GLCD_CMD_DISPLAY_ON, 0);
	glcd_write_cmd(GLCD_CMD_DISPLAY_ON, 1);
	glcd_write_cmd(GLCD_CMD_DISPLAY_START_LINE | 0, 0);
	glcd_write_cmd(GLCD_CMD_DISPLAY_START_LINE | 0, 1);

	if (glcd->done_act!=NULL)
	{
		schedule_now(glcd->done_act);
	}
}

//////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------

#include <avr/io.h>

#define SCREEN_WIDTH	122

extern unsigned char lcd_x, lcd_y;
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_WaitForStatus(unsigned char status, unsigned char controller)
{
	uint8_t tmp;
	gpio_clr(GLCD_A0);
	gpio_set(GLCD_RW);
	glcd_set_bus_dir(GLCD_READ);

	do
	{
		if(controller == 0) 
		{
			gpio_set(GLCD_CS1);
			asm("nop");asm("nop"); 
			tmp = glcd_data_in();
			gpio_clr(GLCD_CS1);
		} 
		else 
		{
			gpio_set(GLCD_CS2);
			asm("nop");asm("nop"); 
			tmp = glcd_data_in();
			gpio_clr(GLCD_CS2);
		}
	} while(tmp & status);
	glcd_set_bus_dir(GLCD_WRITE);
}
//-------------------------------------------------------------------------------------------------
// Write command
//-------------------------------------------------------------------------------------------------
void GLCD_WriteCommand(unsigned char commandToWrite,unsigned char ctrl)
{
	GLCD_WaitForStatus(0x80, ctrl);

	gpio_clr(GLCD_A0);
	gpio_clr(GLCD_RW);

	glcd_data_out(commandToWrite);

	if(ctrl)
	{
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS2);
	}
	else
	{
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS1);
	}
}
//-------------------------------------------------------------------------------------------------
// Write data
//-------------------------------------------------------------------------------------------------
void GLCD_WriteData(unsigned char dataToWrite)
{
	GLCD_WaitForStatus(0x80, 0);
	GLCD_WaitForStatus(0x80, 1);

	gpio_set(GLCD_A0);
	gpio_clr(GLCD_RW);

	glcd_data_out(dataToWrite);

	if(lcd_x < 61) 
	{
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS1);
	}
	else
	{
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS2);
	}
	lcd_x++;
	if(lcd_x >= SCREEN_WIDTH)
		lcd_x = 0;
}
//-------------------------------------------------------------------------------------------------
// Read data
//-------------------------------------------------------------------------------------------------
unsigned char GLCD_ReadData(void)
{
	unsigned char tmp;

	GLCD_WaitForStatus(0x80, 0); 
	GLCD_WaitForStatus(0x80, 1); 
	gpio_set(GLCD_A0);
	gpio_set(GLCD_RW);
	glcd_set_bus_dir(GLCD_READ);
	glcd_data_out(0xff);

	if(lcd_x < 61)
	{
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS1);
		asm("nop");asm("nop");
		gpio_set(GLCD_CS1);
		asm("nop");asm("nop");
		tmp = glcd_data_in();
		gpio_clr(GLCD_CS1);
	}
	else 
	{	
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_clr(GLCD_CS2);
		asm("nop");asm("nop");
		gpio_set(GLCD_CS2);
		asm("nop");asm("nop");
		tmp = glcd_data_in();
		gpio_clr(GLCD_CS2);
	}
	glcd_set_bus_dir(GLCD_WRITE);
	lcd_x++; 
	if(lcd_x > 121)
		lcd_x = 0;
	return tmp;
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//

//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------


#define PAGE_ADDRESS_SET 0xB8
#define COLUMN_ADDRESS_SET 0x00
#define ADC_CLOCKWISE 0xA0
#define ADC_COUNTERCLOCKWISE 0xA1
#define STATIC_DRIVE_ON 0xA5
#define STATIC_DRIVE_OFF 0xA4
#define DUTY_RATIO_16 0xA8
#define DUTY_RATIO_32 0xA9
#define READ_MODIFY_WRITE 0xE0
#define END_READ_MODIFY 0xEE


#define SCREEN_WIDTH	122

void GLCD_WaitForStatus(unsigned char,unsigned char);
void GLCD_WriteCommand(unsigned char,unsigned char);
void GLCD_GoTo(unsigned char,unsigned char);
void GLCD_WriteData(unsigned char);
unsigned char GLCD_ReadData(void);
void GLCD_ClearScreen(void);
void GLCD_WriteChar(char);
void GLCD_WriteString(char *);
void GLCD_SetPixel(unsigned char, unsigned char, unsigned char);
void GLCD_Bitmap(char * , unsigned char, unsigned char, unsigned char, unsigned char);
//-------------------------------------------------------------------------------------------------
unsigned char lcd_x = 0, lcd_y = 0;
//-------------------------------------------------------------------------------------------------
extern void GLCD_WaitForStatus(unsigned char, unsigned char);
extern void GLCD_WriteCommand(unsigned char, unsigned char);
extern void GLCD_WriteDatta(unsigned char);
extern unsigned char GLCD_ReadData(void);
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_GoTo(unsigned char x,unsigned char y)
{
lcd_x = x;
lcd_y = y;

if(x < (SCREEN_WIDTH/2))
  {
  GLCD_WriteCommand(COLUMN_ADDRESS_SET | lcd_x, 0);
  GLCD_WriteCommand(PAGE_ADDRESS_SET | lcd_y, 0);
  GLCD_WriteCommand(COLUMN_ADDRESS_SET | 0, 1);
  GLCD_WriteCommand(PAGE_ADDRESS_SET | lcd_y, 1);
  }
else
  {
  GLCD_WriteCommand(COLUMN_ADDRESS_SET | (lcd_x - (SCREEN_WIDTH/2)), 1);
  GLCD_WriteCommand(PAGE_ADDRESS_SET | lcd_y, 1);
  }
}
//-------------------------------------------------------------------------------------------------
// 
//-------------------------------------------------------------------------------------------------
void GLCD_ClearScreen(void)
{
char j, i;
for(j = 0; j < 4; j++)
  {
  GLCD_GoTo(0, j);
  for(i = 0; i < SCREEN_WIDTH; i++)
    {
	GLCD_WriteData(0);
	}
  }
GLCD_GoTo(0, 0);
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_WriteChar(char x)
{
char i; 
x -= 32; 
for(i = 0; i < 5; i++) 
#if 0
  GLCD_WriteData(pgm_read_byte(font5x7 + (5 * x) + i));
#endif
GLCD_WriteData(0x00);
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_WriteString(char * s)
{
while(*s)
  {
  GLCD_WriteChar(*s++);
  }
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_SetPixel(unsigned char x, unsigned char y, unsigned char color)
{
unsigned char temp;  
GLCD_GoTo(x, y/8); 
temp = GLCD_ReadData(); 
GLCD_GoTo(x, y/8); 
if(color)
  GLCD_WriteData(temp | (1 << (y % 8))); 
else
  GLCD_WriteData(temp & ~(1 << (y % 8))); 
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
void GLCD_Bitmap(char * bmp, unsigned char x, unsigned char y, unsigned char dx, unsigned char dy)
{
unsigned char i, j;
for(j = 0; j < dy / 8; j++)
  {
  GLCD_GoTo(x,y + j);
  for(i = 0; i < dx; i++)
    GLCD_WriteData(pgm_read_byte(bmp++)); 
  }
}
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------------------------
