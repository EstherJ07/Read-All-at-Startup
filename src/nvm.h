#ifndef NVM_H
#define NVM_H
#include "sdk_project_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


#define EEPROM_I2C_ADDRESS    0x50U
#define EEPROM_CAPACITY       512U
#define EEPROM_PAGE_SIZE      16U
#define I2C_TIMEOUT           1000U
#define UART_TIMEOUT          1000U
#define NVM_BLOCK_COUNT       3U
#define BLOCK_LENGTH          3U
#define CRC_SIZE              1U
#define RECORD_SIZE           (BLOCK_LENGTH + CRC_SIZE)
#define LPI2C_INSTANCE        0U
#define BLOCK1_ADDRESS        0x000U
#define BLOCK2_ADDRESS        0x010U
#define BLOCK3_ADDRESS        0x020U
#define WRITE_INITIAL_DATA    1U

void NvM_Init(void);
void NvM_Run(void);

#endif
