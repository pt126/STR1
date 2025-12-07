#include "ui.h"
#include "../mcc_generated_files/mcc.h"
#include "../LCD/lcd.h"
#include "../I2C/i2c.h"
#include <stdint.h>
#include "../mcc_generated_files/adcc.h"
#include "../mcc_generated_files/pwm6.h"

/* -------------------- Defaults -------------------- */
#define ALAF 1    /* alarms enabled at startup (A) */
#define ALAH 0
#define ALAM 0
#define ALAS 30
#define ALAT 20   /* temperature threshold */
#define ALAL 2    /* luminosity threshold (0..3) */
#define TALA 4    /* beep duration (seconds) */
#define TINA 10   /* inactivity timeout (seconds) */

/* -----------------  State Enums --------------------------- */
typedef enum { UI_NORMAL, UI_CONFIG, UI_SHOW_RECORDS } ui_mode_t;

typedef enum {
    CF_CLK_HH,
    CF_CLK_MM,
    CF_CLK_SS,
    CF_CTL_C,      // select clock alarm threshold group
    CF_CTL_T,      // temperature threshold
    CF_CTL_L,      // luminosity threshold
    CF_ALARM_EN,   // A/a toggle
    CF_RESET,      // R reset records

    // sub-edit states for alarm clock time
    CF_CTL_C_HH,
    CF_CTL_C_MM,
    CF_CTL_C_SS
} cfg_field_t;

typedef enum {
    RECORD_NONE,
    RECORD_TEMP,
    RECORD_LUM,
} record_type_t;

/* -------------------- Data and thresholds -------------------- */
static ui_mode_t     mode;
static cfg_field_t   field;
static record_type_t record_type;

static uint8_t hh, mm, ss;
static uint8_t temp;
static uint8_t light;

static uint8_t alarmsEnabled = ALAF;  // shows A/a in UI
static uint8_t alarmFlagC, alarmFlagT, alarmFlagL; // latched flags (still used internally)

/* records */
typedef struct {
    uint8_t clk_hh;
    uint8_t clk_mm;
    uint8_t clk_ss;
    uint8_t temp;
    uint8_t lum;
} record_field_t;

static record_field_t records[4];
static uint8_t record_index;

/* thresholds */
static uint8_t thrSecond = ALAS;
static uint8_t thrMinute = ALAM;
static uint8_t thrHour   = ALAH;
static uint8_t thrTemp   = ALAT;
static uint8_t thrLum    = ALAL;

static uint8_t time_inactive;

/* button flags */
static volatile uint8_t S1_pressed;
static volatile uint8_t S2_pressed;

/* alarm edge tracking */
static uint8_t prevTempCond, prevLumCond, prevClockCond;

/* PWM countdown */
static volatile uint8_t pwm_seconds_left;

/* 1-second alarm suppression after S1 */
static volatile uint8_t alarm_suppress_1s;

/* -------------------- PWM helpers -------------------- */
static inline void PWM6_Start(void) { PWM6CONbits.PWM6EN = 1; }

static inline void PWM6_Stop(void)
{
    PWM6_LoadDutyValue(0);
    PWM6CONbits.PWM6EN = 0;
}

static inline void Alarm_BeepStart(void)
{
    if (pwm_seconds_left != 0) return;
    pwm_seconds_left = TALA;
    PWM6_LoadDutyValue(512);
    PWM6_Start();
}

static inline void Alarm_BeepTick1s(void)
{
    if (pwm_seconds_left == 0) return;
    if (--pwm_seconds_left == 0) PWM6_Stop();
}

/* Force alarms OFF immediately, and keep them OFF for the next 1s tick */
static inline void Alarm_SuppressBegin_1s(void)
{
    alarm_suppress_1s = 1;

    // LCD alarm letters/flags off
    alarmFlagC = alarmFlagT = alarmFlagL = 0;

    // buzzer off
    pwm_seconds_left = 0;
    PWM6_Stop();

    // alarm LEDs off
    IO_RA5_SetLow();
    IO_RA4_SetLow();

    // allow retrigger after the mute window if condition persists
    prevTempCond  = 0;
    prevLumCond   = 0;
    prevClockCond = 0;
}

/* -------------------- Utility: fixed 16-char lines -------------------- */
static void fill16(char s[17])
{
    for (uint8_t i = 0; i < 16; i++) s[i] = ' ';
    s[16] = '\0';
}

static void put2(char *p, uint8_t v)
{
    p[0] = (char)('0' + (v / 10));
    p[1] = (char)('0' + (v % 10));
}

/* -------------------- Public: Clock tick -------------------- */
void Clock_Tick1s(void)
{
    if (++ss >= 60) {
        ss = 0;
        if (++mm >= 60) {
            mm = 0;
            if (++hh >= 24) hh = 0;
        }
    }
}

/* -------------------- Public: Clear flags -------------------- */
void ClearAlarmFlags(void)
{
    alarmFlagC = 0;
    alarmFlagT = 0;
    alarmFlagL = 0;
}

/* -------------------- Config field stepping -------------------- */
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
        case CF_RESET:      return CF_CLK_HH;

        case CF_CTL_C_HH:   return CF_CTL_C_MM;
        case CF_CTL_C_MM:   return CF_CTL_C_SS;
        case CF_CTL_C_SS:   return CF_CTL_T;
        default:            return CF_CLK_HH;
    }
}

/* -------------------- Button handling -------------------- */
void OnS1Pressed(void)
{
    // REQUIRED: pressing S1 shuts down alarms (letters + LEDs + buzzer) for at least 1 second
    Alarm_SuppressBegin_1s();

    time_inactive = 0;

    if (mode == UI_NORMAL) {
        mode  = UI_CONFIG;
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

    if (mode == UI_SHOW_RECORDS) {
        if (record_type == RECORD_TEMP) {
            mode = UI_NORMAL;
            record_type = RECORD_NONE;
            record_index = 0;
        } else if (record_type == RECORD_LUM) {
            record_type = RECORD_TEMP;
            record_index = 0;
        }
        return;
    }
}

void OnS2Pressed(void)
{
    time_inactive = 0;

    if (mode == UI_CONFIG) {
        switch (field) {
            case CF_CLK_HH:      hh = (hh + 1) % 24; break;
            case CF_CLK_MM:      mm = (mm + 1) % 60; break;
            case CF_CLK_SS:      ss = (ss + 1) % 60; break;

            case CF_CTL_C:       field = CF_CTL_C_HH; break;
            case CF_CTL_C_HH:    thrHour   = (thrHour + 1)   % 24; break;
            case CF_CTL_C_MM:    thrMinute = (thrMinute + 1) % 60; break;
            case CF_CTL_C_SS:    thrSecond = (thrSecond + 1) % 60; break;

            case CF_CTL_T:       thrTemp = (thrTemp + 1) % 51; break;
            case CF_CTL_L:       thrLum  = (thrLum  + 1) % 4;  break;

            case CF_ALARM_EN:    alarmsEnabled ^= 1; break;

            case CF_RESET:
                records[0].temp = 0;   records[0].lum = 0;
                records[1].temp = 255; records[1].lum = 3;
                records[2].temp = 0;   records[2].lum = 0;
                records[3].temp = 255; records[3].lum = 3;
                break;

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

    if (mode == UI_SHOW_RECORDS) {
        if (record_type == RECORD_TEMP) {
            record_type = RECORD_LUM;
            record_index = 2;
        } else {
            mode = UI_NORMAL;
            record_type = RECORD_NONE;
            record_index = 0;
        }
        return;
    }
}

/* -------------------- Rendering -------------------- */
void RenderRecords(void)
{
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    record_field_t *r1 = &records[record_index];
    record_field_t *r2 = &records[record_index + 1];

    put2(&line1[0], r1->clk_hh); line1[2] = ':';
    put2(&line1[3], r1->clk_mm); line1[5] = ':';
    put2(&line1[6], r1->clk_ss); line1[8] = ' ';
    put2(&line1[9], r1->temp);   line1[11] = 'C';
    line1[13] = 'L'; line1[15] = (char)('0' + (r1->lum % 10));

    put2(&line2[0], r2->clk_hh); line2[2] = ':';
    put2(&line2[3], r2->clk_mm); line2[5] = ':';
    put2(&line2[6], r2->clk_ss); line2[8] = ' ';
    put2(&line2[9], r2->temp);   line2[11] = 'C';
    line2[13] = 'L'; line2[15] = (char)('0' + (r2->lum % 10));

    LCDcmd(0x80); LCDpos(0,0); LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0); LCDstr(line2); while (LCDbusy());
}

void RenderConfig(void)
{
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    uint8_t show_hh = hh, show_mm = mm, show_ss = ss;

    if (field == CF_CTL_C || field == CF_CTL_C_HH || field == CF_CTL_C_MM || field == CF_CTL_C_SS) {
        show_hh = thrHour;
        show_mm = thrMinute;
        show_ss = thrSecond;
    }

    put2(&line1[0], show_hh); line1[2] = ':';
    put2(&line1[3], show_mm); line1[5] = ':';
    put2(&line1[6], show_ss);

    line1[11] = 'C';
    line1[12] = 'T';
    line1[13] = 'L';
    line1[14] = alarmsEnabled ? 'A' : 'a';
    line1[15] = 'R';

    put2(&line2[0], thrTemp);
    line2[2]  = ' ';
    line2[3]  = 'C';
    line2[13] = 'L';
    line2[15] = (char)('0' + (thrLum % 10));

    LCDcmd(0x80); LCDpos(0,0); LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0); LCDstr(line2); while (LCDbusy());

    LCDcmd(0x80);
    switch (field) {
        case CF_CLK_HH:    LCDpos(0,1);  break;
        case CF_CLK_MM:    LCDpos(0,4);  break;
        case CF_CLK_SS:    LCDpos(0,7);  break;

        case CF_CTL_C:     LCDpos(0,11); break;
        case CF_CTL_C_HH:  LCDpos(0,1);  break;
        case CF_CTL_C_MM:  LCDpos(0,4);  break;
        case CF_CTL_C_SS:  LCDpos(0,7);  break;

        case CF_CTL_T:     LCDpos(0,12); break;
        case CF_CTL_L:     LCDpos(0,13); break;
        case CF_ALARM_EN:  LCDpos(0,14); break;
        case CF_RESET:     LCDpos(0,15); break;
        default: break;
    }
}

void RenderNormal(void)
{
    char line1[17], line2[17];
    fill16(line1);
    fill16(line2);

    put2(&line1[0], hh); line1[2] = ':';
    put2(&line1[3], mm); line1[5] = ':';
    put2(&line1[6], ss); line1[8] = ' ';

    // FIX #1: show "CTL" when alarms are enabled, per project description
    // FIX #2: if S1 mute window is active, letters must go OFF for that second
    if (alarm_suppress_1s) {
        line1[9]  = ' ';
        line1[10] = ' ';
        line1[11] = ' ';
    } else if (alarmsEnabled) {
        line1[9]  = 'C';
        line1[10] = 'T';
        line1[11] = 'L';
    } else {
        line1[9]  = ' ';
        line1[10] = ' ';
        line1[11] = ' ';
    }

    line1[14] = alarmsEnabled ? 'A' : 'a';

    put2(&line2[0], temp);
    line2[2]  = ' ';
    line2[3]  = 'C';
    line2[13] = 'L';
    line2[15] = (char)('0' + (light % 10));

    LCDcmd(0x80); LCDpos(0,0); LCDstr(line1); while (LCDbusy());
    LCDcmd(0xC0); LCDpos(8,0); LCDstr(line2); while (LCDbusy());
}

/* -------------------- UI main -------------------- */
void UI_Init(void)
{
    alarmsEnabled = ALAF;

    hh = 0; mm = 0; ss = 0;

    thrHour = ALAH; thrMinute = ALAM; thrSecond = ALAS;
    thrTemp = ALAT;
    thrLum  = ALAL;

    mode = UI_NORMAL;
    field = CF_CLK_HH;
    record_type = RECORD_NONE;
    record_index = 0;

    time_inactive = 0;

    records[0].temp = 0;   records[0].lum = 0;
    records[1].temp = 255; records[1].lum = 3;
    records[2].temp = 0;   records[2].lum = 0;
    records[3].temp = 255; records[3].lum = 3;

    ClearAlarmFlags();

    prevTempCond = 0;
    prevLumCond  = 0;
    prevClockCond = 0;

    pwm_seconds_left = 0;
    PWM6_Stop();

    alarm_suppress_1s = 0;

    // FIX: ensure indicator LEDs start OFF if alarms are disabled
    IO_RA5_SetLow();
    IO_RA4_SetLow();

    LCDinit();
    RenderNormal();
}

void UI_OnTick1s(void)
{
    // If S1 mute is active, keep everything off for this whole second, then release.
    if (alarm_suppress_1s) {
        // keep outputs OFF
        IO_RA5_SetLow();
        IO_RA4_SetLow();
        pwm_seconds_left = 0;
        PWM6_Stop();

        // prevent immediate “stuck edge” behaviour after mute
        prevTempCond = 0;
        prevLumCond  = 0;
        prevClockCond = 0;

        // consume the 1-second mute
        alarm_suppress_1s = 0;
    } else {
        Alarm_BeepTick1s();

        // time alarm check
        {
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
        }
    }

    time_inactive++;
    if (time_inactive >= TINA) mode = UI_NORMAL;

    if      (mode == UI_NORMAL)       RenderNormal();
    else if (mode == UI_CONFIG)       RenderConfig();
    else                              RenderRecords();
}

/* -------------------- Sensors ------------------------ */
unsigned char readTC74(void)
{
    unsigned char value;

    do {
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
    adc_result_t raw = ADCC_GetSingleConversion((adcc_channel_t)0x00); // ANA0 (RA0)
    uint8_t level = (uint8_t)(raw >> 8);
    if (level > 3u) level = 3u;
    return level;
}

void ReadSensors(void)
{
    temp  = (uint8_t)readTC74();
    light = readLuminosityLevel();
}

/* Call after ReadSensors() each PMON cycle */
void EvaluateTempLumAlarms(void)
{
    // S1 mute window: force OFF
    if (alarm_suppress_1s) {
        IO_RA5_SetLow();
        IO_RA4_SetLow();
        prevTempCond = 0;
        prevLumCond  = 0;
        return;
    }

    // FIX: when alarms are DISABLED (small 'a'), these LEDs must be OFF
    if (!alarmsEnabled) {
        IO_RA5_SetLow();
        IO_RA4_SetLow();

        // keep edge tracking sane
        prevTempCond = 0;
        prevLumCond  = 0;
        return;
    }

    uint8_t tempCond = (temp > thrTemp);
    uint8_t lumCond  = (light < thrLum);

    // indicator LEDs only when alarms are enabled
    if (tempCond) IO_RA5_SetHigh(); else IO_RA5_SetLow();
    if (lumCond)  IO_RA4_SetHigh(); else IO_RA4_SetLow();

    // rising edge triggers (buzzer + latch flags)
    if (tempCond && !prevTempCond) { alarmFlagT = 1; Alarm_BeepStart(); }
    if (lumCond  && !prevLumCond)  { alarmFlagL = 1; Alarm_BeepStart(); }

    prevTempCond = tempCond;
    prevLumCond  = lumCond;
}

/* -------------------- Records ------------------------ */
void CompareReading(void)
{
    if (temp > records[0].temp) {
        records[0].temp = temp;
        records[0].lum  = light;
        records[0].clk_hh = hh; records[0].clk_mm = mm; records[0].clk_ss = ss;
    }
    if (temp < records[1].temp) {
        records[1].temp = temp;
        records[1].lum  = light;
        records[1].clk_hh = hh; records[1].clk_mm = mm; records[1].clk_ss = ss;
    }
    if (light > records[2].lum) {
        records[2].lum  = light;
        records[2].temp = temp;
        records[2].clk_hh = hh; records[2].clk_mm = mm; records[2].clk_ss = ss;
    }
    if (light < records[3].lum) {
        records[3].lum  = light;
        records[3].temp = temp;
        records[3].clk_hh = hh; records[3].clk_mm = mm; records[3].clk_ss = ss;
    }
}

void SaveRecord(void)
{
    /* not implemented */
}

/* -------------------- Button flags API ------------------------ */
void S1_Callback(void){ S1_pressed = 1; }
void S2_Callback(void){ S2_pressed = 1; }
int  S1_check(void){ return S1_pressed; }
int  S2_check(void){ return S2_pressed; }
void Reset_flag_S1(void){ S1_pressed = 0; }
void Reset_flag_S2(void){ S2_pressed = 0; }
