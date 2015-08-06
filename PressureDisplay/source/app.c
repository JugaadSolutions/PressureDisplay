#include "app.h" 
#include "eep.h"
#include "adc_ide.h"
#include "mb.h"

#define REG_INPUT_START 1
#define REG_INPUT_NREGS 1





void conversion( UINT16 );
void itemSelect( void );
void countToPressure(void);
void ASCII_decimal( void );



/* ----------------------- Static variables ---------------------------------*/
static USHORT   usRegInputStart = REG_INPUT_START;
static USHORT   usRegInputBuf[REG_INPUT_NREGS];

typedef struct _App
{
	APP_STATE state;
	UINT8 baudRate_sel; // To store the baud rate from eeprom
	UINT8 address;		// To store the address from eeprom 
	UINT8 currentItem;   	// To display next item in the menu	
	UINT16 adc_output;  // ADC output in counts
	UINT16 pressureMbar; // ADC output in pressure
	UINT16 blinkIndex; //Used to point the digit to be blinked
	}APP;

#pragma idata app_data
UINT8 buffer[4] = {0};
APP app = {0};
#pragma idata

void app_init( void )
{
	eMBErrorCode    eStatus;
	char abc[3] = "ab";
	app.state =  POWER_ON;  // set the app to ADC output state
	app.address = Read_b_eep (EPROM_ADDRESS); 
	app.baudRate_sel = Read_b_eep (EPROM_ADDRESS+1);
	app.blinkIndex = 0XFF;  // reset blink index
	DigitDisplay_dotOn( );						// Display dot in the output
	eStatus = eMBInit( MB_RTU,app.address, 0, app.baudRate_sel, MB_PAR_NONE);


    eStatus = eMBEnable(  );	/* Enable the Modbus Protocol Stack. */
}

void app_task( void )
{
	switch ( app.state )
	{
		case POWER_ON: 
			app.currentItem = 0xFF;						  // To bypass menu items 
			app.adc_output = ADC_getResult();
			countToPressure();

			conversion ( app.pressureMbar );					  // Convert ADC output to segment value
//			conversion(app.adc_output);

			DigitDisplay_updateBuffer_noValidation ( buffer );      // Display ADC output 
			if (LinearKeyPad_getKeyState( MENU_PB ))   	 // When MENU_PB is pressed show first item in the menu
			{	
				app.currentItem = 0;
				conversion ( app.address );
				DigitDisplay_updateBuffer_noValidation ( buffer );
							
				app.state = MENU;					 	 // else display ADC output
			}
		break;

		case MENU: DigitDisplay_dotOff( );		
			if ( LinearKeyPad_getKeyState( MENU_PB ))	 // In MENU if MENU_PB is pressed goto ADC o/p
			{
				app.state = POWER_ON;
				DigitDisplay_dotOn( );						// Display dot in the output
			}
			

			else if (LinearKeyPad_getKeyState( NEXT_PB ))     // If NEXT_PB is pressed show items in the menu
			{

				itemSelect();								  // call fuction to select the different items in the menu
				DigitDisplay_updateBuffer_noValidation (buffer);
			}
			
			else if ( LinearKeyPad_getKeyState( SET_SEL_PB ))
			{	
				app.blinkIndex = 0;
				DigitDisplay_blinkOn_ind(500, app.blinkIndex);
			
				if( app.currentItem == 0 )
					app.state = SET_SELECT_ADDRESS;
				else if ( app.currentItem == 1 )
					app.state = SET_SELECT_BAUDRATE;
			}
								
		
		break;
		
		case SET_SELECT_ADDRESS:
			if (LinearKeyPad_getKeyState( NEXT_PB ))
				{		
					if ( buffer[app.blinkIndex] == '9' )//reset count, if it is > 9
						buffer[app.blinkIndex] = '0';	
		
					else buffer[app.blinkIndex]++;						
									
		
					buffer[3] = 0X77; // To display letter 'A' in the output 77
					DigitDisplay_updateBuffer_noValidation ( buffer );	
					DigitDisplay_blinkOn_ind(500, app.blinkIndex);							
				}
		
			else if ( LinearKeyPad_getKeyState( SET_SEL_PB ))
				{					
					app.blinkIndex++;
					if ( app.blinkIndex >= 3)
					{
						app.blinkIndex = 0XFF;
						ASCII_decimal( );
						Write_b_eep (EPROM_ADDRESS, app.address);
				
						DigitDisplay_blinkOff( );
						app.state = MENU;
					}				
					else 
						DigitDisplay_blinkOn_ind(500, app.blinkIndex);

					
 				}
			
			else if (LinearKeyPad_getKeyState( MENU_PB ))
				{	
					UINT8 previousData;
					previousData = Read_b_eep (EPROM_ADDRESS);
					
					if ( previousData != app.address)
							app.address = previousData;
				
					app.blinkIndex = 0xFF;

					DigitDisplay_blinkOff( );
					app.state = POWER_ON;
				}
			break;


		case SET_SELECT_BAUDRATE:
			if (LinearKeyPad_getKeyState( NEXT_PB ))
				{
					if ( buffer[app.blinkIndex] == '9' )	//reset count, if it is > 9
						buffer[app.blinkIndex] = '0';			
					else buffer[app.blinkIndex]++;					

					buffer[3] = 0X7C; // To display letter 'b' 
					DigitDisplay_updateBuffer_noValidation ( buffer );	
					DigitDisplay_blinkOn_ind(500, app.blinkIndex);							
				}
		
		
			else if ( LinearKeyPad_getKeyState( SET_SEL_PB ))
				{
					app.blinkIndex++;
					if ( app.blinkIndex >= 1)
					{
						app.blinkIndex = 0XFF;
						ASCII_decimal( );
						Write_b_eep (EPROM_ADDRESS+1, app.baudRate_sel);
				
						DigitDisplay_blinkOff( );
						app.state = MENU;
					}
					else
					{
						
						DigitDisplay_blinkOn_ind(500, app.blinkIndex);
					}
				}
			
			else if ( LinearKeyPad_getKeyState( MENU_PB ))
				{
					UINT8 previousData;
					previousData = Read_b_eep (EPROM_ADDRESS+1);
					
					if ( previousData != app.baudRate_sel)
						app.baudRate_sel = previousData;

					app.blinkIndex = 0XFF;
					DigitDisplay_blinkOff( );
					app.state = POWER_ON;
				}
			break;
	
	default: break;

	}
}


// Function to unpack the address and baud select data

void conversion( UINT16 data )
{
	UINT8 i = 0;
	
	do
	{
		buffer[i] = data % 10;
		
		switch( buffer[i] )					// Assigning segment value to the buffer
		{
			case 0: buffer[i] = '0';
					break;
			case 1: buffer[i] = '1';
					break;
			case 2: buffer[i] = '2';
					break;
			case 3: buffer[i] = '3';
					break;
			case 4: buffer[i] = '4';
					break;
			case 5: buffer[i] = '5';
					break;
			case 6: buffer[i] = '6';
					break;
			case 7: buffer[i] = '7';
					break;
			case 8: buffer[i] = '8';
					break;
			case 9: buffer[i] = '9';
					break;
			default: break;
		}
	
	i++;
 
	}while (( data /= 10) > 0);
	
	if ( i < 4)
	{
		for( ; i < 4 ; i++ )
		{
			buffer[i] = '0';     // to fill up the buffer with zeroes
		}
	}

	if (app.currentItem != 0xFF)
	{
		i--;					  // decrementing buffer address to display 'A' or 'b'
		if ( app.currentItem == 1 )
		{
			buffer[i] = 0X7C;     // To display letter 'b' for Baud Rate Selection
		}
		else if ( app.currentItem == 0 )
		{
			buffer[i] = 0X77;     // To display letter 'A' for addresss selection
		}
	}	
}



// Function to select items in  the menu.
void itemSelect( void )
{
	switch(app.currentItem)
	{
	case 0: app.currentItem = 1;				// Change current item to next in the list
			conversion(app.baudRate_sel);
			break;
	
	case 1: app.currentItem = 0;				// Change current item to next in the list
			conversion(app.address);
			break;
	
	default: 
	break;

	}
}


// adc counts to pressure conversion
void countToPressure(void)
{
	UINT32 temp;
	if( app.adc_output < 205 )
		temp = 0;
	else
	{

		temp = ((UINT32)(app.adc_output-205))*313;// (205- corresponds to 1V offset,2.5mBar pressure * adc_output) 
		temp >>= 10;  // divide by 1024 -  max no.of adc counts
		
	}
	usRegInputBuf[0] = app.pressureMbar = (UINT16) temp;
}


// Converting ASCII to DECIMAL
void ASCII_decimal( void )
{
	int i;

	switch ( app.state )
	{
		case SET_SELECT_ADDRESS: app.address = (buffer[2] - '0') * 100;
								 app.address += (buffer[1] - '0') * 10  ;			// subtract 30 and store it in address
								 app.address += (buffer[0] - '0');
		break;
		
		case SET_SELECT_BAUDRATE: app.baudRate_sel = (buffer[2] - '0') * 100;
								  app.baudRate_sel += (buffer[1] - '0') * 10  ;			// subtract 30 and store it in address
								  app.baudRate_sel += (buffer[0] - '0');
		break;	
		
		default:
		break;
	}			

}


eMBErrorCode
eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    int             iRegIndex;

    if( ( usAddress >= REG_INPUT_START )
        && ( usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS ) )
    {
        iRegIndex = ( int )( usAddress - usRegInputStart );
        while( usNRegs > 0 )
        {
            *pucRegBuffer++ =
                ( unsigned char )( usRegInputBuf[iRegIndex] >> 8 );
            *pucRegBuffer++ =
                ( unsigned char )( usRegInputBuf[iRegIndex] & 0xFF );
            iRegIndex++;
            usNRegs--;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}

eMBErrorCode
eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                 eMBRegisterMode eMode )
{
    return MB_ENOREG;
}


eMBErrorCode
eMBRegCoilsCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNCoils,
               eMBRegisterMode eMode )
{
    return MB_ENOREG;
}

eMBErrorCode
eMBRegDiscreteCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNDiscrete )
{
    return MB_ENOREG;
}
