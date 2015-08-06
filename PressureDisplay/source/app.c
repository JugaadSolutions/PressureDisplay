#include "app.h" 
#include "eep.h"
#include "adc_ide.h"
#include "mb.h"
#include "rtc_driver.h"
#include "timer.h"

#define REG_INPUT_START 1
#define REG_INPUT_NREGS 1

/*
*------------------------------------------------------------------------------
* Public Variables
* Buffer[0] = seconds, Buffer[1] = minutes, Buffer[2] = Hour,
* Buffer[3] = day, Buffer[4] = date, Buffer[5] = month, Buffer[6] = year
*------------------------------------------------------------------------------
*/

//UINT8 readTimeDateBuffer[7] = {0};
UINT8 writeTimeDateBuffer[] = {0X00, 0X00, 0X00, 0X01, 0x01, 0X01, 0X14};



void conversion( UINT16 );
void itemSelect( void );
void countToPressure(void);
void ASCII_decimal( void );
void storeTime( void );
void resetBuffer( void );
void updateRTC( void );
void updateTime( void );
void updatePressure( void );



/* ----------------------- Static variables ---------------------------------*/
static USHORT   usRegInputStart = REG_INPUT_START;
static USHORT   usRegInputBuf[REG_INPUT_NREGS];

typedef struct _App
{
	APP_STATE state;
	SUB_STATE subState;
	UINT8 baudRate_sel; 			// To store the baud rate from eeprom
	UINT8 address;					// To store the address from eeprom 
	UINT8 currentItem;   			// To display next item in the menu	
	UINT16 adc_output;  			// ADC output in counts
	UINT16 pressureMbar;			// ADC output in pressure
	UINT16 blinkIndex; 				// Used to point the digit to be blinked

	UINT8 readTimeDateBuffer[7];	// Used to store the data from the rtc
	UINT8 writeTimeDateBuffer[7];   // Used to write the data to the rtc

	UINT32 previousTime;
	UINT32 currentAppTime;
}APP;

#pragma idata app_data
UINT8 buffer[4] = {0};
APP app = {0};
#pragma idata

void APP_init( void )
{
	eMBErrorCode    eStatus;

	app.state =  POWER_ON;  // set the app to ADC output state

	//Retrieve the modbus address and buad rate from eeprom
	app.address = Read_b_eep (EPROM_ADDRESS); 
	Busy_eep(  );
	app.baudRate_sel = Read_b_eep (EPROM_ADDRESS+1);
	Busy_eep(  );

	// reset blink index
	app.blinkIndex = 0XFF;  
	app.currentItem = 0xFF;

	// Display dot in the output
	DigitDisplay_dotOn( );					

	// initialize the modubs
	eStatus = eMBInit( MB_RTU,app.address, 0, app.baudRate_sel, MB_PAR_NONE);

	// Enable the Modbus Protocol Stack. 
    eStatus = eMBEnable(  );	

	DigitDisplay_dotBlinkOn( 1, 2, 50000 );
}

void APP_task( void )
{
	switch( app.state )
	{
		case POWER_ON: 

			// To bypass menu items  in display
			app.currentItem = 0xFF;	

			// When MENU_PB is pressed show first item in the menu
			if( LinearKeyPad_getKeyState( MENU_PB ) == TRUE )   	 
			{	
				app.currentItem = MB_SLAVE_ID;
				conversion( app.address );
				DigitDisplay_updateBuffer_noValidation ( buffer );
							
				app.state = MENU;					 	 // else display ADC output
			}

			app.currentAppTime = GetAppTime();
			if( ( app.currentAppTime - app.previousTime ) >= SWITCHING_TIME )
			{
				if( app.subState == SHOW_CLOCK )
					app.subState = SHOW_PRESSURE;
				else
				{
					app.subState = SHOW_CLOCK;
					DigitDisplay_dotBlinkOn( 1, 2, 50000 );
				}

				app.previousTime = GetAppTime(  );
			}

			switch( app.subState )
			{
				case SHOW_CLOCK:
					updateTime(  );
				break;

				case SHOW_PRESSURE:
					updatePressure(  );    
				break;
			
				default:
				break;
			}
			
	

		break;

		case MENU: 
			
			DigitDisplay_dotOff( );		

			// In MENU if MENU_PB is pressed goto ADC o/p
			if( LinearKeyPad_getKeyState( MENU_PB ) )	 
			{
				app.state = POWER_ON;
				// Display dot in the output
				DigitDisplay_dotOn( );						
			}
			

			// If NEXT_PB is pressed show items in the menu
			else if( LinearKeyPad_getKeyState( NEXT_PB ) )     
			{
				// call fuction to select the different items from menu
				itemSelect(  );								  
				DigitDisplay_updateBuffer_noValidation (buffer);
			}
			
			else if( LinearKeyPad_getKeyState( SET_SEL_PB ) )
			{	
				app.blinkIndex = 0;
				DigitDisplay_blinkOn_ind(500, app.blinkIndex);
			
				if( app.currentItem == MB_SLAVE_ID )
					app.state = SET_SELECT_ADDRESS;
				else if( app.currentItem == MB_BAUD_RATE )
					app.state = SET_SELECT_BAUDRATE;
				else if( app.currentItem == CLOCK )
					app.state = SET_CLOCK;
			}
								
		
		break;
		
		case SET_SELECT_ADDRESS:

			//If the next pb is pressed increment the value of the digit
			//where the current index is pointing
			if( LinearKeyPad_getKeyState( NEXT_PB ) )
			{		
				if ( buffer[app.blinkIndex] == '9' )//reset count, if it is > 9
					buffer[app.blinkIndex] = '0';	
	
				else buffer[app.blinkIndex]++;						
								
				// To display letter 'A' in the output 77
				buffer[3] = 0X77; 
				DigitDisplay_updateBuffer_noValidation ( buffer );	
				DigitDisplay_blinkOn_ind(500, app.blinkIndex);							
			}
	
			//If the next pb is pressed increment the value of the digit
			//where the current index is pointing
			else if( LinearKeyPad_getKeyState( SET_SEL_PB ) )
			{	
				//Point to the next digit 				
				app.blinkIndex++;

				if ( app.blinkIndex >= 3)
				{
					app.blinkIndex = 0XFF;
					ASCII_decimal( );

					//store the changed address
					Write_b_eep(EPROM_ADDRESS, app.address);
					Busy_eep(  );
			
					DigitDisplay_blinkOff( );
					app.state = MENU;
				}				
				else 
					DigitDisplay_blinkOn_ind(500, app.blinkIndex);

				
			}
	
			//If the Menu pb is pressed check for the previous address
			//if both are same exit, else store the previous address from EEPROM		
			else if (LinearKeyPad_getKeyState( MENU_PB ))
			{	
				UINT8 previousData;
				previousData = Read_b_eep (EPROM_ADDRESS);
				Busy_eep(  );
				
				if ( previousData != app.address)
						app.address = previousData;
			
				app.blinkIndex = 0xFF;

				DigitDisplay_blinkOff( );
				app.state = POWER_ON;
			}
		break;


		case SET_SELECT_BAUDRATE:

			//If the next pb is pressed increment the value of the digit
			//where the current index is pointing
			if( LinearKeyPad_getKeyState( NEXT_PB ) )
			{
				if ( buffer[app.blinkIndex] == '9' )	//reset count, if it is > 9
					buffer[app.blinkIndex] = '0';			
				else buffer[app.blinkIndex]++;					

				// To display letter 'b' 
				buffer[3] = 0X7C; 
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

			//If the Menu pb is pressed check for the previous baud rate
			//if both are same exit, else store the previous baud rate from EEPROM			
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
		
		case SET_CLOCK:
			//If the next pb is pressed increment the value of the digit
			//where the current index is pointing
			if( LinearKeyPad_getKeyState( NEXT_PB ) )
			{
				// Time max is 23:59
				// If hour MSB is greater than 2
				if( buffer[app.blinkIndex] >= '2' && app.blinkIndex == 3 )	
					buffer[app.blinkIndex] = '0';	
				// If hour LSB is greater than 3
				else if( buffer[app.blinkIndex] >= '3' && app.blinkIndex == 2 )	
					buffer[app.blinkIndex] = '0';
				// If minute MSB is greater than 5
				else if( buffer[app.blinkIndex] >= '5' && app.blinkIndex == 1 )
					buffer[app.blinkIndex] = '0';	
				// If minute LSB is greater than 9			
				else if( buffer[app.blinkIndex] >= '9' && app.blinkIndex == 0 )
					buffer[app.blinkIndex] = '0'; 						
				else 
					buffer[app.blinkIndex]++;					

				DigitDisplay_updateBuffer_noValidation ( buffer );	
				DigitDisplay_blinkOn_ind( 500, app.blinkIndex );							
			}
	
		
			else if( LinearKeyPad_getKeyState( SET_SEL_PB ) )
			{
				app.blinkIndex++;

				if( app.blinkIndex >= 3 )
				{
					app.blinkIndex = 0XFF;

					updateRTC(  );
			
					DigitDisplay_blinkOff(  );
					app.state = MENU;
				}
				else
				{		
					DigitDisplay_blinkOn_ind( 500, app.blinkIndex );
				}
			}

			else if ( LinearKeyPad_getKeyState( MENU_PB ))
			{
	
				resetBuffer(  );
				app.blinkIndex = 0XFF;
				DigitDisplay_blinkOff( );
				app.state = POWER_ON;
			}
		break;
	
		default: 
		break;

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
		case MB_SLAVE_ID: 
			app.currentItem = MB_BAUD_RATE;				// Change current item to next in the list
			conversion( app.baudRate_sel );
		break;
		
		case MB_BAUD_RATE: 
			app.currentItem = CLOCK;				// Change current item to next in the list
			resetBuffer(  );
			DigitDisplay_updateBuffer( buffer );
		break;

		case CLOCK:
			app.currentItem = MB_SLAVE_ID;				// Change current item to next in the list
			conversion( app.address );
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

	switch( app.state )
	{
		case SET_SELECT_ADDRESS: 
			app.address = (buffer[2] - '0') * 100;
			app.address += (buffer[1] - '0') * 10  ;			// subtract 30 and store it in address
			app.address += (buffer[0] - '0');
		break;
		
		case SET_SELECT_BAUDRATE: 
			app.baudRate_sel = (buffer[2] - '0') * 100;
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


void updateTime( void )
{
	//Read the data from RTC
	ReadRtcTimeAndDate( app.readTimeDateBuffer );  

	//Store date and time in the buffer
	buffer[0] = ( app.readTimeDateBuffer[1] & 0X0F ) + '0';        		//Minute LSB
	buffer[1] = ( ( app.readTimeDateBuffer[1] & 0XF0 ) >> 4 ) + '0'; 	//Minute MSB
	buffer[2] = ( app.readTimeDateBuffer[2] & 0X0F ) + '0';				//Hour LSB
	buffer[3] = ( ( app.readTimeDateBuffer[2] & 0X30 ) >> 4 ) + '0'; 	//Hour MSB

	DigitDisplay_updateBuffer( buffer );
}


void resetBuffer( void )
{
	int i ;
	for(i = 0; i < 4; i++)			//reset all digits
	{
		buffer[i] = '0';
	}
}	



void updateRTC( void )
{
	app.writeTimeDateBuffer[1] = ( ( buffer[1] - '0') << 4 ) | ( buffer[0] - '0' ); 	//store minutes
	app.writeTimeDateBuffer[2] = ( ( buffer[3] - '0') << 4 ) | ( buffer[2] - '0' ); //store Hours

#if defined (MODE_12HRS)
	//Set 6th bit of Hour register to enable 12 hours mode.
	writeTimeDateBuffer[2] |= 0x40;
#endif
	WriteRtcTimeAndDate( app.writeTimeDateBuffer );  //update RTC
}


void updatePressure( void )
{
	app.adc_output = ADC_getResult(  );
	countToPressure(  );

	// Convert ADC output to segment value
	conversion( app.pressureMbar );		

#ifdef DISPLAY_ADC_OUTPUT			 
	conversion( app.adc_output );
#endif

	// Display ADC output
	DigitDisplay_updateBuffer_noValidation( buffer );   
}