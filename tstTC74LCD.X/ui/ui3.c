#include "ui.h"
#include "../mcc_generated_files/mcc.h"
#include "../LCD/lcd.h"
#include "../I2C/i2c.h"
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

// thresholds shown in config mode (spec says tt and l show threshold values there)
uint8_t tala = TALA;  // alarm signal duration
uint8_t tina = TINA;  // inactivity time
uint8_t thrSecond = ALAS;
uint8_t thrMinute = ALAM;
uint8_t thrHour   = ALAH;
char thrTemp   = ALAT;   // 0 -50
char thrLum    = ALAL;    // 0 - 3


uint8_t time_inactive;

/*----------- Flags for the button interrupts --------------*/
static volatile int S1_pressed ;
static volatile int S2_pressed ;

/* alarm edge tracking */
static uint8_t prevTempCond, prevLumCond, prevClockCond;

/* PWM countdown */
static volatile uint8_t pwm_seconds_left;

/* -------------------- PWM helpers -------------------- */
static inline void PWM6_Start(void)
{
    PWM6CONbits.PWM6EN = 1;
}

static inline void PWM6_Stop(void)
{
    PWM6_LoadDutyValue(0);
    PWM6CONbits.PWM6EN = 0;
}

static inline void Alarm_BeepStart(void)
{
    // only start if not already beeping
    if (pwm_seconds_left != 0) return;

    pwm_seconds_left = TALA;
    PWM6_LoadDutyValue(512); //50 percent duty cycle
    PWM6_Start();
}

static inline void Alarm_BeepTick1s(void)
{
    if (pwm_seconds_left == 0) return;
    if (--pwm_seconds_left == 0) PWM6_Stop();
}

/*-----------------------     Utils functions...    ---------------------*/

/* -------------------- Utility: build fixed 16-char lines -------------------- */
static void fill16(char s[17])
{
    for (uint8_t i = 0; i < 16; i++) s[i] = ' ';
    s[16] = '\0';
}

/*put two digits in a string -> for printing values*/
static void put2(char *p, uint8_t v)
{
    //p[0] = (char)('0' + (v / 10));
    uint8_t rightdigit = v%10;
    uint8_t leftdigit  = (v%100 - rightdigit)/10;
    p[0] = (char)('0' + leftdigit);
    p[1] = (char)('0' + rightdigit);
}

/*Clock Tick-> public!*/
void Clock_Tick1s(void)
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

/*------- Config stepping ----------*/
static cfg_field_t cfg_next(cfg_field_t f)
{
    switch (f) {
        case CF_CLK_HH:     return CF_CLK_MM;
        case CF_CLK_MM:     return CF_CLK_SS;
        case CF_CLK_SS:     return CF_CTL_C;

        case CF_CTL_C:      return CF_CTL_T;
        case CF_CTL_T:      return CF_CTL_L;
        case CF_CTL_L:      return CF_ALARM_EN;
        case CF_ALARM_EN:   return CF_RESET;
        case CF_RESET:      return CF_CLK_HH;   // wrap, then exit handled by UI logic

        case CF_CTL_C_HH:   return CF_CTL_C_MM;
        case CF_CTL_C_MM:   return CF_CTL_C_SS;
        case CF_CTL_C_SS:   return CF_CTL_T;    // after editing alarm time -> continue
        default:            return CF_CLK_HH;
    }
} 

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
        cfg_field_t next = cfg_next(field);
        if (field == CF_RESET) {
            mode  = UI_NORMAL;
            field = CF_CLK_HH;
            return;
        }

        field = next;
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
            record_type = RECORD_TEMP;
            record_index = 0;
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
                //char line[17];
                //sprintf(line,"hh %d",hh);
                //LCDcmd(0x80); LCDpos(0,0);LCDstr(line); while (LCDbusy()){}; __delay_ms(5000);
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
                //sprintf(line,"thrHour %d",thrHour);
                //LCDcmd(0x80); LCDpos(0,0);LCDstr(line); while (LCDbusy()){}; __delay_ms(5000);
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
                EEPROM_WriteConfig(EEPROM_CONFIG_ALAL, (uint8_t)thrLum);
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
            record_type = RECORD_LUM;
            record_index += 2;
            return;
        }
        
    }

    return;
}

/*-------------------    UI functions ----------------------------*/
void set_defaults(void){
    pmon = (uint8_t) PMON;
    tala = (uint8_t) TALA;
    tina = (uint8_t) TINA;
    alarmsEnabled = (uint8_t) ALAF;
    thrHour   = (uint8_t) ALAH;
    thrMinute = (uint8_t) ALAM;
    thrSecond = (uint8_t) ALAS;
    thrTemp   = (char) ALAT;
    thrLum    = (char) ALAL;
    hh = (uint8_t) CLKH;
    mm = (uint8_t) CLKM;
    ss = (uint8_t) 0;

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

    // init records to sane extremes
    records[0].temp = (uint8_t)0;   records[0].lum = (uint8_t)0;
    records[1].temp = (uint8_t)255; records[1].lum = (uint8_t)3;
    records[2].temp = (uint8_t)0;   records[2].lum = (uint8_t)0;
    records[3].temp = (uint8_t)255; records[3].lum = (uint8_t)3;
}

void UI_Init(void)
{

    uint16_t magic;
    uint8_t stored_checksum, calc_checksum = 0;

    

    EEPROM_ReadHeader(&magic, &stored_checksum);
    
    if (magic != MAGIC_WORD) {
        // EEPROM not initialized, set defaults and write magic word
        set_defaults();
        
        // Calculate and write checksum
        calc_checksum = 0;
        for (uint8_t i = 0; i <= EEPROM_CONFIG_CLKM; i++) {
            calc_checksum += EEPROM_ReadConfig(i);
        }
        
        for (uint16_t addr = EEPROM_ADDR_TMAX; addr <= EEPROM_ADDR_LMIN + 4; addr++) {
            calc_checksum += DATAEE_ReadByte(addr);
        }

        EEPROM_WriteHeader(MAGIC_WORD, calc_checksum);

    } else {
        // EEPROM is initialized, check checksum
        calc_checksum = 0;
        for (uint8_t i = 0; i <= EEPROM_CONFIG_CLKM; i++) {
            calc_checksum += EEPROM_ReadConfig(i);
        }
        
        for (uint16_t addr = EEPROM_ADDR_TMAX; addr <= EEPROM_ADDR_LMIN + 4; addr++) {
            calc_checksum += DATAEE_ReadByte(addr);
        }

        if (stored_checksum != calc_checksum) {

            set_defaults();
            UpdateEEPROMChecksum();
            //return;
        }
        // Load config and records from EEPROM
        pmon = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_PMON);
        tala = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_TALA);
        tina = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_TINA);
        alarmsEnabled = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_ALAF);
        thrHour   = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_ALAH);
        thrMinute = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_ALAM);
        thrSecond = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_ALAS);
        thrTemp   = (char) EEPROM_ReadConfig(EEPROM_CONFIG_ALAT);
        thrLum    = (char) EEPROM_ReadConfig(EEPROM_CONFIG_ALAL);
        hh = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_CLKH);
        mm = (uint8_t) EEPROM_ReadConfig(EEPROM_CONFIG_CLKM);
        ss = (uint8_t) 0;


        EEPROM_ReadRecord(EEPROM_ADDR_TMAX, &records[0].clk_hh, &records[0].clk_mm, &records[0].clk_ss, &records[0].temp, &records[0].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_TMIN, &records[1].clk_hh, &records[1].clk_mm, &records[1].clk_ss, &records[1].temp, &records[1].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_LMAX, &records[2].clk_hh, &records[2].clk_mm, &records[2].clk_ss, &records[2].temp, &records[2].lum);
        EEPROM_ReadRecord(EEPROM_ADDR_LMIN, &records[3].clk_hh, &records[3].clk_mm, &records[3].clk_ss, &records[3].temp, &records[3].lum);
    }

    ClearAlarmFlags();

    prevTempCond = 0;
    prevLumCond  = 0;
    prevClockCond = 0;

    pwm_seconds_left = 0;
    PWM6_Stop();

    time_inactive = 0;
    mode = UI_NORMAL;
    field = CF_CLK_HH;
    record_type = RECORD_NONE;

    LCDinit();
    RenderNormal();
}


void UI_OnTick1s(void)
{
    
    
    if (time_inactive > tina) {time_inactive = 0;mode = UI_NORMAL;}
    
    Alarm_BeepTick1s();
    uint8_t clockCond = (hh == thrHour) && (mm == thrMinute) && (ss == thrSecond);

    if (!alarmsEnabled) {
        prevClockCond = clockCond;
    } else {
        if (clockCond && !prevClockCond) {
            alarmFlagC = 1;
            Alarm_BeepStart();
        }
        prevClockCond = clockCond;
    }
    
    // Render the UI according to the mode
    if      (mode == UI_NORMAL)      { RenderNormal (); } //here its ok to be inactive
    else if (mode == UI_CONFIG)      { time_inactive ++; RenderConfig (); }
    else if (mode == UI_SHOW_RECORDS){ time_inactive ++; RenderRecords(); }
    return;

    // 4) LEDs for normal mode (later add other LEDs + PWM alarm)
    // D5/RA7 clock activity: toggle every 1 second :contentReference[oaicite:21]{index=21}
   //LATAbits.LATA7 ^= 1;
    //IO_RA7_Toggle();
}

/*-------------------  Rendering  ---------------------------------*/

void RenderRecords(void){
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    put2(&line1[0], records[record_index].clk_hh); line1[2] = ':';
    put2(&line1[3], records[record_index].clk_mm); line1[5] = ':';
    put2(&line1[6], records[record_index].clk_ss); line1[8] = ' ';
    put2(&line1[9], records[record_index].temp);   line1[11]= 'C';
    line1[15] = (char)('0' + records[record_index].lum);   line1[14]= 'L';

    put2(&line2[0], records[record_index+1].clk_hh); line2[2] = ':';
    put2(&line2[3], records[record_index+1].clk_mm); line2[5] = ':';
    put2(&line2[6], records[record_index+1].clk_ss); line2[8] = ' ';
    put2(&line2[9], records[record_index+1].temp);   line2[11]= 'C';
    line2[15] = (char)('0' + records[record_index+1].lum);   line2[14]= 'L';



    //sprintf(line2,"%02u:%02u:%02u %02uC %ul", records[record_index+1].clk_hh, records[record_index+1].clk_mm, records[record_index+1].clk_ss, records[record_index+1].temp, records[record_index+1].lum);
    LCDcmd(0x80); LCDpos(0,0);LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0);LCDstr(line2); while (LCDbusy()); 
}

void RenderConfig(void)
{
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    // --- line1 shows either current time OR alarm time when editing C ---
    uint8_t show_hh = hh, show_mm = mm, show_ss = ss;

    if (field == CF_CTL_C || field == CF_CTL_C_HH || field == CF_CTL_C_MM || field == CF_CTL_C_SS) {
        show_hh = thrHour;
        show_mm = thrMinute;
        show_ss = thrSecond;
    }

    put2(&line1[0], show_hh); line1[2] = ':';
    put2(&line1[3], show_mm); line1[5] = ':';
    put2(&line1[6], show_ss);

    // positions 11..15 spell "CTLAR" with A/a
    line1[11] = 'C';
    line1[12] = 'T';
    line1[13] = 'L';
    line1[14] = alarmsEnabled ? 'A' : 'a';
    line1[15] = 'R';

    // --- line2 ALWAYS shows thresholds ---
    put2(&line2[0], thrTemp);
    line2[2] = ' ';
    line2[3] = 'C';
    line2[13] = 'L';
    //line2[15] = (char)('0' + (thrLum % 10));
    line2[15] = (char)('0' + thrLum );

    LCDcmd(0x80); LCDpos(0,0); LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0); LCDstr(line2); while (LCDbusy());

    // Cursor positioning
    LCDcmd(0x80);
    switch (field) {
        case CF_CLK_HH:    LCDpos(0,1);  break;
        case CF_CLK_MM:    LCDpos(0,4);  break;
        case CF_CLK_SS:    LCDpos(0,7);  break;

        case CF_CTL_C:     LCDpos(0,11); break;
        case CF_CTL_C_HH:  LCDpos(0,1);  break;
        case CF_CTL_C_MM:  LCDpos(0,4);  break;
        case CF_CTL_C_SS:  LCDpos(0,7);  break;

        case CF_CTL_T:     LCDpos(0,12); break; // points at 'T'
        case CF_CTL_L:     LCDpos(0,13); break; // points at 'L'
        case CF_ALARM_EN:  LCDpos(0,14); break; // points at 'A/a'
        case CF_RESET:     LCDpos(0,15); break; // points at 'R'
        default: break;
    }
}

void RenderNormal(void)
{
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    // line1: "hh:mm:ss CTL  A/a"
    put2(&line1[0], hh); line1[2] = ':';
    put2(&line1[3], mm); line1[5] = ':';
    put2(&line1[6], ss); line1[8] = ' ';

    // show all flags at once
    line1[9]  = alarmFlagC ? 'C' : ' ';
    line1[10] = alarmFlagT ? 'T' : ' ';
    line1[11] = alarmFlagL ? 'L' : ' ';

    line1[14] = alarmsEnabled ? 'A' : 'a';

    // line2: "tt C         L x"
    put2(&line2[0], temp);
    line2[2] = ' ';
    line2[3] = 'C';
    line2[13] = 'L';
    line2[15] = (char)('0' + (light % 10));

    LCDcmd(0x80); LCDpos(0,0); LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0); LCDstr(line2); while (LCDbusy());
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
    adc_result_t raw = ADCC_GetSingleConversion((adcc_channel_t)0x00); // 0x00 = ANA0 (RA0)
    //adc_result_t raw = ADCC_GetSingleConversion(channel_ANA0); // RA0/AN0
    

    uint8_t level = (uint8_t)(raw >> 8); // 0..3 for 10-bit ADC
    if (level > 3u) level = 3u;
    return level;
}

void ReadSensors(){
    temp = readTC74();
    light = readLuminosityLevel();
    CompareReading();
}

/* Call after ReadSensors() each PMON cycle */
void EvaluateTempLumAlarms(void)
{
    uint8_t tempCond = (temp > thrTemp);
    uint8_t lumCond  = (light < thrLum);

    // indicator LEDs
    if (tempCond) IO_RA5_SetHigh(); else IO_RA5_SetLow();
    if (lumCond)  IO_RA4_SetHigh(); else IO_RA4_SetLow();

    if (!alarmsEnabled) {
        prevTempCond = tempCond;
        prevLumCond  = lumCond;
        return;
    }

    // rising edge triggers
    if (tempCond && !prevTempCond) { alarmFlagT = 1; Alarm_BeepStart(); }
    if (lumCond  && !prevLumCond)  { alarmFlagL = 1; Alarm_BeepStart(); }

    prevTempCond = tempCond;
    prevLumCond  = lumCond;
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
    // Reset timestamps
    for (uint8_t i = 0; i < 4; i++) {
        records[i].clk_hh = 0;
        records[i].clk_mm = 0;
        records[i].clk_ss = 0;
    }

    // Set "extreme" initial values so min/max comparisons work:
    // [0] Tmax, [1] Tmin, [2] Lmax, [3] Lmin
    records[0].temp = 0;    records[0].lum = 0;   // Tmax
    records[1].temp = 255;  records[1].lum = 0;   // Tmin (temp starts high)
    records[2].temp = 0;    records[2].lum = 0;   // Lmax
    records[3].temp = 0;    records[3].lum = 3;   // Lmin (lum starts high)

    // Persist to EEPROM
    EEPROM_WriteRecord(EEPROM_ADDR_TMAX,
                       records[0].clk_hh, records[0].clk_mm, records[0].clk_ss,
                       records[0].temp, records[0].lum);

    EEPROM_WriteRecord(EEPROM_ADDR_TMIN,
                       records[1].clk_hh, records[1].clk_mm, records[1].clk_ss,
                       records[1].temp, records[1].lum);

    EEPROM_WriteRecord(EEPROM_ADDR_LMAX,
                       records[2].clk_hh, records[2].clk_mm, records[2].clk_ss,
                       records[2].temp, records[2].lum);

    EEPROM_WriteRecord(EEPROM_ADDR_LMIN,
                       records[3].clk_hh, records[3].clk_mm, records[3].clk_ss,
                       records[3].temp, records[3].lum);

    // If you keep checksum logic, refresh it here
    UpdateEEPROMChecksum();
}


/*--------------------    Button Flags ----------------------------*/

void S1_Callback(void){ S1_pressed = 1; }
void S2_Callback(void){ S2_pressed = 1; }
int S1_check(void){return S1_pressed;}
int S2_check(void){return S2_pressed;}
void Reset_flag_S1(void){S1_pressed = 0;}
void Reset_flag_S2(void){S2_pressed = 0;}








