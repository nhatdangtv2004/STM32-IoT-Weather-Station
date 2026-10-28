/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "bmp388_stm32.h"
#include "sht30_stm32.h"
#include "bh1750_stm32.h"
#include "rain_sensor_stm32.h"
#include "weather_uart.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * Rain states
 */
#define RAIN_STATE_DRY       0U
#define RAIN_STATE_LOW       1U
#define RAIN_STATE_HEAVY     2U


/*
 * Gia tri da hieu chuan gan dung:
 *
 * Dry        ~ 4000
 * Low rain   ~ 2000
 * Heavy rain ~ 1000
 *
 * Khong so sanh == 4000 / 2000 / 1000
 * vi ADC luon dao dong.
 *
 * Dung midpoint lam threshold:
 *
 * 3000 = midpoint 4000 va 2000
 * 1500 = midpoint 2000 va 1000
 */

#define RAIN_DRY_THRESHOLD       3000U
#define RAIN_HEAVY_THRESHOLD     1500U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

uint8_t bmp388_ok = 0;
uint8_t sht30_ok = 0;
uint8_t bh1750_ok = 0;
uint8_t rain_ok = 0;


uint8_t rain_state =
    RAIN_STATE_DRY;


float bmp_temperature_c =
    0.0f;

float bmp_pressure_hpa =
    0.0f;


float sht30_temperature_c =
    0.0f;

float sht30_humidity_rh =
    0.0f;


float bh1750_lux =
    0.0f;


uint16_t rain_raw =
    0;


/*
 * Du lieu chung cua Weather Station.
 *
 * Sau nay ESP32, OLED, SD card...
 * deu co the dung struct nay.
 */

WeatherData_t weather_data =
{
    0
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
/* USER CODE BEGIN PFP */

void OLED_SetCursor(uint8_t column, uint8_t page);

void OLED_GetGlyph(char c, uint8_t glyph[5]);

void OLED_WriteChar(char c);

void OLED_WriteString(const char *str);

void OLED_WriteUnsigned(uint32_t value);

void OLED_WriteSignedFixed2(int32_t value);

void OLED_WriteUnsignedFixed2(uint32_t value);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define OLED_ADDR      (0x3C << 1)

#define BMP388_ADDR_76 (0x76 << 1)
#define BMP388_ADDR_77 (0x77 << 1)

#define BMP388_REG_CHIP_ID 0x00
#define BMP388_CHIP_ID     0x50

uint16_t bmp388_addr = 0;
uint8_t bmp388_chip_id = 0;


/* ================= OLED ================= */

void OLED_Command(uint8_t cmd)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = cmd;

    HAL_I2C_Master_Transmit(
        &hi2c1,
        OLED_ADDR,
        data,
        2,
        HAL_MAX_DELAY
    );
}


void OLED_Data(uint8_t data_byte)
{
    uint8_t data[2];

    data[0] = 0x40;
    data[1] = data_byte;

    HAL_I2C_Master_Transmit(
        &hi2c1,
        OLED_ADDR,
        data,
        2,
        HAL_MAX_DELAY
    );
}


void OLED_Init(void)
{
    HAL_Delay(100);

    OLED_Command(0xAE);

    OLED_Command(0x20);
    OLED_Command(0x02);

    OLED_Command(0xB0);

    OLED_Command(0xC8);

    OLED_Command(0x00);
    OLED_Command(0x10);

    OLED_Command(0x40);

    OLED_Command(0x81);
    OLED_Command(0x7F);

    OLED_Command(0xA1);

    OLED_Command(0xA6);

    OLED_Command(0xA8);
    OLED_Command(0x3F);

    OLED_Command(0xA4);

    OLED_Command(0xD3);
    OLED_Command(0x00);

    OLED_Command(0xD5);
    OLED_Command(0x80);

    OLED_Command(0xD9);
    OLED_Command(0xF1);

    OLED_Command(0xDA);
    OLED_Command(0x12);

    OLED_Command(0xDB);
    OLED_Command(0x40);

    OLED_Command(0x8D);
    OLED_Command(0x14);

    OLED_Command(0xAF);
}


void OLED_Fill(uint8_t value)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        OLED_Command(0xB0 + page);
        OLED_Command(0x00);
        OLED_Command(0x10);

        for (uint8_t col = 0; col < 128; col++)
        {
            OLED_Data(value);
        }
    }
}


/* ================= BMP388 ================= */

HAL_StatusTypeDef BMP388_ReadRegister(
    uint16_t device_addr,
    uint8_t reg,
    uint8_t *data
)
{
    return HAL_I2C_Mem_Read(
        &hi2c1,
        device_addr,
        reg,
        I2C_MEMADD_SIZE_8BIT,
        data,
        1,
        100
    );
}


uint8_t BMP388_Detect(void)
{
    /*
     * Thu 0x77 truoc
     */

    if (HAL_I2C_IsDeviceReady(
            &hi2c1,
            BMP388_ADDR_77,
            3,
            100) == HAL_OK)
    {
        bmp388_addr = BMP388_ADDR_77;
    }

    /*
     * Neu khong co 0x77 thi thu 0x76
     */

    else if (HAL_I2C_IsDeviceReady(
                 &hi2c1,
                 BMP388_ADDR_76,
                 3,
                 100) == HAL_OK)
    {
        bmp388_addr = BMP388_ADDR_76;
    }

    else
    {
        bmp388_addr = 0;
        return 0;
    }


    /*
     * Doc CHIP_ID register 0x00
     */

    if (BMP388_ReadRegister(
            bmp388_addr,
            BMP388_REG_CHIP_ID,
            &bmp388_chip_id) != HAL_OK)
    {
        return 0;
    }


    /*
     * BMP388 dung phai tra ve 0x50
     */

    if (bmp388_chip_id == BMP388_CHIP_ID)
    {
        return 1;
    }

    return 2;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */

OLED_Init();

OLED_Fill(0x00);


/* =========================================================
   BUZZER OFF AT STARTUP

   Active LOW
   HIGH = OFF
   ========================================================= */

HAL_GPIO_WritePin(
    GPIOB,
    GPIO_PIN_0,
    GPIO_PIN_SET
);


/* =========================================================
   ADC CALIBRATION
   ========================================================= */

if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
{
    Error_Handler();
}


/* =========================================================
   INIT BMP388
   ========================================================= */

bmp388_ok =
    BMP388_Init(&hi2c1);


/* =========================================================
   INIT SHT30
   ========================================================= */

sht30_ok =
    SHT30_Init(&hi2c1);


/* =========================================================
   INIT BH1750
   ========================================================= */

bh1750_ok =
    BH1750_Init(&hi2c1);


/* =========================================================
   INIT RAIN SENSOR
   ========================================================= */

rain_ok =
    RainSensor_Init(&hadc1);


/* =========================================================
   CHECK ALL SENSORS
   ========================================================= */

if ((bmp388_ok == 0) ||
    (sht30_ok == 0) ||
    (bh1750_ok == 0) ||
    (rain_ok == 0))
{
    OLED_Fill(0xAA);
}
else
{
    OLED_SetCursor(0, 0);
    OLED_WriteString("T: ");

    OLED_SetCursor(0, 1);
    OLED_WriteString("H: ");

    OLED_SetCursor(0, 2);
    OLED_WriteString("P: ");

    OLED_SetCursor(0, 3);
    OLED_WriteString("L: ");

    OLED_SetCursor(0, 4);
    OLED_WriteString("R: ");
}

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

while (1)
{
    if (bmp388_ok &&
        sht30_ok &&
        bh1750_ok &&
        rain_ok)
    {
        uint8_t bmp_read_ok;
        uint8_t sht_read_ok;
        uint8_t bh1750_read_ok;
        uint8_t rain_read_ok;


        /* =============================================
           READ BMP388
           ============================================= */

        bmp_read_ok =
            BMP388_Read(
                &bmp_temperature_c,
                &bmp_pressure_hpa
            );


        /* =============================================
           READ SHT30
           ============================================= */

        sht_read_ok =
            SHT30_Read(
                &sht30_temperature_c,
                &sht30_humidity_rh
            );


        /* =============================================
           READ BH1750
           ============================================= */

        bh1750_read_ok =
            BH1750_ReadLux(
                &bh1750_lux
            );


        /* =============================================
           READ RAIN SENSOR
           ============================================= */

        rain_read_ok =
            RainSensor_ReadRaw(
                &rain_raw
            );


        if (bmp_read_ok &&
            sht_read_ok &&
            bh1750_read_ok &&
            rain_read_ok)
        {
            int32_t temperature_x100;

            uint32_t humidity_x100;

            uint32_t pressure_x100;

            uint32_t lux_integer;


            /* =========================================
               CONVERT SENSOR VALUES
               ========================================= */

            temperature_x100 =
                (int32_t)(
                    sht30_temperature_c *
                    100.0f
                );


            humidity_x100 =
                (uint32_t)(
                    sht30_humidity_rh *
                    100.0f
                );


            pressure_x100 =
                (uint32_t)(
                    bmp_pressure_hpa *
                    100.0f
                );


            lux_integer =
                (uint32_t)(
                    bh1750_lux
                );


            /* =========================================
               RAIN CLASSIFICATION
               ========================================= */

            if (rain_raw >=
                RAIN_DRY_THRESHOLD)
            {
                rain_state =
                    RAIN_STATE_DRY;
            }

            else if (
                rain_raw >=
                RAIN_HEAVY_THRESHOLD
            )
            {
                rain_state =
                    RAIN_STATE_LOW;
            }

            else
            {
                rain_state =
                    RAIN_STATE_HEAVY;
            }


            /* =========================================
               BUZZER

               Active LOW
               ========================================= */

            if (rain_state ==
                RAIN_STATE_DRY)
            {
                HAL_GPIO_WritePin(
                    GPIOB,
                    GPIO_PIN_0,
                    GPIO_PIN_SET
                );
            }

            else if (
                rain_state ==
                RAIN_STATE_LOW
            )
            {
                HAL_GPIO_WritePin(
                    GPIOB,
                    GPIO_PIN_0,
                    GPIO_PIN_RESET
                );


                HAL_Delay(150);


                HAL_GPIO_WritePin(
                    GPIOB,
                    GPIO_PIN_0,
                    GPIO_PIN_SET
                );
            }

            else
            {
                HAL_GPIO_WritePin(
                    GPIOB,
                    GPIO_PIN_0,
                    GPIO_PIN_RESET
                );
            }


            /* =========================================
               UPDATE WeatherData_t
               ========================================= */

            weather_data.temperature_x100 =
                (int16_t)(
                    temperature_x100
                );


            weather_data.humidity_x100 =
                (uint16_t)(
                    humidity_x100
                );


            weather_data.pressure_x100 =
                pressure_x100;


            weather_data.light_lux =
                (uint16_t)(
                    lux_integer
                );


            weather_data.rain_raw =
                rain_raw;


            weather_data.rain_state =
                rain_state;


            /* =========================================
               SEND STM32 -> ESP32
               ========================================= */

            WeatherUART_Send(
                &huart1,
                &weather_data
            );


            /* =========================================
               TEMPERATURE
               ========================================= */

            OLED_SetCursor(
                18,
                0
            );


            OLED_WriteString(
                "                 "
            );


            OLED_SetCursor(
                18,
                0
            );


            OLED_WriteSignedFixed2(
                temperature_x100
            );


            OLED_WriteString(
                " C"
            );


            /* =========================================
               HUMIDITY
               ========================================= */

            OLED_SetCursor(
                18,
                1
            );


            OLED_WriteString(
                "                 "
            );


            OLED_SetCursor(
                18,
                1
            );


            OLED_WriteUnsignedFixed2(
                humidity_x100
            );


            OLED_WriteString(
                " %"
            );


            /* =========================================
               PRESSURE
               ========================================= */

            OLED_SetCursor(
                18,
                2
            );


            OLED_WriteString(
                "                 "
            );


            OLED_SetCursor(
                18,
                2
            );


            OLED_WriteUnsignedFixed2(
                pressure_x100
            );


            OLED_WriteString(
                " hPa"
            );


            /* =========================================
               LIGHT
               ========================================= */

            OLED_SetCursor(
                18,
                3
            );


            OLED_WriteString(
                "                 "
            );


            OLED_SetCursor(
                18,
                3
            );


            OLED_WriteUnsigned(
                lux_integer
            );


            OLED_WriteString(
                " lux"
            );


            /* =========================================
               RAIN
               ========================================= */

            OLED_SetCursor(
                18,
                4
            );


            OLED_WriteString(
                "                 "
            );


            OLED_SetCursor(
                18,
                4
            );


            OLED_WriteUnsigned(
                rain_raw
            );


            OLED_WriteChar(
                ' '
            );


            if (rain_state ==
                RAIN_STATE_DRY)
            {
                OLED_WriteString(
                    "DRY"
                );
            }

            else if (
                rain_state ==
                RAIN_STATE_LOW
            )
            {
                OLED_WriteString(
                    "LOW"
                );
            }

            else
            {
                OLED_WriteString(
                    "HEAVY"
                );
            }
        }

        else
        {
            /*
             * Sensor error:
             * tat buzzer.
             */

            HAL_GPIO_WritePin(
                GPIOB,
                GPIO_PIN_0,
                GPIO_PIN_SET
            );
        }
    }


    /* =============================================
       HEARTBEAT
       ============================================= */

    HAL_GPIO_TogglePin(
        GPIOC,
        GPIO_PIN_13
    );


    HAL_Delay(
        1000
    );


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
}

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


/* =========================================================
   OLED CURSOR
   ========================================================= */

void OLED_SetCursor(uint8_t column, uint8_t page)
{
    OLED_Command(0xB0 + page);

    OLED_Command(
        0x00 | (column & 0x0F)
    );

    OLED_Command(
        0x10 | ((column >> 4) & 0x0F)
    );
}


/* =========================================================
   MINI FONT 5x7
   Chi can cac ky tu dang dung cho BMP388
   ========================================================= */

void OLED_GetGlyph(char c, uint8_t glyph[5])
{
    uint8_t i;

    for (i = 0; i < 5; i++)
    {
        glyph[i] = 0x00;
    }


    switch (c)
    {
        case '0':
        {
            uint8_t g[5] =
            {0x3E, 0x51, 0x49, 0x45, 0x3E};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '1':
        {
            uint8_t g[5] =
            {0x00, 0x42, 0x7F, 0x40, 0x00};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '2':
        {
            uint8_t g[5] =
            {0x42, 0x61, 0x51, 0x49, 0x46};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '3':
        {
            uint8_t g[5] =
            {0x21, 0x41, 0x45, 0x4B, 0x31};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '4':
        {
            uint8_t g[5] =
            {0x18, 0x14, 0x12, 0x7F, 0x10};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '5':
        {
            uint8_t g[5] =
            {0x27, 0x45, 0x45, 0x45, 0x39};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '6':
        {
            uint8_t g[5] =
            {0x3C, 0x4A, 0x49, 0x49, 0x30};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '7':
        {
            uint8_t g[5] =
            {0x01, 0x71, 0x09, 0x05, 0x03};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '8':
        {
            uint8_t g[5] =
            {0x36, 0x49, 0x49, 0x49, 0x36};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '9':
        {
            uint8_t g[5] =
            {0x06, 0x49, 0x49, 0x29, 0x1E};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'A':
        {
            uint8_t g[5] =
            {0x7E, 0x11, 0x11, 0x11, 0x7E};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'C':
        {
            uint8_t g[5] =
            {0x3E, 0x41, 0x41, 0x41, 0x22};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'D':
        {
            uint8_t g[5] =
            {0x7F, 0x41, 0x41, 0x22, 0x1C};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'E':
        {
            uint8_t g[5] =
            {0x7F, 0x49, 0x49, 0x49, 0x41};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'H':
        {
            uint8_t g[5] =
            {0x7F, 0x08, 0x08, 0x08, 0x7F};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'L':
        {
            uint8_t g[5] =
            {0x7F, 0x40, 0x40, 0x40, 0x40};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'O':
        {
            uint8_t g[5] =
            {0x3E, 0x41, 0x41, 0x41, 0x3E};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'P':
        {
            uint8_t g[5] =
            {0x7F, 0x09, 0x09, 0x09, 0x06};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'R':
        {
            uint8_t g[5] =
            {0x7F, 0x09, 0x19, 0x29, 0x46};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'T':
        {
            uint8_t g[5] =
            {0x01, 0x01, 0x7F, 0x01, 0x01};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'V':
        {
            uint8_t g[5] =
            {0x1F, 0x20, 0x40, 0x20, 0x1F};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'W':
        {
            uint8_t g[5] =
            {0x3F, 0x40, 0x38, 0x40, 0x3F};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'Y':
        {
            uint8_t g[5] =
            {0x03, 0x04, 0x78, 0x04, 0x03};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'h':
        {
            uint8_t g[5] =
            {0x7F, 0x08, 0x04, 0x04, 0x78};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'a':
        {
            uint8_t g[5] =
            {0x20, 0x54, 0x54, 0x54, 0x78};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'l':
        {
            uint8_t g[5] =
            {0x00, 0x41, 0x7F, 0x40, 0x00};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'u':
        {
            uint8_t g[5] =
            {0x3C, 0x40, 0x40, 0x20, 0x7C};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case 'x':
        {
            uint8_t g[5] =
            {0x44, 0x28, 0x10, 0x28, 0x44};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '%':
        {
            uint8_t g[5] =
            {0x63, 0x13, 0x08, 0x64, 0x63};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case ':':
        {
            uint8_t g[5] =
            {0x00, 0x36, 0x36, 0x00, 0x00};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '.':
        {
            uint8_t g[5] =
            {0x00, 0x60, 0x60, 0x00, 0x00};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case '-':
        {
            uint8_t g[5] =
            {0x08, 0x08, 0x08, 0x08, 0x08};

            for (i = 0; i < 5; i++)
                glyph[i] = g[i];

            break;
        }


        case ' ':
        default:
            break;
    }
}

/* =========================================================
   WRITE CHARACTER
   ========================================================= */

void OLED_WriteChar(char c)
{
    uint8_t glyph[5];

    OLED_GetGlyph(c, glyph);

    for (uint8_t i = 0; i < 5; i++)
    {
        OLED_Data(glyph[i]);
    }

    OLED_Data(0x00);
}


/* =========================================================
   WRITE STRING
   ========================================================= */

void OLED_WriteString(const char *str)
{
    while (*str)
    {
        OLED_WriteChar(*str);

        str++;
    }
}


/* =========================================================
   WRITE UNSIGNED INTEGER
   ========================================================= */

void OLED_WriteUnsigned(uint32_t value)
{
    char buffer[11];

    uint8_t index = 0;


    if (value == 0)
    {
        OLED_WriteChar('0');

        return;
    }


    while (value > 0)
    {
        buffer[index++] =
            '0' + (value % 10);

        value /= 10;
    }


    while (index > 0)
    {
        OLED_WriteChar(
            buffer[--index]
        );
    }
}


/* =========================================================
   WRITE SIGNED NUMBER WITH 2 DECIMALS

   Example:
   2835 -> 28.35
   ========================================================= */

void OLED_WriteSignedFixed2(int32_t value)
{
    uint32_t abs_value;

    uint32_t integer_part;

    uint32_t decimal_part;


    if (value < 0)
    {
        OLED_WriteChar('-');

        abs_value =
            (uint32_t)(-value);
    }
    else
    {
        abs_value =
            (uint32_t)value;
    }


    integer_part =
        abs_value / 100;

    decimal_part =
        abs_value % 100;


    OLED_WriteUnsigned(integer_part);

    OLED_WriteChar('.');


    OLED_WriteChar(
        '0' + (decimal_part / 10)
    );

    OLED_WriteChar(
        '0' + (decimal_part % 10)
    );
}


/* =========================================================
   WRITE UNSIGNED NUMBER WITH 2 DECIMALS
   ========================================================= */

void OLED_WriteUnsignedFixed2(uint32_t value)
{
    uint32_t integer_part;

    uint32_t decimal_part;


    integer_part =
        value / 100;

    decimal_part =
        value % 100;


    OLED_WriteUnsigned(integer_part);

    OLED_WriteChar('.');


    OLED_WriteChar(
        '0' + (decimal_part / 10)
    );

    OLED_WriteChar(
        '0' + (decimal_part % 10)
    );
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

    __disable_irq();

    while (1)
    {
    }

  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
