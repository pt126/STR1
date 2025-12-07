#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

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

// EEPROM Address Map
#define EEPROM_ADDR_HEADER      0x00
#define MAGIC_WORD              0xF1A5
#define EEPROM_ADDR_MAGIC       (EEPROM_ADDR_HEADER)
#define EEPROM_ADDR_CHECKSUM    (EEPROM_ADDR_HEADER + 2)

#define EEPROM_ADDR_CONFIG      0x03
#define EEPROM_CONFIG_PMON      0x00
#define EEPROM_CONFIG_TALA      0x01
#define EEPROM_CONFIG_TINA      0x02
#define EEPROM_CONFIG_ALAF      0x03
#define EEPROM_CONFIG_ALAH      0x04
#define EEPROM_CONFIG_ALAM      0x05
#define EEPROM_CONFIG_ALAS      0x06
#define EEPROM_CONFIG_ALAT      0x07
#define EEPROM_CONFIG_ALAL      0x08
#define EEPROM_CONFIG_CLKH      0x09
#define EEPROM_CONFIG_CLKM      0x0A

#define EEPROM_ADDR_RECORDS     0x0E
#define EEPROM_RECORD_SIZE      0x05
#define EEPROM_ADDR_TMAX        (EEPROM_ADDR_RECORDS)
#define EEPROM_ADDR_TMIN        (EEPROM_ADDR_RECORDS + EEPROM_RECORD_SIZE)
#define EEPROM_ADDR_LMAX        (EEPROM_ADDR_RECORDS + 2 * EEPROM_RECORD_SIZE)
#define EEPROM_ADDR_LMIN        (EEPROM_ADDR_RECORDS + 3 * EEPROM_RECORD_SIZE)

// Helper functions
void EEPROM_WriteConfig(uint8_t config_id, uint8_t value);
uint8_t EEPROM_ReadConfig(uint8_t config_id);

void EEPROM_WriteRecord(uint16_t base_addr, uint8_t h, uint8_t m, uint8_t s, uint8_t T, uint8_t L);
void EEPROM_ReadRecord(uint16_t base_addr, uint8_t *h, uint8_t *m, uint8_t *s, uint8_t *T, uint8_t *L);

void EEPROM_WriteHeader(uint16_t magic, uint8_t checksum);
void EEPROM_ReadHeader(uint16_t *magic, uint8_t *checksum);

void UpdateEEPROMChecksum(void);


#endif // EEPROM_H