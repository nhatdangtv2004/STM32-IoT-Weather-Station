#ifndef RAIN_SENSOR_STM32_H
#define RAIN_SENSOR_STM32_H

#include "main.h"

uint8_t RainSensor_Init(ADC_HandleTypeDef *hadc);

uint8_t RainSensor_ReadRaw(uint16_t *raw_value);

#endif