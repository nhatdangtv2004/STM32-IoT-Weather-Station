#ifndef SHT30_STM32_H
#define SHT30_STM32_H

#include "main.h"

uint8_t SHT30_Init(I2C_HandleTypeDef *hi2c);

uint8_t SHT30_Read(
    float *temperature_c,
    float *humidity_rh
);

uint8_t SHT30_GetAddress(void);

#endif