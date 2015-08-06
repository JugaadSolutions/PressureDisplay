/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id: porttimer.c,v 1.1 2006/08/22 21:35:13 wolti Exp $
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

USHORT reload;

/* ----------------------- static functions ---------------------------------*/
void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation -----------------------------*/
BOOL
xMBPortTimersInit( USHORT usTim1Timerout50us )
{
	
	OpenTimer3(TIMER_INT_ON & T1_16BIT_RW & 		// Cofigure TMR1 as 
				T1_SOURCE_INT & T1_PS_1_1 & 		//16 bit counter, 
				T1_OSC1EN_OFF & T1_SYNC_EXT_OFF );  //with internal clock, No prescalar

	T3CONbits.TMR3ON = 0;		//Disable Timer
	// Make timer1 interrupt high priority
  	IPR2bits.TMR3IP = 1;

	reload = usTim1Timerout50us;
    return TRUE;
}


//inline 
void
vMBPortTimersEnable(  )
{
	T3CONbits.TMR3ON = 0;		//Disable Timer
	WriteTimer3(reload);		// Reload 
	T3CONbits.TMR3ON = 1;		//Enable Timer
    
}

//inline 
void
vMBPortTimersDisable(  )
{
		T3CONbits.TMR3ON = 0;	/* Disable any pending timers. */
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
#pragma interrupt prvvTIMERExpiredISR
void prvvTIMERExpiredISR( void )
{
	PIR2bits.TMR3IF = 0;
	
    ( void )pxMBPortCBTimerExpired(  );

}

