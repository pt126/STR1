#include "ui.h"
#include "../mcc_generated_files/mcc.h"
#include "../LCD/lcd.h"
#include <stdio.h>

typedef enum { UI_NORMAL, UI_CONFIG, UI_SHOW_RECORDS } ui_mode_t;

// order matches the spec?s ?S1 cycles through until normal again?
typedef enum {
    CF_CLK_HH,
    CF_CLK_MM,
    CF_CLK_SS,
    CF_CTL_C,      // select which alarm threshold to edit (clock)
    CF_CTL_T,      // temp threshold
    CF_CTL_L,      // lum threshold
    CF_ALARM_EN,   // A/a toggle
    CF_RESET,      // R reset max/min
    CF_DONE
} cfg_field_t;

static ui_mode_t   mode;
static cfg_field_t field;

static uint8_t hh, mm, ss;
static uint8_t alarmsEnabled;       // 0/1  -> shows 'a' or 'A'
static uint8_t alarmFlagC, alarmFlagT, alarmFlagL; // shows letters CTL until S1 clears

// thresholds shown in config mode (spec says tt and l show threshold values there)
static uint8_t thrTemp;   // 0..50
static uint8_t thrLum;    // 0..3

// simple ?previous sampled state? for polling buttons once per second
static uint8_t s1_prev = 1, s2_prev = 1;

/*----------- Flags for the button interrupts --------------*/
extern volatile bool S1_pressed      = 0;
extern volatile bool S2_pressed      = 0;

static void Clock_Tick1s(void)
{
    if (++ss >= 60) { ss = 0; 
    if (++mm >= 60) { mm = 0; 
    if (++hh >= 24) hh = 0; } }
}

static void ClearAlarmFlags(void) { alarmFlagC = alarmFlagT = alarmFlagL = 0; }

static void RenderNormal(void)
{
    char line1[17], line2[17];

    // Line1: hh:mm:ss CTL AR   (AR = alarm enabled letter + maybe 'R' reserved for config; in normal just A/a)
    // show CTL letters only if flagged :contentReference[oaicite:10]{index=10}
    snprintf(line1, sizeof(line1), "%02u:%02u:%02u %c%c%c  %c ",
             hh, mm, ss,
             alarmFlagC ? 'C' : ' ',
             alarmFlagT ? 'T' : ' ',
             alarmFlagL ? 'L' : ' ',
             alarmsEnabled ? 'A' : 'a');

    // For now: dummy temp/lum until you hook sensors fully
    // Format: "tt C   L  l" :contentReference[oaicite:11]{index=11}
    //snprintf(line2, sizeof(line2), "   C   L   ");

    LCDcmd(0x80); LCDstr(line1);
    //LCDcmd(0xC0); LCDstr(line2);
}

static void RenderConfig(void)
{
    // In config mode, tt and l show threshold values, and you show an 'R' option for reset :contentReference[oaicite:12]{index=12}
    // Keep it simple: still print the same template, but values change based on selected field.
    char line1[17], line2[17];

char mC = (field == CF_CTL_C) ? '^' : ' ';
char mT = (field == CF_CTL_T) ? '^' : ' ';
char mL = (field == CF_CTL_L) ? '^' : ' ';

// then print markers on a second row/position if you want

    snprintf(line1, sizeof(line1), "%02u:%02u:%02u %c%c%c %cR",
             hh, mm, ss, mC, mT, mL, alarmsEnabled ? 'A' : 'a');

    snprintf(line2, sizeof(line2), "%02u C   L  %u",
             thrTemp, thrLum);

    LCDcmd(0x80); LCDstr(line1);
    LCDcmd(0xC0); LCDstr(line2);

    // Cursor positioning is LCD-specific; if your lcd.c supports it, move cursor here based on `field`.
    // (Optional for now)
}

static void OnS1Pressed(void)
{
    // S1 clears CTL *and* is used to enter config and move between fields :contentReference[oaicite:13]{index=13}
    ClearAlarmFlags();

    if (mode == UI_NORMAL) {
        mode = UI_CONFIG;
        field = CF_CLK_HH;
        return;
    }

    if (mode == UI_CONFIG) {
        field = (cfg_field_t)(field + 1);
        if (field >= CF_DONE) {
            mode = UI_NORMAL;
            field = CF_CLK_HH;
        }
        return;
    }
}

static void OnS2Pressed(void)
{
    if (mode == UI_CONFIG) {
        switch (field) {
            case CF_CLK_HH: hh = (hh + 1) % 24; break;                 // :contentReference[oaicite:14]{index=14}
            case CF_CLK_MM: mm = (mm + 1) % 60; break;
            case CF_CLK_SS: ss = (ss + 1) % 60; break;
            case CF_CTL_C:  /* select editing of clock-alarm threshold later */ break;
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
        return;
    }
}

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
    // 1) update time
    Clock_Tick1s();

    // 2) poll buttons once per second (simple + OK to start; later you can add IOC)
    // assume active-low switches with pull-ups (common); adjust if needed
    uint8_t s1 = PORTBbits.RB4;
    uint8_t s2 = PORTCbits.RC5;

    if (s1_prev == 0) OnS1Pressed();
    if (s2_prev == 0) OnS2Pressed();
    
    s1_prev = s1;
    s2_prev = s2;

    // 3) render
    if (mode == UI_NORMAL) RenderNormal();
    else if (mode == UI_CONFIG) RenderConfig();

    // 4) LEDs for normal mode (later add other LEDs + PWM alarm)
    // D5/RA7 clock activity: toggle every 1 second :contentReference[oaicite:21]{index=21}
   //LATAbits.LATA7 ^= 1;
    //IO_RA7_Toggle();
}

/*-- button flag setting--*/
void S1_Callback(void){ S1_pressed = 1; }
void S2_Callback(void){ S2_pressed = 1; }
int S1_check(void){return S1_pressed;}
int S2_check(void){return S2_pressed;}
void Reset_flag_S1(void){S1_pressed = 0;}
void Reset_flag_S2(void){S2_pressed = 0;}
