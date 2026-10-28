#include "rain_sensor_stm32.h"


static ADC_HandleTypeDef *rain_adc = NULL;


/* =========================================================
   INIT
   ========================================================= */

uint8_t RainSensor_Init(
    ADC_HandleTypeDef *hadc
)
{
    if (hadc == NULL)
    {
        return 0;
    }

    rain_adc = hadc;

    return 1;
}


/* =========================================================
   READ RAW ADC

   STM32F103 ADC = 12-bit

   Range:
   0 ... 4095
   ========================================================= */

uint8_t RainSensor_ReadRaw(
    uint16_t *raw_value
)
{
    uint32_t adc_value;


    if ((rain_adc == NULL) ||
        (raw_value == NULL))
    {
        return 0;
    }


    if (HAL_ADC_Start(rain_adc) != HAL_OK)
    {
        return 0;
    }


    if (HAL_ADC_PollForConversion(
            rain_adc,
            100) != HAL_OK)
    {
        HAL_ADC_Stop(rain_adc);

        return 0;
    }


    adc_value =
        HAL_ADC_GetValue(rain_adc);


    HAL_ADC_Stop(rain_adc);


    if (adc_value > 4095)
    {
        adc_value = 4095;
    }


    *raw_value =
        (uint16_t)adc_value;


    return 1;
}