#include "bmp388_stm32.h"

#include <string.h>


/* =========================================================
   PRIVATE VARIABLES
   ========================================================= */

static struct bmp3_dev bmp_dev;

static struct bmp3_settings bmp_settings;

static I2C_HandleTypeDef *bmp_i2c = NULL;

static uint8_t bmp_address = 0;


/* =========================================================
   BOSCH I2C READ CALLBACK
   ========================================================= */

static BMP3_INTF_RET_TYPE BMP388_I2C_Read(
    uint8_t reg_addr,
    uint8_t *read_data,
    uint32_t len,
    void *intf_ptr
)
{
    uint8_t address;

    address = *((uint8_t *)intf_ptr);


    if (HAL_I2C_Mem_Read(
            bmp_i2c,
            (uint16_t)(address << 1),
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            read_data,
            (uint16_t)len,
            100) == HAL_OK)
    {
        return BMP3_INTF_RET_SUCCESS;
    }


    return -1;
}


/* =========================================================
   BOSCH I2C WRITE CALLBACK
   ========================================================= */

static BMP3_INTF_RET_TYPE BMP388_I2C_Write(
    uint8_t reg_addr,
    const uint8_t *write_data,
    uint32_t len,
    void *intf_ptr
)
{
    uint8_t address;

    address = *((uint8_t *)intf_ptr);


    if (HAL_I2C_Mem_Write(
            bmp_i2c,
            (uint16_t)(address << 1),
            reg_addr,
            I2C_MEMADD_SIZE_8BIT,
            (uint8_t *)write_data,
            (uint16_t)len,
            100) == HAL_OK)
    {
        return BMP3_INTF_RET_SUCCESS;
    }


    return -1;
}


/* =========================================================
   BOSCH DELAY CALLBACK
   Bosch gui delay theo microsecond.

   HAL_Delay() dung millisecond,
   nen lam tron len.
   ========================================================= */

static void BMP388_DelayUs(
    uint32_t period,
    void *intf_ptr
)
{
    uint32_t delay_ms;

    (void)intf_ptr;


    if (period == 0)
    {
        return;
    }


    delay_ms = (period + 999U) / 1000U;


    if (delay_ms == 0)
    {
        delay_ms = 1;
    }


    HAL_Delay(delay_ms);
}


/* =========================================================
   BMP388 INIT
   ========================================================= */

uint8_t BMP388_Init(
    I2C_HandleTypeDef *hi2c
)
{
    int8_t result;

    uint32_t settings_sel;


    /* ---------------------------------------------
       Save I2C handle
       --------------------------------------------- */

    bmp_i2c = hi2c;


    /* ---------------------------------------------
       Auto detect address

       Thu 0x77 truoc
       --------------------------------------------- */

    if (HAL_I2C_IsDeviceReady(
            bmp_i2c,
            (0x77 << 1),
            3,
            100) == HAL_OK)
    {
        bmp_address = 0x77;
    }

    else if (HAL_I2C_IsDeviceReady(
                 bmp_i2c,
                 (0x76 << 1),
                 3,
                 100) == HAL_OK)
    {
        bmp_address = 0x76;
    }

    else
    {
        bmp_address = 0;

        return 0;
    }


    /* ---------------------------------------------
       Clear structures
       --------------------------------------------- */

    memset(
        &bmp_dev,
        0,
        sizeof(bmp_dev)
    );


    memset(
        &bmp_settings,
        0,
        sizeof(bmp_settings)
    );


    /* ---------------------------------------------
       Connect Bosch driver to STM32 HAL
       --------------------------------------------- */

    bmp_dev.intf =
        BMP3_I2C_INTF;

    bmp_dev.read =
        BMP388_I2C_Read;

    bmp_dev.write =
        BMP388_I2C_Write;

    bmp_dev.delay_us =
        BMP388_DelayUs;

    bmp_dev.intf_ptr =
        &bmp_address;


    /* ---------------------------------------------
       Bosch init

       bmp3_init() se:
       - doc chip ID
       - doc calibration data
       --------------------------------------------- */

    result =
        bmp3_init(&bmp_dev);


    if (result != BMP3_OK)
    {
        return 0;
    }


    /* ---------------------------------------------
       Dam bao dung la BMP388

       BMP388 chip ID = 0x50
       --------------------------------------------- */

    if (bmp_dev.chip_id != BMP3_CHIP_ID)
    {
        return 0;
    }


    /* ---------------------------------------------
       Enable pressure + temperature
       --------------------------------------------- */

    bmp_settings.press_en =
        BMP3_ENABLE;

    bmp_settings.temp_en =
        BMP3_ENABLE;


    /* ---------------------------------------------
       Oversampling

       x2 pressure
       x2 temperature
       --------------------------------------------- */

    bmp_settings.odr_filter.press_os =
        BMP3_OVERSAMPLING_2X;

    bmp_settings.odr_filter.temp_os =
        BMP3_OVERSAMPLING_2X;


    /* ---------------------------------------------
       IIR filter
       --------------------------------------------- */

    bmp_settings.odr_filter.iir_filter =
        BMP3_IIR_FILTER_COEFF_3;


    /* ---------------------------------------------
       Output Data Rate

       Khoang 1.5 sample / second
       Du cho Weather Station.
       --------------------------------------------- */

    bmp_settings.odr_filter.odr =
        BMP3_ODR_1_5_HZ;


    /* ---------------------------------------------
       Select settings to write
       --------------------------------------------- */

    settings_sel =
          BMP3_SEL_PRESS_EN
        | BMP3_SEL_TEMP_EN
        | BMP3_SEL_PRESS_OS
        | BMP3_SEL_TEMP_OS
        | BMP3_SEL_IIR_FILTER
        | BMP3_SEL_ODR;


    result =
        bmp3_set_sensor_settings(
            settings_sel,
            &bmp_settings,
            &bmp_dev
        );


    if (result != BMP3_OK)
    {
        return 0;
    }


    /* ---------------------------------------------
       Normal mode
       --------------------------------------------- */

    bmp_settings.op_mode =
        BMP3_MODE_NORMAL;


    result =
        bmp3_set_op_mode(
            &bmp_settings,
            &bmp_dev
        );


    if (result != BMP3_OK)
    {
        return 0;
    }


    /*
     * Cho sensor tao measurement dau tien.
     */

    HAL_Delay(1000);


    return 1;
}


/* =========================================================
   BMP388 READ
   ========================================================= */

uint8_t BMP388_Read(
    float *temperature_c,
    float *pressure_hpa
)
{
    int8_t result;

    struct bmp3_data data;


    if ((temperature_c == NULL) ||
        (pressure_hpa == NULL))
    {
        return 0;
    }


    memset(
        &data,
        0,
        sizeof(data)
    );


    /* ---------------------------------------------
       Bosch doc + compensation
       --------------------------------------------- */

    result =
        bmp3_get_sensor_data(
            BMP3_PRESS_TEMP,
            &data,
            &bmp_dev
        );


    if (result != BMP3_OK)
    {
        return 0;
    }


    /*
     * Bosch driver hien tai mac dinh dung
     * floating-point compensation.
     *
     * data.temperature:
     * degree Celsius
     *
     * data.pressure:
     * Pascal
     */


    *temperature_c =
        (float)data.temperature;


    /*
     * Pa -> hPa
     *
     * 100800 Pa
     * =
     * 1008 hPa
     */

    *pressure_hpa =
        (float)(data.pressure / 100.0);


    return 1;
}


/* =========================================================
   GET ADDRESS
   ========================================================= */

uint8_t BMP388_GetAddress(void)
{
    return bmp_address;
}