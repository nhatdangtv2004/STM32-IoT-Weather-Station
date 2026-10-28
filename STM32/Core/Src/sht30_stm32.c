#include "sht30_stm32.h"


/* =========================================================
   PRIVATE VARIABLES
   ========================================================= */

static I2C_HandleTypeDef *sht30_i2c = NULL;

static uint8_t sht30_address = 0;


/* =========================================================
   CRC-8

   Polynomial = 0x31
   Initial    = 0xFF
   ========================================================= */

static uint8_t SHT30_CRC8(
    const uint8_t *data,
    uint8_t len
)
{
    uint8_t crc = 0xFF;

    uint8_t i;
    uint8_t bit;


    for (i = 0; i < len; i++)
    {
        crc ^= data[i];


        for (bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
            {
                crc =
                    (uint8_t)(
                        (crc << 1) ^ 0x31
                    );
            }
            else
            {
                crc <<= 1;
            }
        }
    }


    return crc;
}


/* =========================================================
   INIT
   ========================================================= */

uint8_t SHT30_Init(
    I2C_HandleTypeDef *hi2c
)
{
    uint8_t reset_cmd[2];


    sht30_i2c = hi2c;


    /* -----------------------------------------------------
       Detect address

       Default SHT30 = 0x44
       Alternative  = 0x45
       ----------------------------------------------------- */

    if (HAL_I2C_IsDeviceReady(
            sht30_i2c,
            (0x44 << 1),
            3,
            100) == HAL_OK)
    {
        sht30_address = 0x44;
    }

    else if (HAL_I2C_IsDeviceReady(
                 sht30_i2c,
                 (0x45 << 1),
                 3,
                 100) == HAL_OK)
    {
        sht30_address = 0x45;
    }

    else
    {
        sht30_address = 0;

        return 0;
    }


    /* -----------------------------------------------------
       Soft reset

       Command = 0x30A2
       ----------------------------------------------------- */

    reset_cmd[0] = 0x30;
    reset_cmd[1] = 0xA2;


    if (HAL_I2C_Master_Transmit(
            sht30_i2c,
            (uint16_t)(sht30_address << 1),
            reset_cmd,
            2,
            100) != HAL_OK)
    {
        return 0;
    }


    HAL_Delay(10);


    return 1;
}


/* =========================================================
   READ TEMPERATURE + HUMIDITY
   ========================================================= */

uint8_t SHT30_Read(
    float *temperature_c,
    float *humidity_rh
)
{
    uint8_t command[2];

    uint8_t data[6];

    uint16_t raw_temperature;

    uint16_t raw_humidity;


    if ((temperature_c == NULL) ||
        (humidity_rh == NULL))
    {
        return 0;
    }


    if (sht30_i2c == NULL)
    {
        return 0;
    }


    if (sht30_address == 0)
    {
        return 0;
    }


    /* -----------------------------------------------------
       Single shot measurement

       High repeatability
       Clock stretching disabled

       Command = 0x2400
       ----------------------------------------------------- */

    command[0] = 0x24;
    command[1] = 0x00;


    if (HAL_I2C_Master_Transmit(
            sht30_i2c,
            (uint16_t)(sht30_address << 1),
            command,
            2,
            100) != HAL_OK)
    {
        return 0;
    }


    /*
     * Cho sensor do xong.
     */

    HAL_Delay(20);


    /* -----------------------------------------------------
       Read 6 bytes:

       Byte 0: Temp MSB
       Byte 1: Temp LSB
       Byte 2: Temp CRC

       Byte 3: RH MSB
       Byte 4: RH LSB
       Byte 5: RH CRC
       ----------------------------------------------------- */

    if (HAL_I2C_Master_Receive(
            sht30_i2c,
            (uint16_t)(sht30_address << 1),
            data,
            6,
            100) != HAL_OK)
    {
        return 0;
    }


    /* -----------------------------------------------------
       Check CRC
       ----------------------------------------------------- */

    if (SHT30_CRC8(
            &data[0],
            2) != data[2])
    {
        return 0;
    }


    if (SHT30_CRC8(
            &data[3],
            2) != data[5])
    {
        return 0;
    }


    /* -----------------------------------------------------
       Combine raw data
       ----------------------------------------------------- */

    raw_temperature =
        ((uint16_t)data[0] << 8)
        |
        data[1];


    raw_humidity =
        ((uint16_t)data[3] << 8)
        |
        data[4];


    /* -----------------------------------------------------
       Convert raw -> physical values

       T = -45 + 175 * raw / 65535

       RH = 100 * raw / 65535
       ----------------------------------------------------- */

    *temperature_c =
        -45.0f
        +
        (
            175.0f
            *
            (float)raw_temperature
            /
            65535.0f
        );


    *humidity_rh =
        100.0f
        *
        (float)raw_humidity
        /
        65535.0f;


    /*
     * Bao ve neu module clone tra ve gia tri vuot gioi han.
     */

    if (*humidity_rh < 0.0f)
    {
        *humidity_rh = 0.0f;
    }


    if (*humidity_rh > 100.0f)
    {
        *humidity_rh = 100.0f;
    }


    return 1;
}


/* =========================================================
   GET ADDRESS
   ========================================================= */

uint8_t SHT30_GetAddress(void)
{
    return sht30_address;
}