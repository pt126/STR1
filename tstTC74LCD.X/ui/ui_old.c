#include "ui.h"
#include "../mcc_generated_files/mcc.h"
#include "../LCD/lcd.h"
#include "../I2C/i2c.h"
#include <stdio.h>

#define ALAF 0   /*alarm flag – alarms initially disabled*/
#define ALAH 12  /*alarm clock hours*/
#define ALAM 0   /*alarm clock minutes*/
#define ALAS 0   /*alarm clock seconds*/
#define ALAT 20  /*20ºC threshold for temperature alarm*/
#define ALAL 2   /*threshold for luminosity level alarm */

/*-----------------  State Enums ---------------------------*/

typedef enum { UI_NORMAL, UI_CONFIG, UI_SHOW_RECORDS } ui_mode_t;

/* This are the possible selected fields for config, S1 cycles through until normal*/
typedef enum {
    CF_CLK_HH,
    CF_CLK_MM,
    CF_CLK_SS,
    CF_CTL_C,      // select which alarm threshold to edit (clock)
    CF_CTL_T,      // temp threshold
    CF_CTL_L,      // lum threshold
    CF_ALARM_EN,   // A/a toggle
    CF_RESET,      // R reset max/min
    CF_DONE,
    CF_CTL_C_HH,    //incrementing alarm hour
    CF_CTL_C_MM,    //incrementing alarm minute
    CF_CTL_C_SS     //incrementing alarm second

} cfg_field_t;

static ui_mode_t   mode;
static cfg_field_t field;

/*--------------------  Data and thresholds ---------------------------------*/
static uint8_t hh, mm, ss;
static unsigned char temp;
static unsigned char light;
static uint8_t alarmsEnabled = ALAF;       // 0/1  -> shows 'a' or 'A'
static uint8_t alarmFlagC,alarmFlagT,alarmFlagL; // shows letters CTL until S1 clears

// thresholds shown in config mode (spec says tt and l show threshold values there)
uint8_t thrSecond = ALAS;
uint8_t thrMinute = ALAM;
uint8_t thrHour   = ALAH;
uint8_t thrTemp   = ALAT;   // 0 -50
uint8_t thrLum    = ALAL;    // 0 - 3

// simple previous sampled state? for polling buttons once per second
static uint8_t s1_prev = 1, s2_prev = 1;

uint8_t time_inactive;

/*----------- Flags for the button interrupts --------------*/
static volatile int S1_pressed ;
static volatile int S2_pressed ;

/*-----------------------     Utils functions...    ---------------------*/

void Clock_Tick1s(void) /*I really hate this here >=(*/
{
    if (++ss >= 60) { 
        ss = 0; 
        if (++mm >= 60) { 
            mm = 0; 
            if (++hh >= 24) {hh = 0;} 
        } 
    }
}

void ClearAlarmFlags(void) { alarmFlagC = alarmFlagT = alarmFlagL = 0; }

/*-------------------- Button resolving functions ----------------------*/

void OnS1Pressed(void)
{
    // S1 clears CTL *and* is used to enter config and move between fields :contentReference[oaicite:13]{index=13}
    ClearAlarmFlags();
    time_inactive = 0;

    if (mode == UI_NORMAL) {
        mode = UI_CONFIG;
        field = CF_CLK_HH;
        return;
    }

    if (mode == UI_CONFIG) {
        field = (cfg_field_t)(field + 1);
        if (field == CF_DONE) {
            mode = UI_NORMAL;
            field = CF_CLK_HH;
            return;
        }
        if (field > CF_CTL_C_SS){
            field = CF_CTL_T;
        }
        
        return;
    }

    if (mode == UI_SHOW_RECORDS){
        mode = UI_NORMAL;
    }
    return;
}

void OnS2Pressed(void)
{
    time_inactive = 0;
    if (mode == UI_CONFIG) {
        switch (field) {
            case CF_CLK_HH: hh = (hh + 1) % 24;  break;                 // :contentReference[oaicite:14]{index=14}
            case CF_CLK_MM: mm = (mm + 1) % 60;  break;
            case CF_CLK_SS: ss = (ss + 1) % 60;  break;
            case CF_CTL_C:  field = CF_CTL_C_HH; break;
            case CF_CTL_C_HH :thrHour   = (thrHour + 1) % 24; break;
            case CF_CTL_C_MM :thrMinute = (thrMinute + 1) %60; break;
            case CF_CTL_C_SS :thrSecond = (thrSecond + 1) %60; break; 
            case CF_CTL_T:  thrTemp = (thrTemp + 1) % 51; break;       // 0..50 :contentReference[oaicite:15]{index=15}
            case CF_CTL_L:  thrLum  = (thrLum  + 1) % 4;  break;       // 0..3  :contentReference[oaicite:16]{index=16}
            case CF_ALARM_EN: alarmsEnabled ^= 1; break;               // A/a :contentReference[oaicite:17]{index=17}
            case CF_RESET:   /* reset max/min records here */ break;   // 'R' :contentReference[oaicite:18]{index=18}
            default: break;
        }
        return;
    }

    if (mode == UI_NORMAL) {
        // Later: implement ?show saved records sequence with each press of S2? :contentReference[oaicite:19]{index=19}
        // Keep as no-op until EEPROM records exist.
        mode = UI_SHOW_RECORDS;
        return;
    }



    return;
}

/*-------------------    UI functions ----------------------------*/

void UI_Init(void)
{
    // initial/default values from spec (you can later load from EEPROM)
    alarmsEnabled = 0;
    hh = mm = ss = 0;
    thrTemp = 20;
    thrLum  = 2;    // example defaults shown in doc :contentReference[oaicite:20]{index=20}

    mode = UI_NORMAL;
    field = CF_CLK_HH;

    LCDinit();
    RenderNormal();
}

void UI_OnTick1s(void)
{
    if (time_inactive >= TINA) {mode = UI_NORMAL;}

    // Render the UI according to the mode
    if      (mode == UI_NORMAL)      { RenderNormal (); }
    else if (mode == UI_CONFIG)      { RenderConfig (); }
    else if (mode == UI_SHOW_RECORDS){ RenderRecords(); }
    return;

    // 4) LEDs for normal mode (later add other LEDs + PWM alarm)
    // D5/RA7 clock activity: toggle every 1 second :contentReference[oaicite:21]{index=21}
   //LATAbits.LATA7 ^= 1;
    //IO_RA7_Toggle();
}

void RenderRecords(void){
    char line1[17], line2[17];
    sprintf(line1,"   EEPROM      "); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
    sprintf(line2,"   Records     ");                LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());

}

void RenderConfig(void)
{
    /*
    char mC = (field == CF_CTL_C) ? '^' : ' ';
    char mT = (field == CF_CTL_T) ? '^' : ' ';
    char mL = (field == CF_CTL_L) ? '^' : ' ';


        snprintf(line1, sizeof(line1), "%02u:%02u:%02u %c%c%c %cR",
                hh, mm, ss, mC, mT, mL, alarmsEnabled ? 'A' : 'a');

        snprintf(line2, sizeof(line2), "%02u C   L  %u",
                thrTemp, thrLum);

        LCDcmd(0x80); LCDstr(line1);
        LCDcmd(0xC0); LCDstr(line2); */

    char line1[17], line2[17];

    switch (field) {
        case CF_CLK_HH:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80); LCDpos(0,1);
        break;
        case CF_CLK_MM:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,4);
        break;
        case CF_CLK_SS:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,7);
        break;
        case CF_CTL_C:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); /*falta a atualização disto !!!!!!*/
            LCDcmd(0x80);LCDpos(0,11);
        break;
        case CF_CTL_C_HH:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); /*falta a atualização disto !!!!!!*/
            LCDcmd(0x80); LCDpos(0,1);
        break;
        case CF_CTL_C_MM:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); /*falta a atualização disto !!!!!!*/
            LCDcmd(0x80);LCDpos(0,4);
        break;
        case CF_CTL_C_SS:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",thrHour,thrMinute,thrSecond); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                   ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); /*falta a atualização disto !!!!!!*/
            LCDcmd(0x80);LCDpos(0,7);
        break;
        case CF_CTL_T:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"%02d C               ",thrTemp);
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,12);
        break;
        case CF_CTL_L:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"             L %01d ",thrLum); /*<--- isto aqui não está a cooperar*/
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,13);
        break;
        case CF_ALARM_EN:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,14);
        break;
        case CF_RESET:
            if(alarmsEnabled==1)
            {     sprintf(line1,"%02u:%02u:%02u   CTLAR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            else{ sprintf(line1,"%02u:%02u:%02u   CTLaR",hh,mm,ss); LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            sprintf(line2,"                ");
            LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
            LCDcmd(0x80);LCDpos(0,15);
        break;
        default: break;
    }

}

void RenderNormal(void)
{
    char line1[17], line2[17];
    /*clear LCD*/
    sprintf(line1,"                ");
    sprintf(line2,"                ");
    
    LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());

    // Line1: hh:mm:ss CTL A (in normal just A/a)
    // show CTL letters only if flagged
    int any_alarm = alarmFlagC+alarmFlagL+alarmFlagT;
    if (alarmsEnabled == 1){
        if (any_alarm  == 0){ /*no alarm going off*/
            sprintf(line1,"%02u:%02u:%02u      A ",hh,mm,ss);
            LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
        }
        else{
            if (alarmFlagC == 1){
                sprintf(line1,"%02u:%02u:%02u CA ",hh,mm,ss);
                LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            if (alarmFlagT == 1){
                sprintf(line1,"%02u:%02u:%02u TA ",hh,mm,ss);
                LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
            if (alarmFlagL == 1){
                sprintf(line1,"%02u:%02u:%02u LA ",hh,mm,ss);
                LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());}
        }
    }else{/*alarms disabled*/
        sprintf(line1,"%02u:%02u:%02u      a ",hh,mm,ss);
        LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
    }
    
    sprintf(line2,"%02d C         L %c ",temp,light);
    //LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy());
}

/*--------------------    Sensor functions ------------------------*/
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

void ReadSensors(){
    temp = readTC74();
    light = 65; /*switch with proper ADC reading*/
}

/*--------------------    Button Flags ----------------------------*/

void S1_Callback(void){ S1_pressed = 1; }
void S2_Callback(void){ S2_pressed = 1; }
int S1_check(void){return S1_pressed;}
int S2_check(void){return S2_pressed;}
void Reset_flag_S1(void){S1_pressed = 0;}
void Reset_flag_S2(void){S2_pressed = 0;}








