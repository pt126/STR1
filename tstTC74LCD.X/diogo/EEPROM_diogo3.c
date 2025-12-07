#include "EEPROM.h"
#include "../mcc_generated_files/memory.h" 

void EEPROM_WriteConfig(uint8_t config_id, uint8_t value) {
    DATAEE_WriteByte(EEPROM_ADDR_CONFIG + config_id, value);
}

uint8_t EEPROM_ReadConfig(uint8_t config_id) {
    return DATAEE_ReadByte(EEPROM_ADDR_CONFIG + config_id);
}

void EEPROM_WriteRecord(uint16_t base_addr, uint8_t h, uint8_t m, uint8_t s, uint8_t T, uint8_t L) {
    DATAEE_WriteByte(base_addr,     h);
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
    DATAEE_WriteByte(EEPROM_ADDR_MAGIC,     (magic & 0xFF));
    DATAEE_WriteByte(EEPROM_ADDR_MAGIC + 1, (magic >> 8) & 0xFF);
    DATAEE_WriteByte(EEPROM_ADDR_CHECKSUM,  checksum);
}

void EEPROM_ReadHeader(uint16_t *magic, uint8_t *checksum) {
    uint8_t low = DATAEE_ReadByte(EEPROM_ADDR_MAGIC);
    uint8_t high = DATAEE_ReadByte(EEPROM_ADDR_MAGIC + 1);
    *magic = (high << 8) | low;
    *checksum = DATAEE_ReadByte(EEPROM_ADDR_CHECKSUM);
}
