#ifndef __ADC__
#define __ADC__

#include "typedefs.h"
#include "board.h"
#include "adc.h"

void ADC_init(void);
void ADC_task(void);
INT16 ADC_getResult(void);
void ADC_interrupt(void);

#endif	//__ADC__