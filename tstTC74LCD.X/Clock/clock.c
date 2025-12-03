#include <xc.h>
#include "../mcc_generated_files/mcc.h"
#include "../mcc_generated_files/tmr1.h"
#include "clock.h"

static volatile uint32_t s_seconds = 0;
static volatile uint8_t  s_tick1s  = 0;

static void TMR1_1sCallback(void)
{
    s_seconds++;
    s_tick1s = 1;
    LATAbits.LATA7 ^= 1;   // debug: blink each second
}

void AppClock_Init(void)
{
    // Configure LED once (NOT inside while(1))
    ANSELAbits.ANSA7 = 0;
    TRISAbits.TRISA7 = 0;
    LATAbits.LATA7 = 0;

    TMR1_SetInterruptHandler(TMR1_1sCallback);
    TMR1_StartTimer();

    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
}

uint32_t AppClock_Seconds(void) { return s_seconds; }

uint8_t AppClock_ConsumeTick1s(void)
{
    if (s_tick1s) { s_tick1s = 0; return 1; }
    return 0;
}
