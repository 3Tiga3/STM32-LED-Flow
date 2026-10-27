/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 三个 LED 流水灯（非阻塞延时实现）
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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ---- 流水灯参数：改这里就能改速度 / 灯的数量 ---- */
#define LED_FLOW_INTERVAL_MS   500U   /* 每 500 ms 切换一次 */
#define LED_FLOW_COUNT         3U     /* 一共 3 个灯 */

/* LED 亮灭电平：本工程 LED 阳极接引脚、阴极经电阻接地，输出高电平点亮。
   如果你的 LED 是 引脚 -> 电阻 -> LED -> GND 接法，保持下面不变；
   若是 LED 阳极接 3V3、阴极接引脚（灌电流点亮），把这两个宏对调即可。 */
#define LED_ON                 GPIO_PIN_SET
#define LED_OFF                GPIO_PIN_RESET

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* 逻辑序号 -> 实际引脚 的映射表。
   直接使用 CubeMX 依据 User Label 生成的宏（LED0_Pin / LED0_GPIO_Port ...），
   以后在 CubeMX 里换引脚，这里一行都不用改。 */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t      pin;
} LedPin_t;

static const LedPin_t s_ledTable[LED_FLOW_COUNT] =
{
  { LED0_GPIO_Port, LED0_Pin },
  { LED1_GPIO_Port, LED1_Pin },
  { LED2_GPIO_Port, LED2_Pin },
};

static uint32_t s_tickBase = 0U;   /* 上一次切换的时间戳（单位 ms） */
static uint32_t s_ledIndex = 0U;   /* 当前点亮的灯在 s_ledTable 中的下标 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

static void LedFlow_Init(void);
static void LedFlow_Poll(uint32_t nowMs);

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
  /* USER CODE BEGIN 2 */

  /* 初始化流水灯：全灭 -> 点亮第 0 个 -> 记下起始时间 */
  LedFlow_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 非阻塞核心：每轮循环只做一次「时间够了没有」的判断，
       没到点就立刻返回，CPU 不会被卡在这里（对比 HAL_Delay 会原地死等）。 */
    LedFlow_Poll(HAL_GetTick());

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
}

/* USER CODE BEGIN 4 */

/**
  * @brief  流水灯初始化：全部熄灭，点亮第 0 个灯，记录起始时间戳。
  * @retval None
  */
static void LedFlow_Init(void)
{
  uint32_t i;

  for (i = 0U; i < LED_FLOW_COUNT; i++)
  {
    HAL_GPIO_WritePin(s_ledTable[i].port, s_ledTable[i].pin, LED_OFF);
  }

  s_ledIndex = 0U;
  HAL_GPIO_WritePin(s_ledTable[0].port, s_ledTable[0].pin, LED_ON);

  s_tickBase = HAL_GetTick();
}

/**
  * @brief  非阻塞的流水灯节拍函数：时间到了才切换，没到立刻返回。
  * @param  nowMs 当前系统毫秒数（由 HAL_GetTick() 提供，SysTick 中断累加）
  * @retval None
  */
static void LedFlow_Poll(uint32_t nowMs)
{
  /* 用「无符号减法」算时间差：
     即使 uwTick 溢出回绕（约 49.7 天一次），相减结果依然正确，
     所以不需要额外处理溢出。 */
  if ((uint32_t)(nowMs - s_tickBase) < LED_FLOW_INTERVAL_MS)
  {
    return;   /* 还没到 500 ms —— 直接返回，主循环继续跑，不占用等待时间 */
  }

  /* 时间到：把基准时间推进一个周期（+= 而不是 = now，避免长时间运行后累积误差） */
  s_tickBase += LED_FLOW_INTERVAL_MS;

  /* 关掉当前灯，序号跳到下一个，点亮新灯 —— 形成"流水"效果 */
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_OFF);
  s_ledIndex = (s_ledIndex + 1U) % LED_FLOW_COUNT;
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_ON);
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
