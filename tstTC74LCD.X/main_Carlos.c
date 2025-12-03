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

/*
                         Main application
 */

//extern volatile bool second_has_passed;

/*--------------------------------  MACROS(values for variable init mostly) -------------------------------------*/
#define PMON 5   /*5 sec monitoring period*/
#define TALA 3   /*3 seconds duration of alarm signal (PWM)*/
#define TINA 10  /*10 sec inactivity time*/
#define ALAF 0   /*alarm flag – alarms initially disabled*/
#define ALAH 12  /*alarm clock hours*/
#define ALAM 0   /*alarm clock minutes*/
#define ALAS 0   /*alarm clock seconds*/
#define ALAT 20  /*20ºC threshold for temperature alarm*/
#define ALAL 2   /*threshold for luminosity level alarm */
#define CLKH 0   /*clock hours*/
#define CLKM 0   /*clock minutes*/

#define NORMAL_MODE 0
#define CONFIG_MODE 1
#define RECORD_MODE 2

#define NO_ALARM 0
#define TIME_ALARM 1
#define TEMP_ALARM 2
#define LIGHT_ALARM 3

#define CONFIG_SELECT_HOUR   0
#define CONFIG_SELECT_MINUTE 1
#define CONFIG_SELECT_SECOND 2
#define CONFIG_SELECT_C      3
#define CONFIG_SELECT_T      4
#define CONFIG_SELECT_L      5
#define CONFIG_SELECT_A      6
#define CONFIG_SELECT_R      7

#define true  1
#define false 0


/*------------------GLOBAL VARIABLES(good for interrupt handling) ----------------*/
extern volatile int curr_mode        = NORMAL_MODE;
extern volatile int alarm_flag       = NO_ALARM;
extern volatile int config_selection = CONFIG_SELECT_HOUR;

extern volatile int inactive_time      = 0;  /*for counting the time in record mode*/
extern volatile int pwm_active_time    = -1; /*start at -1 to even out the increments*/


extern volatile int second_has_passed      = false;
extern volatile int S1_pressed             = false;
extern volatile int S2_pressed             = false;
extern volatile int alarms_enabled         = false;
extern volatile int sound_PWM              = false;

extern volatile int alarm_hour   = ALAS;
extern volatile int alarm_minute = ALAM;
extern volatile int alarm_second = ALAH;
extern volatile int alarm_temp   = ALAT;
extern volatile int alarm_light  = ALAL;

extern volatile char buf[17]; /*buffer to write to LCD display*/

extern volatile struct SensorData{
  unsigned long seconds = 0;
  unsigned long hours, minutes, secs;
  unsigned char temp;
  unsigned char light;
}


/*---- Function primitves(in C we must declare stuff before calling them, definition is a the bottom)*/
void resolve_NORMAL_MODE(void);
void resolve_CONFIG_MODE(void);
void resolve_RECORD_MODE(void);
void resolve_OUT_OF_BOUNDS(void);


unsigned char readTC74 (void)
{
	unsigned char value;
  do{
    IdleI2C();
    StartI2C(); IdleI2C();
      
    WriteI2C(0x9a | 0x00); IdleI2C();
    WriteI2C(0x01); IdleI2C();
    RestartI2C(); IdleI2C();
    WriteI2C(0x9a | 0x01); IdleI2C();
    value = ReadI2C(); IdleI2C();
    NotAckI2C(); IdleI2C();
    StopI2C();
  } while (!(value & 0x40));

    IdleI2C();
    StartI2C(); IdleI2C();
    WriteI2C(0x9a | 0x00); IdleI2C();
    WriteI2C(0x00); IdleI2C();
    RestartI2C(); IdleI2C();
    WriteI2C(0x9a | 0x01); IdleI2C();
    value = ReadI2C(); IdleI2C();
    NotAckI2C(); IdleI2C();
    StopI2C();

	return value;
}

unsigned char readLight(void){
  
  return readTC74();

}

void main(void)
{    
    // initialize the device
    SYSTEM_Initialize();


    // When using interrupts, you need to set the Global and Peripheral Interrupt Enable bits
    // Use the following macros to:

    // Enable the Global Interrupts
    INTERRUPT_GlobalInterruptEnable();

    // Enable the Peripheral Interrupts
    INTERRUPT_PeripheralInterruptEnable();

    // Disable the Global Interrupts
    //INTERRUPT_GlobalInterruptDisable();

    // Disable the Peripheral Interrupts
    //INTERRUPT_PeripheralInterruptDisable();

    OpenI2C();
    //I2C_SCL = 1;
    //I2C_SDA = 1;
    //WPUC3 = 1;
    //WPUC4 = 1;
    LCDinit();



    while (1)
    {
      second_has_passed = AppClock_ConsumeTick1s();
      S1_pressed = S1_check();
      S2_pressed = S2_check();

      if     (curr_mode == NORMAL_MODE){resolve_NORMAL_MODE();}
      else if(curr_mode == CONFIG_MODE){resolve_CONFIG_MODE();}
      else if(curr_mode == RECORD_MODE){resolve_RECORD_MODE();}
      else{resolve_OUT_OF_BOUNDS();}
        
    }
}

void resolve_NORMAL_MODE(){


  if (second_has_passed==true) {
    
    /*update the clock*/
    second_has_passed = false;
    SensorData.seconds++;
    SensorData.hours = (SensorData.seconds / 3600) % 24;
    SensorData.minutes = (SensorData.seconds / 60) % 60;
    secs = SensorData.seconds % 60;

    LCDcmd(0x80);       //first line, first column
    sprintf(buf, "%02lu:%02lu:%02lu", SensorData.hours, SensorData.minutes, SensorData.secs);
    LCDstr(buf);
    while (LCDbusy());

    if( secs%PMON == 0 ){
      /*update temperature reading*/
      SensorData.temp = readTC74();
      LCDcmd(0xc0);       // second line, first column
      sprintf(buf, "%02d C", SensorData.temp); LCDstr(buf);
      
      while (LCDbusy());

      /*update light reading*/
      SensorData.light = readLight();
      LCDcmd(0xc0); LCDpos(0,13);      // second line, first column
      sprintf(buf, "L %1d", SensorData.light); LCDstr(buf);
      while (LCDbusy());
    }

    if(alarms_enabled == true){

      /*show on LCD "A"*/
      LCDcmd(0x80);       // second line, first column
      LCDpos(0,15);
      LCDstr("A");
      while(LDCbusy());

      /*check if alarm should go off for time*/
      if (SensorData.hours == alarm_hour){
        if(SensorData.minutes == alarm_minute){
          if(SensorData.seconds == alarm_second){
            alarm_flag = TIME_ALARM;
          }
        }
      }

      /*check if alarm should go off for temperature*/
      if(SensorData.temp > alarm_temp){
        alarm_flag = TEMP_ALARM;

      }
      
      /*check if alarm should go off for light*/
      if(SensorData.light < alarm_light){
        alarm_flag = LIGHT_ALARM;
      }

      /*resolve alarm situation*/
      switch (curr_alarm){
        case TIME_ALARM : 
          /*LCD shows C*/
          LCDcmd(0x80);      
          LCDpos(0,13);   // line 1, column 13
          LCDstr("C");
        break;
        case TEMP_ALARM : 
          /*LCD shows T*/
          LCDcmd(0x80);       
          LCDpos(0,13);   // line 1, column 13
          LCDstr("T"); 
        break;
        case LIGHT_ALARM: 
          /*LCD shows L*/
          LCDcmd(0x80);       
          LCDpos(0,13);  // line 1, column 13
          LCDstr("L");
        break;
        default:
        }else{
          LCDcmd(0x80);       
          LCDpos(0,15);   // line 1, column 13
          LCDstr("a");
      }

    }

    SLEEP();
    NOP();

  }/*end of every second code*/
  
  if (S1_pressed == true){
    if (alarm_flag != NO_ALARM){alarm_flag == NO_ALARM;}
    else{ /*there was no alarm going off, lets go config mode*/
      curr_mode = CONFIG_MODE;
      /*show config mode screen*/
      LCDcmd(0x80);       //first line, first column
      if(alarms_enabled){
        sprintf(buf, "%02lu:%02lu:%02lu  CTL AR", alarm_hour,alarm_minute,alarm_second);
      }else{
        sprintf(buf, "%02lu:%02lu:%02lu  CTL aR", alarm_hour,alarm_minute,alarm_second);
      }
      LCDstr(buf);
      while (LCDbusy());
      LCDcmd(0xC0);       //first line, first column
      sprintf(buf, "%02lu C         L %02lu", alarm_temp, alarm_light);
      LCDstr(buf);
      while (LCDbusy());
      return;
    }
  }

  if (S2_pressed == true){
    curr_mode = RECORD_MODE;
    /*show record mode screen*/
    return;

  }

}

void resolve_CONFIG_MODE(){

  if (second_has_passed==true) {
    /*update the clock*/
    second_has_passed = false;
    SensorData.seconds++;
    SensorData.hours = (SensorData.seconds / 3600) % 24;
    SensorData.minutes = (SensorData.seconds / 60) % 60;
    secs = SensorData.seconds % 60;

    if (config_selection != CONFIG_SELECT_C){/*if alarm clock is not being configured update clock*/
      LCDcmd(0x80);       //first line, first column
      sprintf(buf, "%02lu:%02lu:%02lu", SensorData.hours, SensorData.minutes, SensorData.secs);
      LCDstr(buf);
      while (LCDbusy());
    }
  }

  if(S1_pressed == true){ 
    config_selection ++; /*move selection to the next one*/
    switch (config_selection){
      case CONFIG_SELECT_HOUR   : 
        /*move cursor to the right of hours*/
        LCDcmd(0x80);LCDpos(0,1);
        
      break;
      case CONFIG_SELECT_MINUTE : 
        /*move cursor to the right of minutes*/
        LCDcmd(0x80);LCDpos(0,4);
        
      break;
      case CONFIG_SELECT_SECOND : 
        /*move cursor to the right of seconds*/
        LCDcmd(0x80);LCDpos(0,7);
        
      break;
      case CONFIG_SELECT_C      : 
        /*move cursor to C*/
        LCDcmd(0x80);
        LCDpos(0,10);
        /*show alarm clock value*/
        LCDcmd(0x80);
        LCDpos(0,0);
        sprintf(buf, "%02lu:%02lu:%02lu", alarm_hour, alarm_minute, alarm_second);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,10);
      break;
      case CONFIG_SELECT_T      : 
        /*move cursor to T*/
        LCDcmd(0x80);LCDpos(0,11);
        
        /*show current time value*/
        /*show alarm temperature value*/
      break;
      case CONFIG_SELECT_L      : 
        /*move cursor to L*/
        LCDcmd(0x80);LCDpos(0,12);
        
        /*show alarm light value*/
      break;
      case CONFIG_SELECT_A      : 
        /*move cursor to A*/
        LCDcmd(0x80);LCDpos(0,14);
        
      break;
      case CONFIG_SELECT_R      : 
        /*move cursor to R*/
        LCDcmd(0x80);LCDpos(0,15);
        
      break;
      default:
        /*went out of selections, exit config mode*/
        curr_mode = NORMAL_MODE;
        /*show normal screen*/
      return;
    }
  }
  
  if(S2_pressed == true){ 
    switch (config_selection){
      case CONFIG_SELECT_HOUR   : 
        SensorData.seconds += 3600;
        SensorData.hours ++;
        /*show updated value*/
        LCDcmd(0x80);
        LCDpos(0,0);
        sprintf(buf, "%02lu:%02lu:%02lu", SensorData.hours, SensorData.minutes, SensorData.secs);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,1);
      break;
      case CONFIG_SELECT_MINUTE : 
        SensorData.seconds += 60;
        SensorData.minutes ++;
        /*show updated value*/
        LCDcmd(0x80);
        LCDpos(0,0);
        sprintf(buf, "%02lu:%02lu:%02lu", SensorData.hours, SensorData.minutes, SensorData.secs);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,4);
      break;
      case CONFIG_SELECT_SECOND : 
        SensorData.seconds ++;
        SensorData.secs ++;
        /*show updated value*/
        LCDcmd(0x80);
        LCDpos(0,0);
        sprintf(buf, "%02lu:%02lu:%02lu", alarm_hour, alarm_minute, alarm_second);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,10);
      break;
      case CONFIG_SELECT_C      : 
        alarm_second ++;
        if (alarm_second == 60){
          alarm_second = 0;
          alarm_minute++;
        }
        if (alarm_minute==60){
          alarm_minute = 0;
          alarm_hour ++;
        }
        if(alarm_hour == 24){
          alarm_hour = 0;
        }
        /*show updated value*/
        LCDcmd(0x80);
        LCDpos(0,0);
        sprintf(buf, "%02lu:%02lu:%02lu", SensorData.hours, SensorData.minutes, SensorData.secs);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,7);

      break;
      case CONFIG_SELECT_T      : 
        alarm_temp  = (alarm_temp+1)%51;
        /*show updated value*/
        LCDcmd(0xC0);
        LCDpos(8,0);
        sprintf(buf, "%02d C", alarm_temp);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDcmd(0x80);
        LCDpos(0,11);
      break;
      case CONFIG_SELECT_L      : 
        alarm_light = (alarm_light+1)%4;
        /*show updated value*/
        LCDcmd(0xC0);
        LCDpos(8,13);
        sprintf(buf, "%02d C", alarm_temp);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDcmd(0x80);
        LCDpos(0,12);
      break;
      case CONFIG_SELECT_A      : 
        alarms_enabled = !alarms_enabled;
        /*show updated value*/
        LCDcmd(0x80);
        LCDpos(0,14);
        if(alarms_enabled==true){LCDstr("A");}
        else{LCDstr("a");}
        while (LCDbusy());
        /*reposition cursor*/
        LCDpos(0,14);
      break;
      case CONFIG_SELECT_R      :
        /*erase EEPROM records*/
        /*flash erase message to user*/
        LCDcmd(0xC0);
        LCDpos(8,0);
        sprintf(buf, "Record reset",);
        LCDstr(buf);
        while (LCDbusy());
        __delay_ms(500);
        LCDpos(8,0);
        sprintf(buf, "%02lu C          ", alarm_temp);
        LCDstr(buf);
        while (LCDbusy());
        /*reposition cursor*/
        LCDcmd(0x80);
        LCDpos(0,15);
      break;
      default:
      /*went out of selections, exit config mode*/
      curr_mode = NORMAL_MODE;
      LCDcmd(0x80);       //first line, first column
      if(alarms_enabled){
        sprintf(buf, "%02lu:%02lu:%02lu      A ", SensorData.hours,SensorData.minutes,SensorData.seconds);
      }else{
        sprintf(buf, "%02lu:%02lu:%02lu      a ", SensorData.hours,SensorData.minutes,SensorData.seconds);
      }
      LCDstr(buf);
      while (LCDbusy());
      LCDcmd(0xC0);       //first line, first column
      sprintf(buf, "%02d C         L %02lu",SensorData.temp,SensorData.light);
      LCDstr(buf);
      while (LCDbusy());
      return;
    }
  }

}

void resolve_RECORD_MODE(){

   if (second_has_passed==true) {
    /*update the clock*/
    second_has_passed = false;
    SensorData.seconds++;
    SensorData.hours = (SensorData.seconds / 3600) % 24;
    SensorData.minutes = (SensorData.seconds / 60) % 60;
    secs = SensorData.seconds % 60;

    inactive_time ++; /*increase inactive time, can cleared ahead if a button was pressed*/

   }

   if(S1_pressed == true){ inactive_time = 0;}
   if(S2_pressed == true){ inactive_time = 0;}

   /*show records and be responsive*/

  if (inactive_time > TINA){ /*timeout->return to normal mode*/
    inactive_time = 0;
    curr_mode = NORMAL_MODE;
  }


}

void resolve_OUT_OF_BOUNDS(){
  /*no idea how we got here...*/
  /*nice job =)*/
  curr_mode = NORMAL_MODE;
}


/**
 End of File
*/