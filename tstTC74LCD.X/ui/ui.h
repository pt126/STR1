#pragma once
#include <stdint.h>
#ifndef UI_H
#define UI_H

void UI_Init(void);
void UI_OnTick1s(void);


/*---- funcoes para os butoes -------*/
void S1_Callback(void);
void S2_Callback(void);

int S1_check(void);
int S2_check(void);

void Reset_flag_S1(void);
void Reset_flag_S2(void);

#endif
