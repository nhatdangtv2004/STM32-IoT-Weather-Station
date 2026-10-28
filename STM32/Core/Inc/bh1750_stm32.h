#ifndef BH1750_STM32_H
#define BH1750_STM32_H

#include "main.h"

uint8_t BH1750_Init(I2C_HandleTypeDef *hi2c);

uint8_t BH1750_ReadLux(float *lux);

uint8_t BH1750_GetAddress(void);

#endif