#include "board.h"
#include "typedefs.h"
#include "linearkeypad.h"
#include "digitdisplay.h"

#define EPROM_ADDRESS 	(0)
	

typedef enum 
{
	POWER_ON = 0,
	MENU,
	SET_SELECT_ADDRESS,
	SET_SELECT_BAUDRATE,
	SET_CLOCK
}APP_STATE;

typedef enum
{
	SHOW_CLOCK = 0,
	SHOW_PRESSURE
}SUB_STATE;


typedef enum
{
	MENU_PB = KEY0,
	NEXT_PB = KEY1,
	SET_SEL_PB = KEY2
}PB;


typedef enum
{
	MB_SLAVE_ID = 0,
	MB_BAUD_RATE,
	CLOCK
}ITEMS;




extern void APP_init(void);
extern void APP_task(void);




