#ifndef WEATHER_UART_H
#define WEATHER_UART_H

#include "main.h"


typedef struct
{
    int16_t temperature_x100;

    uint16_t humidity_x100;

    uint32_t pressure_x100;

    uint16_t light_lux;

    uint16_t rain_raw;

    uint8_t rain_state;

} WeatherData_t;


/*
 * Gui mot packet weather qua UART.
 *
 * Packet:
 *
 * Byte 0      : 0xAA
 * Byte 1      : 0x55
 *
 * Byte 2-3    : temperature x100
 * Byte 4-5    : humidity x100
 * Byte 6-9    : pressure x100
 * Byte 10-11  : light lux
 * Byte 12-13  : rain raw
 * Byte 14     : rain state
 * Byte 15     : XOR checksum
 */

HAL_StatusTypeDef WeatherUART_Send(
    UART_HandleTypeDef *huart,
    const WeatherData_t *data
);


#endif