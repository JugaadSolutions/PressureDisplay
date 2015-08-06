#include "board.h"
#include "typedefs.h"
#include "linearkeypad.h"
#include "digitdisplay.h"

typedef enum 
{
	POWER_ON,
	MENU,
	SET_SELECT_ADDRESS,
	SET_SELECT_BAUDRATE
}APP_STATE;


typedef enum
{
	MENU_PB = KEY0,
	NEXT_PB = KEY1,
	SET_SEL_PB = KEY2
}PB;

extern void app_init(void);
extern void app_task(void);


#define EPROM_ADDRESS (0)