#include "ui.h"
#include "../mcc_generated_files/mcc.h"
#include "../mcc_generated_files/adcc.h"
#include "../LCD/lcd.h"
#include "../I2C/i2c.h"
#include "../EEPROM/EEPROM.h"
#include <stdio.h>


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

typedef enum {
    RECORD_NONE,
    RECORD_TEMP,
    RECORD_LUM,
} record_type_t;

static ui_mode_t   mode;
static cfg_field_t field;
static record_type_t record_type;

/*--------------------  Data and thresholds ---------------------------------*/
static uint8_t hh, mm, ss;
static unsigned char temp;
static unsigned char light;
static uint8_t alarmsEnabled = ALAF;       // 0/1  -> shows 'a' or 'A'
static uint8_t alarmFlagC, alarmFlagT, alarmFlagL; // shows letters CTL until S1 clears

typedef struct {
    uint8_t clk_hh;
    uint8_t clk_mm;
    uint8_t clk_ss;
    uint8_t temp;
    uint8_t lum;
} record_field_t;

record_field_t records[4]; // [0 - max temp, 1 - min temp, 2 - max lum, 3 - min lum]
uint8_t record_index = 0;

// thresholds shown in config mode (spec says tt and l show threshold values there)~
uint8_t pmon = PMON;  // monitoring period
uint8_t tala = TALA;  // alarm signal duration
uint8_t tina = TINA;  // inactivity time
uint8_t thrSecond = ALAS;
uint8_t thrMinute = ALAM;
uint8_t thrHour   = ALAH;
char thrTemp   = ALAT;   // 0 -50
char thrLum    = ALAL;    // 0 - 3

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
            EEPROM_WriteConfig(EEPROM_CONFIG_CLKM, mm);
            if (++hh >= 24) { 
                hh = 0; 
                EEPROM_WriteConfig(EEPROM_CONFIG_CLKH, hh);
            } else {
                EEPROM_WriteConfig(EEPROM_CONFIG_CLKH, hh);
            }
        } else {
            EEPROM_WriteConfig(EEPROM_CONFIG_CLKM, mm);
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
        if(record_type == RECORD_TEMP){
            mode = UI_NORMAL;
            record_type = RECORD_NONE;
            record_index = 0;
            return;
        }
        else if (record_type == RECORD_LUM){
            record_type = (record_type_t)(record_type - 1);
            record_index -= 2;
            return;
        }
    }
    return;
}

void OnS2Pressed(void)
{
    time_inactive = 0;
    if (mode == UI_CONFIG) {
        switch (field) {
            case CF_CLK_HH: 
                hh = (hh + 1) % 24;
                EEPROM_WriteConfig(EEPROM_CONFIG_CLKH, hh);
                break;           
            case CF_CLK_MM: 
                mm = (mm + 1) % 60;  
                EEPROM_WriteConfig(EEPROM_CONFIG_CLKM, mm);
                break;
            case CF_CLK_SS: 
                ss = (ss + 1) % 60;  
                break;
            case CF_CTL_C:  field = CF_CTL_C_HH; break;
            case CF_CTL_C_HH :
                thrHour = (thrHour + 1) %24;
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAH, thrHour);
                break;
            case CF_CTL_C_MM :
                thrMinute = (thrMinute + 1) %60;
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAM, thrMinute);
                break;
            case CF_CTL_C_SS :
                thrSecond = (thrSecond + 1) %60;
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAS, thrSecond);
                break; 
            case CF_CTL_T:  
                thrTemp = (thrTemp + 1) % 51; 
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAT, thrTemp);
                break;       // 0..50 
            case CF_CTL_L:  
                thrLum  = (thrLum  + 1) % 4;
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAL, thrLum);
                break;       // 0..3 
            case CF_ALARM_EN: 
                alarmsEnabled ^= 1; 
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAF, alarmsEnabled);
                break;       // A/a 
            case CF_RESET:   
                ClearRecords(); 
                break;   // 'R'
            default: break;
        }
        return;
    }

    if (mode == UI_NORMAL) {
        // Later: implement ?show saved records sequence with each press of S2? :contentReference[oaicite:19]{index=19}
        // Keep as no-op until EEPROM records exist.
        mode = UI_SHOW_RECORDS;
        record_type = RECORD_TEMP;
        record_index = 0;
        return;
    }

    if(mode == UI_SHOW_RECORDS){
        if(record_type == RECORD_LUM){
            mode = UI_NORMAL;
            record_type = RECORD_NONE;
            record_index = 0;
            return;
        } 
        else if (record_type == RECORD_TEMP)  {
            record_type = (record_type_t)(record_type + 1);
            record_index += 2;
            return;
        }
        
    }



    return;
}

/*-------------------    UI functions ----------------------------*/

void UI_Init(void)
{
    uint16_t magic;
    uint8_t stored_checksum, calc_checksum = 0;

    EEPROM_ReadHeader(&magic, &stored_checksum);

    if (magic != MAGIC_WORD) {
        // EEPROM not initialized, set defaults and write magic word
        pmon = PMON;
        tala = TALA;
        tina = TINA;
        alarmsEnabled = ALAF;
        thrHour   = ALAH;
        thrMinute = ALAM;
        thrSecond = ALAS;
        thrTemp   = ALAT;
        thrLum    = ALAL;
        hh = CLKH;
        mm = CLKM;
        ss = 0;

        // Save default config to EEPROM
        EEPROM_WriteConfig(EEPROM_CONFIG_PMON, pmon);
        EEPROM_WriteConfig(EEPROM_CONFIG_TALA, tala);
        EEPROM_WriteConfig(EEPROM_CONFIG_TINA, tina);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAF, alarmsEnabled);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAH, thrHour);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAM, thrMinute);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAS, thrSecond);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAT, thrTemp);
        EEPROM_WriteConfig(EEPROM_CONFIG_ALAL, thrLum);

        ClearRecords();

        // Calculate and write checksum
        calc_checksum = 0;
        for (uint8_t i = 0; i <= EEPROM_CONFIG_CLKM; i++) {
            calc_checksum += EEPROM_ReadConfig(i);
        }
        EEPROM_WriteHeader(MAGIC_WORD, calc_checksum);

    } else {
        // EEPROM is initialized, check checksum
        calc_checksum = 0;
        for (uint8_t i = 0; i <= EEPROM_CONFIG_CLKM; i++) {
            calc_checksum += EEPROM_ReadConfig(i);
        }
        if (stored_checksum != calc_checksum) {
            // Checksum mismatch, handle error or reinitialize as above
            // For now, reinitialize:
            UI_Init();
            return;
        }
        // Load config and records from EEPROM
        pmon = EEPROM_ReadConfig(EEPROM_CONFIG_PMON);
        tala = EEPROM_ReadConfig(EEPROM_CONFIG_TALA);
        tina = EEPROM_ReadConfig(EEPROM_CONFIG_TINA);
        alarmsEnabled = EEPROM_ReadConfig(EEPROM_CONFIG_ALAF);
        thrHour   = EEPROM_ReadConfig(EEPROM_CONFIG_ALAH);
        thrMinute = EEPROM_ReadConfig(EEPROM_CONFIG_ALAM);
        thrSecond = EEPROM_ReadConfig(EEPROM_CONFIG_ALAS);
        thrTemp   = EEPROM_ReadConfig(EEPROM_CONFIG_ALAT);
        thrLum    = EEPROM_ReadConfig(EEPROM_CONFIG_ALAL);
        hh = EEPROM_ReadConfig(EEPROM_CONFIG_CLKH);
        mm = EEPROM_ReadConfig(EEPROM_CONFIG_CLKM);

        EEPROM_ReadRecord(EEPROM_ADDR_TMAX, &records[0].clk_hh, &records[0].clk_mm, &records[0].clk_ss, &records[0].temp, &records[0].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_TMIN, &records[1].clk_hh, &records[1].clk_mm, &records[1].clk_ss, &records[1].temp, &records[1].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_LMAX, &records[2].clk_hh, &records[2].clk_mm, &records[2].clk_ss, &records[2].temp, &records[2].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_LMIN, &records[3].clk_hh, &records[3].clk_mm, &records[3].clk_ss, &records[3].temp, &records[3].lum);
    }

    mode = UI_NORMAL;
    field = CF_CLK_HH;
    record_type = RECORD_NONE;

    LCDinit();
    RenderNormal();
}

void UI_OnTick1s(void)
{
    if (time_inactive >= tina) {mode = UI_NORMAL;}

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
    sprintf(line1,"%02u:%02u:%02u %02uC %ul", records[record_index].clk_hh, records[record_index].clk_mm, records[record_index].clk_ss, records[record_index].temp, records[record_index].lum);
    LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
    sprintf(line2,"%02u:%02u:%02u %02uC %ul", records[record_index+1].clk_hh, records[record_index+1].clk_mm, records[record_index+1].clk_ss, records[record_index+1].temp, records[record_index+1].lum);
    LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); 
}

void RenderConfig(void)
{


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

static uint8_t readLuminosityLevel(void)
{
    uint8_t adc_value = ADCC_GetSingleConversion(channel_AN0);
    if (adc_value < 64) return 0;
    else if (adc_value < 128) return 1;
    else if (adc_value < 192) return 2;
    else return 3;
}

void ReadSensors(){
    temp = readTC74();
    light = readLuminosityLevel();
    CompareReading();
}

/*--------------------    Records functions ------------------------*/
void CompareReading(void){

    int record_to_save = 0;
    // compare current reading with records and update if needed
    if (temp > records[0].temp) {
        records[0].temp = temp;
        records[0].clk_hh = hh;
        records[0].clk_mm = mm;
        records[0].clk_ss = ss;
        record_to_save = 1;
        SaveRecord_EEPROM(record_to_save);
    }
    if (temp < records[1].temp) {
        records[1].temp = temp;
        records[1].clk_hh = hh;
        records[1].clk_mm = mm;
        records[1].clk_ss = ss;
        record_to_save = 2;
        SaveRecord_EEPROM(record_to_save);
    }
    if (light > records[2].lum) {
        records[2].lum = light;
        records[2].clk_hh = hh;
        records[2].clk_mm = mm;
        records[2].clk_ss = ss;
        record_to_save = 3;
        SaveRecord_EEPROM(record_to_save);
    }
    if (light < records[3].lum) {
        records[3].lum = light;
        records[3].clk_hh = hh;
        records[3].clk_mm = mm;
        records[3].clk_ss = ss;
        record_to_save = 4;
        SaveRecord_EEPROM(record_to_save);
    }

}

void SaveRecord_EEPROM(int record_to_save)
{
    
    switch(record_to_save){
        case 1:
            EEPROM_WriteRecord(EEPROM_ADDR_TMAX, records[0].clk_hh, records[0].clk_mm, records[0].clk_ss, records[0].temp, records[0].lum);
            break;
        case 2:
            EEPROM_WriteRecord(EEPROM_ADDR_TMIN, records[1].clk_hh, records[1].clk_mm, records[1].clk_ss, records[1].temp, records[1].lum);
            break;
        case 3:
            EEPROM_WriteRecord(EEPROM_ADDR_LMAX, records[2].clk_hh, records[2].clk_mm, records[2].clk_ss, records[2].temp, records[2].lum);
            break;
        case 4:
            EEPROM_WriteRecord(EEPROM_ADDR_LMIN, records[3].clk_hh, records[3].clk_mm, records[3].clk_ss, records[3].temp, records[3].lum);
            break;
        default:
            break;
    }
}

void ClearRecords(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        records[i].clk_hh = 0;
        records[i].clk_mm = 0;
        records[i].clk_ss = 0;
        records[i].temp   = 0;
        records[i].lum    = 0;
    }
    EEPROM_WriteRecord(EEPROM_ADDR_TMAX, records[0].clk_hh, records[0].clk_mm, records[0].clk_ss, records[0].temp, records[0].lum);
    EEPROM_WriteRecord(EEPROM_ADDR_TMIN, records[1].clk_hh, records[1].clk_mm, records[1].clk_ss, records[1].temp, records[1].lum);
    EEPROM_WriteRecord(EEPROM_ADDR_LMAX, records[2].clk_hh, records[2].clk_mm, records[2].clk_ss, records[2].temp, records[2].lum);
    EEPROM_WriteRecord(EEPROM_ADDR_LMIN, records[3].clk_hh, records[3].clk_mm, records[3].clk_ss, records[3].temp, records[3].lum);
}

/*--------------------    Button Flags ----------------------------*/

void S1_Callback(void){ S1_pressed = 1; }
void S2_Callback(void){ S2_pressed = 1; }
int S1_check(void){return S1_pressed;}
int S2_check(void){return S2_pressed;}
void Reset_flag_S1(void){S1_pressed = 0;}
void Reset_flag_S2(void){S2_pressed = 0;}








