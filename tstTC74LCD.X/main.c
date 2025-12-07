/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.6
        Device            :  PIC16F18875
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"
#include "I2C/i2c.h"
#include "LCD/lcd.h"
#include "stdio.h"
#include "Clock/clock.h"
#include <stdint.h>
#include "ui/ui.h"



void main(void)
{
    unsigned char c;
    char buf[17];
    char buff[19];
    uint32_t last_update = 0;

    SYSTEM_Initialize();

    OpenI2C();
    LCDinit();
    AppClock_Init();
    LCDcmd(0x80); LCDpos(0,0);LCDstr("Antes do INIT"); while (LCDbusy());
    UI_Init();
    
    
    /*S1 interrupt setup*/
    EXT_INT_Initialize();
    INT_SetInterruptHandler(S1_Callback); 
    
    
    /*S2 setup*/
    PIN_MANAGER_Initialize();
    IOCCF5_SetInterruptHandler(S2_Callback);

    
    while (1)
    {
        if (S1_check() ){Reset_flag_S1(); OnS1Pressed();}
        if (S2_check() ){Reset_flag_S2(); OnS2Pressed();}

        if (AppClock_ConsumeTick1s()) /*one second has passed*/
        {
            IO_RA7_Toggle(); //status LED
            Clock_Tick1s(); /*tick the clock inside the interrupt!!*/
            uint32_t now = AppClock_Seconds();     
            
            if ((now - last_update) >= pmon | last_update == 0)
            {
                last_update = now;
                ReadSensors();    
            }
            
            UI_OnTick1s();   // updates hh:mm:ss and renders again    
            SLEEP();
            NOP();
        }
    }
}
