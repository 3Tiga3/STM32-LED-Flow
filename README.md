# 电控组第二次培训作业

- **刘祥博**
- ***20263008899***

## 一、任务说明

使用 CLion + STM32CubeMX 完成三个 LED 的流水灯控制：

| 项目 | 内容 |
| --- | --- |
| 主控芯片 | STM32F103C8T6（LQFP48，最小系统板） |
| 开发工具 | STM32CubeMX 6.15.0 + CLion 2026.2.2 |
| 工具链 | STM32CubeIDE 工程（CMake）+ arm-none-eabi-gcc 10.3.1 |
| 系统时钟 | HSE 8 MHz × PLL9 = 72 MHz |
| 流水灯间隔 | 500 ms |
| 延时方式 | **非阻塞**：`HAL_GetTick()` 时间戳轮询（完成加分项） |

## 二、引脚分配

在 CubeMX 中配置三个 GPIO 为推挽输出（Output Push Pull / No Pull / Low Speed），并用 **User Label** 分别命名为 `LED0`、`LED1`、`LED2`：

| User Label | 引脚 | 配置 |
| --- | --- | --- |
| `LED0` | PB12 | GPIO_Output |
| `LED1` | PB13 | GPIO_Output |
| `LED2` | PB14 | GPIO_Output |

CubeMX 依据 User Label 自动在 `Core/Inc/main.h` 中生成宏，代码里直接调用即可，不出现任何裸引脚号：

```c
#define LED0_Pin GPIO_PIN_12
#define LED0_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_13
#define LED1_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_14
#define LED2_GPIO_Port GPIOB
```

接线方式（共地）：`PB12/PB13/PB14` → 各串联 1 kΩ 限流电阻 → LED 阳极，LED 阴极接 GND。输出高电平点亮。

## 三、流水灯实现思路

### 3.1 为什么不用 HAL_Delay()

```c
while (1)
{
    HAL_GPIO_WritePin(GPIOB, LED0_Pin, GPIO_PIN_SET);
    HAL_Delay(500);   /* 死等 500 ms */
}
```

`HAL_Delay()` 的本质是**忙等待**：它在一个循环里反复查 SysTick 计数，直到时间到才返回。这 500 ms 内 CPU 被完全占据，无法响应按键、串口、传感器等其他任务。三个灯轮流点就要连续阻塞 1.5 s 一个周期，一旦后续要加电机控制或遥控接收，整个系统的响应会被彻底拖死。所以本次采用非阻塞写法。

### 3.2 非阻塞实现原理：HAL_GetTick() 时间戳轮询

**原理链条：**

1. `HAL_Init()` 里初始化 SysTick 定时器，配置为每 **1 ms** 产生一次中断；
2. 每次 SysTick 中断进入 `SysTick_Handler()` → 调用 `HAL_IncTick()`，把全局变量 `uwTick` 加 1；
3. `HAL_GetTick()` 只是把这个 `uwTick` 读出来返回，**不会阻塞**；
4. 因此 `HAL_GetTick()` 相当于一块免费的“秒表”，告诉我们系统上电后运行了多少毫秒；
5. 主循环每轮只做一次比较：`当前时刻 - 上次切换时刻 >= 500` 吗？
   - 不够 → 立刻 `return`，主循环进入下一轮，CPU 空闲去干别的事；
   - 够了 → 切换 LED，并把“上次切换时刻”推进 500 ms。

**关键点：把「等待」拆成了「每次只问一句时间够不够」，而不是「原地站 500 ms」。**

状态机一圈：

```
t=0ms      LED0 亮
t=500ms    LED0 灭 → LED1 亮
t=1000ms   LED1 灭 → LED2 亮
t=1500ms   LED2 灭 → LED0 亮（回到起点，s_ledIndex 用 % 3 回绕）
```

**核心代码片段**（`Core/Src/main.c`）：

```c
#define LED_FLOW_INTERVAL_MS   500U   /* 每 500 ms 切换一次 */
#define LED_FLOW_COUNT         3U     /* 一共 3 个灯 */

/* 逻辑序号 -> 实际引脚 的映射表，全部使用 CubeMX 生成的宏 */
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

static uint32_t s_tickBase = 0U;   /* 上一次切换的时间戳（ms） */
static uint32_t s_ledIndex = 0U;   /* 当前点亮的灯序号 */

/**
  * @brief  非阻塞的流水灯节拍函数：时间到了才切换，没到立刻返回。
  */
static void LedFlow_Poll(uint32_t nowMs)
{
  /* 无符号减法算时间差：uwTick 溢出回绕（约 49.7 天）时结果依然正确 */
  if ((uint32_t)(nowMs - s_tickBase) < LED_FLOW_INTERVAL_MS)
  {
    return;   /* 还没到 500 ms —— 立即返回，CPU 不被占用 */
  }

  /* 时间到：基准时间推进一个周期（+= 而不是 = now，避免累积误差） */
  s_tickBase += LED_FLOW_INTERVAL_MS;

  /* 关掉当前灯，序号跳到下一个，点亮新灯 —— 形成"流水"效果 */
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_OFF);
  s_ledIndex = (s_ledIndex + 1U) % LED_FLOW_COUNT;
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_ON);
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  LedFlow_Init();          /* 全灭 -> 点亮第 0 个 -> 记录起始时间戳 */

  while (1)
  {
    LedFlow_Poll(HAL_GetTick());   /* 主循环只做一次时间比较 */
  }
}
```

**两个细节说明：**

- **为什么用 `(uint32_t)(nowMs - s_tickBase)` 而不是 `nowMs >= s_tickBase + 500`**：`uwTick` 是 32 位无符号数，约 49.7 天会溢出回绕。无符号减法在回绕时依然得到正确的时间差，而直接比较大小会在回绕瞬间失效。
- **为什么用 `s_tickBase += 500` 而不是 `s_tickBase = nowMs`**：`+=` 让节拍基准严格按 500 ms 递增，不会因为轮询的微小延迟逐次累积，长时间运行不会越来越慢。

### 3.3 其他可行的非阻塞方案（对比）

| 方案 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| **本工程：时间戳轮询** | 主循环比较 `HAL_GetTick()` 差值 | 实现简单、无中断风险、易与其他任务共存 | 精度取决于主循环周期（本工程远小于 1 ms，无影响） |
| 定时器中断 | TIM2 配 500 ms 更新中断，在 `HAL_TIM_PeriodElapsedCallback` 里切换 LED | 时序最准，CPU 完全不参与 | 逻辑跑在中断里，复杂业务容易引发竞态 |
| 硬件 PWM + DMA | 定时器 PWM 输出 + DMA 搬运占空比序列 | 硬件自动换相，CPU 占用为 0 | 配置复杂，只适合固定花样 |

本工程选第一种：既满足“摆脱 `HAL_Delay()` 死等”的要求，又保留了主循环给后续任务（串口、按键、电机 PID）使用的余地。

## 四、工程文件说明

| 路径 | 说明 |
| --- | --- |
| `STM32F103C8T6_LED_Flow.ioc` | CubeMX 工程文件，含 LED0/LED1/LED2 的 User Label |
| `Core/Src/main.c` | 主逻辑：时钟配置 + 非阻塞流水灯状态机 |
| `Core/Src/gpio.c` | `MX_GPIO_Init()`：三个 LED 引脚初始化 |
| `Core/Inc/main.h` | CubeMX 生成的 LED0/LED1/LED2 引脚宏 |
| `CMakeLists.txt` | CubeMX 为 STM32CubeIDE(CMake) 工具链生成的构建脚本 |
| `Drivers/` | STM32F1xx HAL 库与 CMSIS |

## 五、编译结果

CLion 中 Build 通过，链接输出：

```
Memory region         Used Size  Region Size  %age Used
             RAM:        2664 B        20 KB     13.01%
           FLASH:        5028 B        64 KB      7.67%
```

## 六、验收对照

- [x] CubeMX 引脚配置中包含 `LED0`、`LED1`、`LED2` 三个 User Label（见 `.ioc` 文件 `PB12/13/14.GPIO_Label`）
- [x] 代码中直接调用 CubeMX 生成的宏（`LED0_Pin` / `LED0_GPIO_Port` …），无裸引脚号
- [x] 三个 LED 间隔 500 ms 依次点亮，形成流水效果
- [x] 完成加分项：使用 `HAL_GetTick()` 非阻塞延时，全程未使用 `HAL_Delay()`
