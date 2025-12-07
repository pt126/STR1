# 1 "EEPROM/EEPROM.c"
# 1 "<built-in>" 1
# 1 "<built-in>" 3
# 295 "<built-in>" 3
# 1 "<command line>" 1
# 1 "<built-in>" 2
# 1 "/home/barbichas/.mchp_packs/Microchip/PIC16F1xxxx_DFP/1.7.146/xc8/pic/include/language_support.h" 1 3
# 2 "<built-in>" 2
# 1 "EEPROM/EEPROM.c" 2
# 1 "EEPROM/EEPROM.h" 1



# 1 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 1 3



# 1 "/opt/microchip/xc8/v3.10/pic/include/c99/musl_xc8.h" 1 3
# 5 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 2 3
# 26 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 3
# 1 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 1 3
# 133 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 3
typedef unsigned short uintptr_t;
# 148 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 3
typedef short intptr_t;
# 164 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 3
typedef signed char int8_t;




typedef short int16_t;




typedef __int24 int24_t;




typedef long int32_t;





typedef long long int64_t;
# 194 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 3
typedef long long intmax_t;





typedef unsigned char uint8_t;




typedef unsigned short uint16_t;




typedef __uint24 uint24_t;




typedef unsigned long uint32_t;





typedef unsigned long long uint64_t;
# 235 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/alltypes.h" 3
typedef unsigned long long uintmax_t;
# 27 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 2 3

typedef int8_t int_fast8_t;

typedef int64_t int_fast64_t;


typedef int8_t int_least8_t;
typedef int16_t int_least16_t;

typedef int24_t int_least24_t;
typedef int24_t int_fast24_t;

typedef int32_t int_least32_t;

typedef int64_t int_least64_t;


typedef uint8_t uint_fast8_t;

typedef uint64_t uint_fast64_t;


typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;

typedef uint24_t uint_least24_t;
typedef uint24_t uint_fast24_t;

typedef uint32_t uint_least32_t;

typedef uint64_t uint_least64_t;
# 148 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 3
# 1 "/opt/microchip/xc8/v3.10/pic/include/c99/bits/stdint.h" 1 3
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
# 149 "/opt/microchip/xc8/v3.10/pic/include/c99/stdint.h" 2 3
# 5 "EEPROM/EEPROM.h" 2
# 46 "EEPROM/EEPROM.h"
void EEPROM_WriteConfig(uint8_t config_id, uint8_t value);
uint8_t EEPROM_ReadConfig(uint8_t config_id);

void EEPROM_WriteRecord(uint16_t base_addr, uint8_t h, uint8_t m, uint8_t s, uint8_t T, uint8_t L);
void EEPROM_ReadRecord(uint16_t base_addr, uint8_t *h, uint8_t *m, uint8_t *s, uint8_t *T, uint8_t *L);

void EEPROM_WriteHeader(uint16_t magic, uint8_t checksum);
void EEPROM_ReadHeader(uint16_t *magic, uint8_t *checksum);
# 2 "EEPROM/EEPROM.c" 2
# 1 "EEPROM/../mcc_generated_files/memory.h" 1
# 54 "EEPROM/../mcc_generated_files/memory.h"
# 1 "/opt/microchip/xc8/v3.10/pic/include/c99/stdbool.h" 1 3
# 55 "EEPROM/../mcc_generated_files/memory.h" 2
# 99 "EEPROM/../mcc_generated_files/memory.h"
uint16_t FLASH_ReadWord(uint16_t flashAddr);
# 128 "EEPROM/../mcc_generated_files/memory.h"
void FLASH_WriteWord(uint16_t flashAddr, uint16_t *ramBuf, uint16_t word);
# 164 "EEPROM/../mcc_generated_files/memory.h"
int8_t FLASH_WriteBlock(uint16_t writeAddr, uint16_t *flashWordArray);
# 189 "EEPROM/../mcc_generated_files/memory.h"
void FLASH_EraseBlock(uint16_t startAddr);
# 222 "EEPROM/../mcc_generated_files/memory.h"
void DATAEE_WriteByte(uint16_t bAdd, uint8_t bData);
# 248 "EEPROM/../mcc_generated_files/memory.h"
uint8_t DATAEE_ReadByte(uint16_t bAdd);
# 3 "EEPROM/EEPROM.c" 2

void EEPROM_WriteConfig(uint8_t config_id, uint8_t value) {
    DATAEE_WriteByte(0x03 + config_id, value);
}

uint8_t EEPROM_ReadConfig(uint8_t config_id) {
    return DATAEE_ReadByte(0x03 + config_id);
}

void EEPROM_WriteRecord(uint16_t base_addr, uint8_t h, uint8_t m, uint8_t s, uint8_t T, uint8_t L) {
    DATAEE_WriteByte(base_addr, h);
    DATAEE_WriteByte(base_addr + 1, m);
    DATAEE_WriteByte(base_addr + 2, s);
    DATAEE_WriteByte(base_addr + 3, T);
    DATAEE_WriteByte(base_addr + 4, L);
}

void EEPROM_ReadRecord(uint16_t base_addr, uint8_t *h, uint8_t *m, uint8_t *s, uint8_t *T, uint8_t *L) {
    *h = DATAEE_ReadByte(base_addr);
    *m = DATAEE_ReadByte(base_addr + 1);
    *s = DATAEE_ReadByte(base_addr + 2);
    *T = DATAEE_ReadByte(base_addr + 3);
    *L = DATAEE_ReadByte(base_addr + 4);
}

void EEPROM_WriteHeader(uint16_t magic, uint8_t checksum) {
    DATAEE_WriteByte((0x00), (magic & 0xFF));
    DATAEE_WriteByte((0x00) + 1, (magic >> 8) & 0xFF);
    DATAEE_WriteByte((0x00 + 2), checksum);
}

void EEPROM_ReadHeader(uint16_t *magic, uint8_t *checksum) {
    uint8_t low = DATAEE_ReadByte((0x00));
    uint8_t high = DATAEE_ReadByte((0x00) + 1);
    *magic = (uint16_t)( (high << 8) | low) ;
    *checksum = DATAEE_ReadByte((0x00 + 2));
}
