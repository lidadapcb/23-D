/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "myFFT.h"
#include "2023Diansai_DSP.h"
#include <stdio.h>
#include "ad9959.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t adc_cplt_flag = 0; // ADC/DMA采集完成标志
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  // 1. 初始化FFT和DSP参数 (500k采样率，16384点，3.3V量程)
  myFFT_init();
  
  // 2. 启动触发ADC采样的定时器(根据您的工程环境为TIM3)
  HAL_TIM_Base_Start(&htim3);
	ad9959_init();
	ad9959_write_frequency(AD9959_CHANNEL_0, 1900000);
	ad9959_write_amplitude(AD9959_CHANNEL_0, 1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 3. 启动一轮ADC DMA采集 (采集16384点)
    ADC_Update(1);
    
    // 4. 阻塞等待采集完成 (等待DMA中断回调中置位标志)
    while(adc_cplt_flag == 0)
    {
      // 可以在此执行其他非阻塞任务
    }
    adc_cplt_flag = 0; // 清除标志，准备下一轮
    
    // 采集完一组数据后先停止DMA，防止在处理FFT时DMA覆盖底层数据
    HAL_ADC_Stop_DMA(&hadc1); 
    
    // 5. 执行FFT转换与特征提取 (k为校准系数 3.3V/4096)
    FFT_convert(ADC1_Val, ADC1_FFT, 8192, 1, 3.3f / 4096.0f);
    
    FFT_Accomplish_TAG = 1; // 标记 FFT 完成，放行调制识别算法
    
    // 6. 进行调制方式识别与参数估计
    u8 mod_type = Modulation_Judge();
    
    FFT_Accomplish_TAG = 0; // 重置标志位
    
    // 7. 根据题目要求通过串口打印结果
    if(mod_type != _ERR_ && mod_type != 0) {
      printf("\r\n=================================\r\n");
    }
    switch(mod_type)
    {
      case _CW_:
        printf("调制方式: CW (连续载波)\r\n");
        break;
      case _AM_:
        printf("调制方式: AM\r\n");
        printf("调制频率 F: %.1f Hz\r\n", Modem_ARR[_AM_].parameter2);
        printf("调幅系数 ma: %.3f\r\n", Modem_ARR[_AM_].parameter1);
        break;
      case _FM_:
        printf("调制方式: FM\r\n");
        printf("调制频率 F: %.1f Hz\r\n", Modem_ARR[_FM_].parameter2);
        printf("最大频偏 df_max: %.1f Hz\r\n", Modem_ARR[_FM_].parameter3);
        printf("调频系数 mf: %.3f\r\n", Modem_ARR[_FM_].parameter1);
        break;
      case _2ASK_:
        printf("调制方式: 2ASK\r\n");
        printf("码元速率 Rc: %.1f bps\r\n", Modem_ARR[_2ASK_].parameter2);
        break;
      case _2FSK_:
        printf("调制方式: 2FSK\r\n");
        printf("码元速率 Rc: %.1f bps\r\n", Modem_ARR[_2FSK_].parameter2);
        printf("移频键控系数 h: %.3f\r\n", Modem_ARR[_2FSK_].parameter1);
        break;
      case _2PSK_:
        printf("调制方式: 2PSK\r\n");
        printf("码元速率 Rc: %.1f bps\r\n", Modem_ARR[_2PSK_].parameter2);
        break;
      default:
        // 取消频繁打印“识别中”，改为打印一个点或完全略过，减少负担
        printf(".");
        break;
    }
    if(mod_type != _ERR_ && mod_type != 0) {
      printf("=================================\r\n");
    }
    
    // 8. 延时 (此处延时500ms防止串口刷屏过快)
    HAL_Delay(500); 
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// DMA传输完成中断回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  // 检查是否是ADC1的DMA传输完成
  if(hadc->Instance == ADC1)
  {
    adc_cplt_flag = 1; // 置位采集完成标志
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
