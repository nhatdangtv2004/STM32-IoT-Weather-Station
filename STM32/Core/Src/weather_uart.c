#include "weather_uart.h"


#define WEATHER_PACKET_SIZE       16U

#define WEATHER_HEADER_1          0xAAU
#define WEATHER_HEADER_2          0x55U


/* =========================================================
   PUT UINT16 LITTLE ENDIAN
   ========================================================= */

static void PutU16LE(
    uint8_t *buffer,
    uint16_t value
)
{
    buffer[0] =
        (uint8_t)(
            value & 0xFFU
        );

    buffer[1] =
        (uint8_t)(
            (value >> 8) & 0xFFU
        );
}


/* =========================================================
   PUT UINT32 LITTLE ENDIAN
   ========================================================= */

static void PutU32LE(
    uint8_t *buffer,
    uint32_t value
)
{
    buffer[0] =
        (uint8_t)(
            value & 0xFFU
        );

    buffer[1] =
        (uint8_t)(
            (value >> 8) & 0xFFU
        );

    buffer[2] =
        (uint8_t)(
            (value >> 16) & 0xFFU
        );

    buffer[3] =
        (uint8_t)(
            (value >> 24) & 0xFFU
        );
}


/* =========================================================
   CALCULATE XOR CHECKSUM
   ========================================================= */

static uint8_t WeatherUART_Checksum(
    const uint8_t *data,
    uint8_t length
)
{
    uint8_t checksum = 0U;

    uint8_t i;


    for (i = 0U;
         i < length;
         i++)
    {
        checksum ^=
            data[i];
    }


    return checksum;
}


/* =========================================================
   SEND WEATHER PACKET
   ========================================================= */

HAL_StatusTypeDef WeatherUART_Send(
    UART_HandleTypeDef *huart,
    const WeatherData_t *data
)
{
    uint8_t packet[
        WEATHER_PACKET_SIZE
    ];


    uint16_t temp_raw;


    if ((huart == NULL) ||
        (data == NULL))
    {
        return HAL_ERROR;
    }


    /* =============================================
       HEADER
       ============================================= */

    packet[0] =
        WEATHER_HEADER_1;

    packet[1] =
        WEATHER_HEADER_2;


    /* =============================================
       TEMPERATURE

       Signed int16.
       Example:
       2843 = 28.43 C
       ============================================= */

    temp_raw =
        (uint16_t)(
            data->temperature_x100
        );


    PutU16LE(
        &packet[2],
        temp_raw
    );


    /* =============================================
       HUMIDITY

       Example:
       6521 = 65.21 %
       ============================================= */

    PutU16LE(
        &packet[4],
        data->humidity_x100
    );


    /* =============================================
       PRESSURE

       Example:
       100842 = 1008.42 hPa
       ============================================= */

    PutU32LE(
        &packet[6],
        data->pressure_x100
    );


    /* =============================================
       LIGHT
       ============================================= */

    PutU16LE(
        &packet[10],
        data->light_lux
    );


    /* =============================================
       RAIN ADC
       ============================================= */

    PutU16LE(
        &packet[12],
        data->rain_raw
    );


    /* =============================================
       RAIN STATE

       0 = DRY
       1 = LOW
       2 = HEAVY
       ============================================= */

    packet[14] =
        data->rain_state;


    /* =============================================
       CHECKSUM
       ============================================= */

    packet[15] =
        WeatherUART_Checksum(
            packet,
            15U
        );


    /* =============================================
       TRANSMIT
       ============================================= */

    return HAL_UART_Transmit(
        huart,
        packet,
        WEATHER_PACKET_SIZE,
        100
    );
}