#ifndef BMP388_STM32_H
#define BMP388_STM32_H

#include "main.h"
#include "bmp3.h"

/*
 * Khoi tao BMP388.
 *
 * Return:
 * 1 = OK
 * 0 = ERROR
 */
uint8_t BMP388_Init(I2C_HandleTypeDef *hi2c);


/*
 * Doc nhiet do va ap suat.
 *
 * temperature_c:
 *      don vi do C
 *
 * pressure_hpa:
 *      don vi hPa
 *
 * Return:
 * 1 = OK
 * 0 = ERROR
 */
uint8_t BMP388_Read(
    float *temperature_c,
    float *pressure_hpa
);


/*
 * Tra ve dia chi I2C dang su dung:
 *
 * 0x76
 * hoac
 * 0x77
 */
uint8_t BMP388_GetAddress(void);


#endif