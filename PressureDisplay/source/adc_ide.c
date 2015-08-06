#include "adc_ide.h"

#define SAMPLE_COUNT	20

typedef struct _ADC
{
	UINT8 conversion;
	UINT8 sampleCount;
	INT16 partialResult;
	INT16 result;
}ADC;

ADC adc = {0};
void ADC_init( void )
{
	OpenADC(ADC_FOSC_32 & ADC_RIGHT_JUST & ADC_2_TAD,	 //open adc with required 
			ADC_CH0 & ADC_INT_ON & ADC_REF_VDD_VSS,	 //configuration 
			ADC_1ANA ); 	
	PIE1bits.ADIE = 1;		//Enable adc interrupt
	IPR1bits.ADIP = 1;		//set adc interrupt as high priority
	ConvertADC();
}

void ADC_task(void)
{

	DISABLE_ADC_INTERRUPT();	//entering critical section for adc
	if( adc.conversion == 1 )	//check for completion
	{

		adc.sampleCount++;		//count no. of samples collected
		adc.partialResult += ReadADC();	//accumulate samples

		if( adc.sampleCount >= SAMPLE_COUNT ) 
		{
			adc.result = adc.partialResult/SAMPLE_COUNT;//average sample data
			adc.partialResult = 0;		//reset partial result
			adc.sampleCount = 0;			//reset sample count

		}
		ConvertADC();			//start conversion
		adc.conversion = 0;	//indicate in conversion status
	}
	ENABLE_ADC_INTERRUPT();
}


INT16 ADC_getResult(void)
{
	return adc.result;
}


#pragma interrupt ADC_interrupt
void ADC_interrupt(void)
{
	adc.conversion = 1;	//clear conversion flag to indicate completion
	PIR1bits.ADIF = 0;	//clear conversion completion flag in PIR register
}


			
