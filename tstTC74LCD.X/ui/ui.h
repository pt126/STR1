#pragma once
#include <stdint.h>
#ifndef UI_H
#define UI_H
#include "../EEPROM/EEPROM.h"



uint8_t pmon = PMON;  // monitoring period


/*-----------------------     Utils functions...    ---------------------*/

void Clock_Tick1s(void);
void ClearAlarmFlags(void);

/*-------------------- Button resolving functions ----------------------*/
void OnS1Pressed(void);
void OnS2Pressed(void);

/*-------------------    UI functions ----------------------------*/

void UI_Init(void);
void UI_OnTick1s(void);
void RenderRecords(void);
void RenderConfig(void);
void RenderNormal(void);

/*--------------------    Sensor functions ------------------------*/
unsigned char readTC74 (void);
void ReadSensors(void);

/*--------------------    Records functions ------------------------*/
void CompareReading(void);
void SaveRecord_EEPROM(int record_to_save);
void ClearRecords(void);
void UpdateEEPROMChecksum(void);

/*--------------------    Button Flags ----------------------------*/

void S1_Callback(void);
void S2_Callback(void);
int S1_check(void);
int S2_check(void);
void Reset_flag_S1(void);
void Reset_flag_S2(void);


#endif
