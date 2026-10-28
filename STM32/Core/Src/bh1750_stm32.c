#include "bh1750_stm32.h"


/* =========================================================
   PRIVATE VARIABLES
   ========================================================= */

static I2C_HandleTypeDef *bh1750_i2c = NULL;

static uint8_t bh1750_address = 0;


/* =========================================================
   BH1750 COMMANDS
   ========================================================= */

#define BH1750_POWER_ON              0x01
#define BH1750_CONT_HIGH_RES_MODE    0x10


/* =========================================================
   INIT
   ========================================================= */

uint8_t BH1750_Init(
    I2C_HandleTypeDef *hi2c
)
{
    uint8_t command;


    bh1750_i2c = hi2c;


    /* -----------------------------------------------------
       Detect address

       ADDR LOW  -> 0x23
       ADDR HIGH -> 0x5C
       ----------------------------------------------------- */

    if (HAL_I2C_IsDeviceReady(
            bh1750_i2c,
            (0x23 << 1),
            3,
            100) == HAL_OK)
    {
        bh1750_address = 0x23;
    }

    else if (HAL_I2C_IsDeviceReady(
                 bh1750_i2c,
                 (0x5C << 1),
                 3,
                 100) == HAL_OK)
    {
        bh1750_address = 0x5C;
    }

    else
    {
        bh1750_address = 0;

        return 0;
    }


    /* -----------------------------------------------------
       POWER ON
       ----------------------------------------------------- */

    command =
        BH1750_POWER_ON;


    if (HAL_I2C_Master_Transmit(
            bh1750_i2c,
            (uint16_t)(bh1750_address << 1),
            &command,
            1,
            100) != HAL_OK)
    {
        return 0;
    }


    HAL_Delay(10);


    /* -----------------------------------------------------
       CONTINUOUS HIGH RESOLUTION MODE

       Resolution approximately 1 lx
       Typical measurement time ~120 ms
       ----------------------------------------------------- */

    command =
        BH1750_CONT_HIGH_RES_MODE;


    if (HAL_I2C_Master_Transmit(
            bh1750_i2c,
            (uint16_t)(bh1750_address << 1),
            &command,
            1,
            100) != HAL_OK)
    {
        return 0;
    }


    /*
     * Cho sample dau tien hoan thanh.
     */

    HAL_Delay(180);


    return 1;
}


/* =========================================================
   READ LUX
   ========================================================= */

uint8_t BH1750_ReadLux(
    float *lux
)
{
    uint8_t data[2];

    uint16_t raw;


    if (lux == NULL)
    {
        return 0;
    }


    if (bh1750_i2c == NULL)
    {
        return 0;
    }


    if (bh1750_address == 0)
    {
        return 0;
    }


    /* -----------------------------------------------------
       Read 2-byte measurement
       ----------------------------------------------------- */

    if (HAL_I2C_Master_Receive(
            bh1750_i2c,
            (uint16_t)(bh1750_address << 1),
            data,
            2,
            100) != HAL_OK)
    {
        return 0;
    }


    raw =
        ((uint16_t)data[0] << 8)
        |
        data[1];


    /*
     * BH1750 standard conversion:
     *
     * lux = raw / 1.2
     */

    *lux =
        (float)raw
        /
        1.2f;


    return 1;
}


/* =========================================================
   GET I2C ADDRESS
   ========================================================= */

uint8_t BH1750_GetAddress(void)
{
    return bh1750_address;
}