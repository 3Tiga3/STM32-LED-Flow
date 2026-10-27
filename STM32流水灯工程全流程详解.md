# STM32F103 流水灯工程 · 全流程详解

> 配套《2026.10.24 电控组第二次招新培训作业》
> 对照工程：`D:\_学习\RM电控\STM32F103C8T6_LED_Flow`
> 读者定位：已完成第一次作业（能用 CubeMX 生成空工程、CLion 能编译烧录），现在要**自己从零配一遍引脚、自己写逻辑**的人。
> 全文严格按第一次培训文档的顺序推进：**选芯片 → 基础配置 → 工程选项 → 生成代码 → CLion 导入 → 改 .ld → 改代码 → 编译 → 烧录**。每一步都回答三个问题：**点哪里 / 为什么 / 不做会怎样**。

---

## 目录

- [1. 先建立全局认知：五个工具各干什么](#1-先建立全局认知五个工具各干什么)
- [2. 第 0 步 环境确认（4 个路径 + 2 个变量）](#2-第-0-步-环境确认4-个路径--2-个变量)
- [3. 第 1 步 CubeMX 新建工程与芯片选型](#3-第-1-步-cubemx-新建工程与芯片选型)
- [4. 第 2 步 基础三件套：RCC / SYS / Clock](#4-第-2-步-基础三件套rcc--sys--clock)
- [5. 第 3 步【作业重点】配置三个 LED 引脚与 User Label](#5-第-3-步作业重点配置三个-led-引脚与-user-label)
- [6. 第 4 步 Project Manager：决定 CLion 能不能打开](#6-第-4-步-project-manager决定-clion-能不能打开)
- [7. 第 5 步 生成后目录解剖与 USER CODE 机制](#7-第-5-步-生成后目录解剖与-user-code-机制)
- [8. 第 6 步 导入 CLion 2026.2.2](#8-第-6-步-导入-clion-202622)
- [9. 第 7 步 必改：链接脚本里的 READONLY](#9-第-7-步-必改链接脚本里的-readonly)
- [10. 第 8 步 改代码：三处、为什么、改哪](#10-第-8-步-改代码三处为什么改哪)
- [11. 第 9 步 编译与读输出](#11-第-9-步-编译与读输出)
- [12. 第 10 步 烧录、验收录像与接线](#12-第-10-步-烧录验收录像与接线)
- [附录 A 常见坑速查表](#附录-a-常见坑速查表)
- [附录 B 术语表](#附录-b-术语表)
- [附录 C 验收自检清单](#附录-c-验收自检清单)
- [附录 D 本工程实际参数速查](#附录-d-本工程实际参数速查)

---

## 1. 先建立全局认知：五个工具各干什么

新手最容易懵的不是"点哪个按钮"，而是**不知道这些软件之间是什么关系**。先看一张表：

| 工具 / 文件 | 角色 | 你直接操作它吗 | 它出错时的典型表现 |
| --- | --- | --- | --- |
| STM32CubeMX | 图形化配置 + **代码生成器** | 是（配引脚、时钟、选项） | 引脚/时钟配错 → 代码没错但板子不动 |
| `.ioc` 文件 | 配置的**唯一真相源**（纯文本 INI） | 间接（双击用 CubeMX 打开） | 手改 `.ioc` 与生成代码不一致 |
| `CMakeLists.txt` + CMake | **构建描述**：编译哪些文件、用什么参数 | 极少手改 | CLion 报 CMake 配置错误，红条一片 |
| arm-none-eabi-gcc | 真正的**编译器 + 链接器**（ARM 版 gcc） | 不直接调用 | 语法错、`undefined reference` |
| OpenOCD + ST-LINK | 把 `.elf` **写进芯片 Flash** | 通过运行配置 | `target not found`、烧录失败 |

一条数据流看懂它们如何接力：

```
      你在 CubeMX 里点鼠标（选芯片、点引脚、拉时钟）
                     │ 保存
                     ▼
            STM32F103C8T6_LED_Flow.ioc        ← 设计图
                     │ GENERATE CODE
                     ▼
     Core/Src/*.c  Core/Inc/*.h  Drivers/ ...  ← 成品代码
                     │ 被 CMakeLists.txt 描述
                     ▼
        CMake 生成构建文件 → arm-none-eabi-gcc 编译 + 链接
                     │
                     ▼
     build/debug/xxx.elf（机器码 + 每段放在哪个地址）
                     │
                     ▼
     OpenOCD 通过 ST-LINK 的 SWD 写入 Flash → 复位 → 芯片运行
```

一句话记住：**`.ioc` 是设计图，CubeMX 是施工队，生成的 C 代码是成品，CMake 是施工指令，GCC 是工人，OpenOCD 是搬家师傅。**

所以顺序永远是：**改配置只动 CubeMX，写逻辑只动 C 代码，两者不要互相越界**。越界的代价在第 7 节讲（USER CODE 机制）。

---

## 2. 第 0 步 环境确认（4 个路径 + 2 个变量）

第一次培训的《南海鲨开发环境配置文档》和《Clion配置.pdf》已经把这些装好了。开工前先确认 4 个路径存在，否则后面每一步都会卡：

| 组件 | 本机实际路径 | 用途 |
| --- | --- | --- |
| STM32CubeMX | `D:\CubeMX\STM32CubeMX.exe` | 配置 + 生成代码（本机 6.15.0） |
| CLion | `D:\CLion\CLion 2026.2.2` | 编辑器 + 构建 + 烧录入口 |
| ARM 工具链 | `D:\CLion\CLion 2026.2.2\arm\bin\arm-none-eabi-gcc.exe` | 编译（本机 10.3.1，**这个版本号后面第 9 节会用到**） |
| OpenOCD | `D:\OpenOCD\bin\openocd.exe` | 烧录 |
| 板级配置 | `D:\_学习\RM电控\QQ群文件-电控组\enviroment\CFJ_STM32\stm32f1.cfg` | 告诉 OpenOCD "这颗是 STM32F1" |

两个环境变量（培训文档要求配的）：

- `JAVA_HOME` → JDK 目录；`Path` 里要有 `%JAVA_HOME%\bin`
- `CLion` → `D:\CLion\CLion 2026.2.2`；`Path` 里要有 `%CLion%`、`%CLion%\bin\mingw\bin`、`%CLion%\arm\bin`、`D:\OpenOCD\bin`

验证（PowerShell）：

```powershell
arm-none-eabi-gcc --version     # 应打印 10.3.1 20210824
openocd --version               # 应打印 OpenOCD 版本
& 'D:\CubeMX\STM32CubeMX.exe'   # 能起来就说明 Java 环境 OK
```

> 固件包位置：`C:\Users\LXB\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.7`。CubeMX 生成 F1 工程时就是从这里拷 HAL 库，**如果没有这个目录，生成会失败**。

---

## 3. 第 1 步 CubeMX 新建工程与芯片选型

### 3.1 为什么必须是 STM32F103C8T6

先学会读型号（这是电控组的入门常识）：

| STM32 | F | 103 | C | 8 | T | 6 |
| --- | --- | --- | --- | --- | --- | --- |
| 厂商 | 基础型 F 系列 | 增强型 103 子系列 | 引脚数 **C = 48 脚** | Flash **8 = 64 KB** | 封装 **T = LQFP** | 温度等级 **6 = -40~85℃** |

C8T6 的三个关键参数：**Cortex-M3 内核、72 MHz 主频、64 KB Flash + 20 KB RAM**。班上发的蓝色最小系统板（"蓝pill"）就是这颗，所以必须选它——**你手上有哪块板，就配哪颗芯片，一个字母都不能错**。

选错的后果（很难自查）：启动文件、链接脚本里的 Flash/RAM 大小、外设寄存器数量都会跟着变。比如误选成 `STM32F103RCT6`（256 KB Flash / 48 KB RAM），链接脚本会写"RAM 48K"，程序可能编译通过、一上电就 HardFault 死机，或者烧进 64KB 的芯片时直接报 `Flash download failed`。

### 3.2 操作步骤

1. 打开 `D:\CubeMX\STM32CubeMX.exe`。
2. 主界面点 **`ACCESS TO MCU SELECTOR`**（也可以 `File → New Project`），进入芯片选择器。
3. 左上角搜索框输入 **`STM32F103C8T6`**，等待列表刷新。
4. 看右侧列表的 **Package** 列，选 **LQFP48**（C8T6 只有这一种封装，但列表里有时会同时出现 C8T6 / C8T6TR，两个都对，TR 是编带包装）。
5. 双击该行，或选中后点右上角 **`Start Project`**。

### 3.3 首次下载固件包（这一步会卡住很多人）

第一次生成 F1 工程时，CubeMX 会弹窗要求下载 **STM32Cube MCU Package for STM32F1（STM32Cube FW_F1 V1.8.7，约 200 MB）**。

- 培训文档里写明：**首次要联网下载，网络不好时需要挂梯子**。
- 本机已经装过（在 `C:\Users\LXB\STM32Cube\Repository`），所以现在弹下载框可以直接点 **`Cancel`**，CubeMX 会使用本地已安装版本。
- 想确认版本：`Help → Manage embedded software packages`，看 `STM32F1` 那一行是否显示 `1.8.7` 且已勾选。

> 之后的每次生成都不需要联网，除非你换了芯片系列（比如以后做 F4 的板子，要再下一次 F4 的包）。

---

## 4. 第 2 步 基础三件套：RCC / SYS / Clock

生成代码之前必须配这三项，否则要么时钟上不去，要么**烧一次就再也连不上板子**（这不是夸张，是新手最经典的翻车）。

左侧 `Categories` → `System Core` 下依次操作。

### 4.1 RCC：HSE 与 LSE 都选 Crystal/Ceramic Resonator

`System Core → RCC`：

| 项 | 选什么 | 为什么 |
| --- | --- | --- |
| High Speed Clock (HSE) | **Crystal/Ceramic Resonator** | 让芯片使用板上那颗 **8 MHz 外部晶振**作为时钟源。默认是 `Disable`，此时只能用内部 RC（HSI，8 MHz 但精度差 ±1%），而且**你根本拉不到 72 MHz** |
| Low Speed Clock (LSE) | **Crystal/Ceramic Resonator** | 板上 32.768 kHz 晶振，给 RTC 用。本题用不到，但第一次培训文档要求选上，保持一致 |

> 如果只选 HSE 不选 LSE，生成代码里不会有 `RCC_OSC32...` 相关处理，不影响流水灯——但验收时对照文档会被问"为什么这里不一样"，按文档来最省事。

### 4.2 SYS：Debug 选 Serial Wire

`System Core → SYS`：

| 项 | 选什么 | 为什么 |
| --- | --- | --- |
| Debug | **Serial Wire** | 启用 **SWD** 两线调试接口（SWDIO = PA13、SWCLK = PA14），ST-LINK 就是通过这两根线烧录和调试的 |
| Timebase Source | SysTick（默认） | HAL 的 1 ms 时基，`HAL_GetTick()` 就靠它 |

**为什么必须手动选**：CubeMX 默认 Debug = `Disable`，此时 PA13/PA14 被当成普通 GPIO。程序一旦烧进去，芯片的 SWD 引脚功能就被你的程序接管 → **ST-LINK 再也连不上**，只能靠拉高 BOOT0 进系统存储器模式抢救。所以这条是"防砖"配置，**每次建工程都必须选**。

### 4.3 Clock Configuration：把 HCLK 的 8 改成 72

点顶部 **`Clock Configuration`** 标签页，会看到一棵时钟树。找到中间偏上的 **HCLK (MHz)** 输入框：

1. 框里默认是 `8`（HSI 直出）。
2. 把它改成 **`72`**，按 **回车**。
3. CubeMX 会弹窗问是否自动求解，选 **OK/Yes**，它会自动把 PLL 等前置参数配好。

改完检查这三处（都应该自动变成这样）：

| 位置 | 值 | 含义 |
| --- | --- | --- |
| Input frequency | `8 MHz` | HSE 晶振 |
| PLL Source Mux | `HSE` | PLL 输入来自外部晶振 |
| PLLMul | `×9` | **8 × 9 = 72 MHz** |
| System Clock Mux | `PLLCLK` | 系统时钟走 PLL |
| AHB Prescaler | `/1` | HCLK = 72 MHz |
| APB1 Prescaler | `/2` | PCLK1 = **36 MHz** |
| APB2 Prescaler | `/1` | PCLK2 = 72 MHz |

**为什么 APB1 必须是 36 而不是 72**：F1 的数据手册规定 APB1 总线最高只能跑 36 MHz（挂在上面的是 TIM2~7、USART2~5、I2C1/2、SPI2、USB 等低速外设）。所以 APB1 分频器必须 `/2`。这也是为什么定时器算频率时经常要乘 2——APB1 分频不为 1 时，定时器时钟会自动 ×2。

**72 MHz 是怎么"藏"在代码里的**：生成后的 `SystemClock_Config()` 里会出现这几个关键值：

```c
RCC_OscInitStruct.HSEState = RCC_HSE_ON;                 // 打开外部晶振
RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;             // 8 × 9
RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;      // 72 MHz
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;        // 36 MHz
HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);// 2 个等待周期
```

最后的 `FLASH_LATENCY_2` 不是随便写的：Flash 读取速度跟不上 72 MHz，需要插入 2 个等待周期，否则读指令会出错（表现为"跑飞"）。CubeMX 会根据你填的主频自动算，**这也是为什么必须用 CubeMX 拉时钟而不是手写**。

---

## 5. 第 3 步【作业重点】配置三个 LED 引脚与 User Label

这一步是本次作业的**验收硬指标**：CubeMX 引脚配置里必须出现 `LED0`、`LED1`、`LED2` 三个 User Label，并且代码里要直接调用对应的宏。

### 5.1 为什么选 PB12 / PB13 / PB14

不是随便挑的，选引脚有讲究：

| 候选引脚 | 能不能用 | 说明 |
| --- | --- | --- |
| PA13 / PA14 | **绝对不行** | SWD 烧录线，占了就再也连不上（第 4.2 节说过） |
| PA11 / PA12 | 不行 | USB D- / D+ |
| PA9 / PA10 | 不建议 | USART1，串口调试/打印最常用 |
| PB6 / PB7 | 不建议 | I2C1，后面 OLED 模块要用 |
| PA0~PA7、PA15 | 能用但别占 | 后续 PWM、ADC、编码器会用到 |
| PB2 | 不建议 | BOOT1 引脚 |
| **PB12 / PB13 / PB14** | **最佳** | 普通 IO、无复用冲突、板上排针外露、**三个脚连续**便于接线和读图 |

"三个脚连续"是实用主义：接线时三根线并排走，拍照/录像时一眼能看清，调试时也不会接错。

### 5.2 配置引脚（两条路，任选）

**路径 A（推荐，直观）**：在芯片引脚图上直接点。

1. 点顶部 `Pinout & Configuration` 标签（默认就在这页）。
2. 在芯片图上找到 **PB12**（芯片图下方标注 `PB12` 的位置），**左键单击**它。
3. 弹出菜单里选 **`GPIO_Output`**（不是 `GPIO_Input`）。
4. 引脚会变成绿色，表示已配置。
5. 对 **PB13**、**PB14** 重复同样操作。

**路径 B（表格法，便于统一改参数）**：

1. 左侧 `Categories → System Core → GPIO`。
2. 右侧出现所有已配 GPIO 的表格，`Pin Name` 列会列出 `PB12`、`PB13`、`PB14`。
3. 逐个选中，在下方 `GPIO Pull-up/Pull-down` 等参数区改值（见 5.3）。

### 5.3 GPIO 参数逐项解释（这是"为什么"的核心）

选中 PB12 后，下方出现一组参数。逐个说清楚：

| 参数 | 设为 | 为什么 |
| --- | --- | --- |
| **GPIO output level** | **Low** | 初始输出低电平 = LED 灭。这样上电的瞬间不会"三个灯同时闪一下" |
| **GPIO mode** | **Output Push Pull**（推挽输出） | 推挽能主动输出高/低两种电平、有驱动能力。开漏（Open Drain）只能拉低 + 高阻，驱动 LED 要么接反要么不亮 |
| **GPIO Pull-up/Pull-down** | **No pull-up and no pull-down** | 上下拉是给**输入**用的。输出模式下不需要——引脚本身已经在主动驱动电平了 |
| **Maximum output speed** | **Low** | 2 MHz 边沿速度足够点亮 LED。高速档（50 MHz）只是让上升沿更陡，对点灯毫无帮助，反而增加电磁干扰和功耗 |
| **User Label** | **`LED0`**（PB13 填 `LED1`，PB14 填 `LED2`） | **本次作业的验收要求**，见 5.4 |

> 极性说明：本工程代码用 `GPIO_PIN_SET` 表示"灯亮"，对应接线是 **PB12 → 1kΩ 电阻 → LED 长脚(阳极) → LED 短脚(阴极) → GND**（灌不出电流的写法是反过来的）。如果你的板子是 LED 一端接 3V3、另一端接引脚，那就是"低电平点亮"，把代码里 `#define LED_ON GPIO_PIN_RESET` 反过来即可（第 10.3 节会讲这个宏的意义）。

### 5.4 User Label 到底做了什么

你填的 `LED0` 会被写进 `.ioc` 文件，像这样：

```ini
PB12.GPIO_Label=LED0
PB13.GPIO_Label=LED1
PB14.GPIO_Label=LED2
```

**生成代码时，CubeMX 会把它变成两行宏，写进 `Core/Inc/main.h`**：

```c
/* Private defines -----------------------------------------------------------*/
#define LED0_Pin GPIO_PIN_12          /* 等价于 0x1000 */
#define LED0_GPIO_Port GPIOB          /* 端口指针 */
#define LED1_Pin GPIO_PIN_13
#define LED1_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_14
#define LED2_GPIO_Port GPIOB
```

这就是验收要求"在代码中直接调用对应宏定义"的由来：**宏是由 User Label 自动产生的，代码里写 `LED0_Pin` 而不是写 `GPIO_PIN_12`，老师一看就知道配置和代码是打通的**。而且将来把 LED0 改到 PB0，只需在 CubeMX 里改一次，代码里的 `LED0_Pin` 自动跟着变，不用全局搜索替换 `12`。

> `LED0_Pin` = `GPIO_PIN_12` = `0x1000`（第 12 位置 1 的位掩码），不是引脚序号 12 本身。这是新手第一次看 HAL 代码最容易误解的地方。

### 5.5 生成后怎么验证标签生效

生成代码后打开 `Core/Src/gpio.c`，如果看到 `LED0_Pin|LED1_Pin|LED2_Pin` 这样的写法，说明标签链完全打通了：

```c
HAL_GPIO_WritePin(GPIOB, LED0_Pin|LED1_Pin|LED2_Pin, GPIO_PIN_RESET);
GPIO_InitStruct.Pin = LED0_Pin|LED1_Pin|LED2_Pin;
```

### 5.6 这一步的常见错误

| 错误做法 | 后果 |
| --- | --- |
| 标签填了 `LED0`，但点在 PA12 上 | 生成的宏指向 PA12，接在 PB12 的灯永远不亮，且编译无任何报错 |
| 三个脚填了同一个标签 | `redefinition of 'LED0_Pin'`，直接编译失败 |
| 标签写成 `LED 0`、`led-0`、`0LED` | 标签会拼进宏名，空格/连字符/数字开头都是非法 C 标识符 → 编译失败 |
| 忘了设 `GPIO output level = Low` | 上电瞬间三个灯状态不确定（可能全亮一下） |
| 选成 `GPIO_Input` | 引脚变成输入，`HAL_GPIO_WritePin` 写不动它，灯始终不亮 |

---

## 6. 第 4 步 Project Manager：决定 CLion 能不能打开

点顶部 **`Project Manager`** 标签。这一页决定两件事：**生成到哪**、**能不能被 CLion 用**。

### 6.1 Project 基本项

| 项 | 本次填什么 | 说明 |
| --- | --- | --- |
| Project Name | `STM32F103C8T6_LED_Flow` | 会成为文件夹名、CMake 工程名、`.elf` 文件名 |
| Project Location | `D:\_学习\RM电控` | 生成结果是 `...\RM电控\STM32F103C8T6_LED_Flow\` |
| Application Structure | `Advanced`（默认） | 保持默认即可 |
| Toolchain/IDE | **`STM32CubeIDE`** | **关键项，见 6.2** |
| Generate Under Root | 勾选 | 让 `.ioc` 和 `CMakeLists.txt` 生成在工程根目录里 |

> 路径建议：**纯英文、无空格最佳**。当前路径含中文（`_学习\RM电控`），本机实测 CubeMX/CLion 图形界面都能用、gcc 直接编译也正常；但如果遇到工具报奇怪的路径错误，第一步就是把工程搬到 `D:\Clion_Project\RM\` 这类纯 ASCII 路径再试。

### 6.2 Toolchain/IDE 为什么必须选 STM32CubeIDE

培训文档要求"把默认的 EWARM 换成 STM32CubeIDE"。原因：

| 选项 | 生成什么工程文件 | CLion 能用吗 |
| --- | --- | --- |
| EWARM | IAR 的 `.ewp/.eww` | 不能，没有 `CMakeLists.txt` |
| MDK-ARM | Keil 的 `.uvprojx` | 不能，同上 |
| Makefile | `Makefile` | 勉强可用，但 CLion 默认走 CMake，配置麻烦 |
| **STM32CubeIDE** | `.cproject/.project` **以及 `CMakeLists.txt`** | **能**，CLion 靠 CMakeLists 构建 |

关键点：新版 CubeIDE 内部也换成了 CMake 构建，所以选它时 CubeMX 6.x 会**同时**生成 `.cproject`（给 CubeIDE 用）和 `CMakeLists.txt`（给 CLion / 命令行用）。第一次作业时 CLion 能打开工程，就是因为这个 `CMakeLists.txt`。

### 6.3 Code Generator 勾选项逐条解释

培训文档截图里标红的勾选项，就是下面这些（在 `Project Manager` 页面下半部分 `Code Generator`）：

| 选项 | 建议 | 作用 | 不勾的后果 |
| --- | --- | --- | --- |
| **Copy only the necessary library files** | 勾 | 只把用到的 HAL `.c` 拷进工程的 `Drivers/` | 勾 "Copy all" 会让工程多出几十 MB，GitHub 仓库臃肿、clone 慢 |
| **Generate peripheral initialization as a pair of '.c/.h' files per peripheral** | **勾** | GPIO 的初始化独立成 `gpio.c` + `gpio.h` | **代码全塞进 `main.c`**，本工程里就不会有 `gpio.c`（第 10.2 节要改的地方会变成 `main.c` 里的 `MX_GPIO_Init`） |
| **Keep User Code when re-generating** | **勾（绝对不要取消）** | 重新 GENERATE 时保留 `USER CODE BEGIN/END` 段里的内容 | 你写的所有逻辑在下一次点击 GENERATE 时被清空——那是真的会哭 |
| **Delete previously generated files when not re-generated** | 勾 | 配置变更后删掉不再需要的外设文件 | 旧文件残留，改配置后出现重复定义、链接冲突 |
| Set all free pins as analog | 可不勾 | 未使用引脚设为模拟模式，降低功耗 | 空闲脚浮空输入，噪声/功耗略高（点灯场景无影响） |
| Backup previously generated files | 可勾 | 生成前把旧文件备份一份 | 没有后悔药（但你有 git，其实不需要） |

**一定要理解第 2 条**：勾了它，GPIO 初始化才会生成独立的 `gpio.c`。如果你没勾，生成结果里只有 `main.c`，那么本教程第 10.2 节的代码位置就要对应改为 `main.c` 里的 `MX_GPIO_Init()` 函数——**内容完全一样，只是文件不同**。

### 6.4 生成

点右上角 **`GENERATE CODE`**。成功时：

- 弹窗 `Code generation finished`，按钮从灰变亮（`Open Project` 等）。
- 工程目录里出现 `CMakeLists.txt`、`Core/`、`Drivers/`、`STM32F103C8T6_LED_Flow.ioc`。

如果失败，看弹窗里的第一条 Error，最常见的是"固件包未安装"（回 3.3 节）和"路径含非法字符"。

---

## 7. 第 5 步 生成后目录解剖与 USER CODE 机制

### 7.1 每个文件是谁在用

```
STM32F103C8T6_LED_Flow/
├── STM32F103C8T6_LED_Flow.ioc      ← 配置真相源，双击可用 CubeMX 打开
├── CMakeLists.txt                  ← CLion 读它来构建
├── CMakeLists_template.txt         ← 上面那个文件的模板（重新生成时用来还原）
├── STM32F103C8TX_FLASH.ld          ← 链接脚本：Flash/RAM 怎么分段、向量表放哪
├── Core/
│   ├── Inc/
│   │   ├── main.h                  ← 引脚宏（LED0_Pin...）+ 全局函数声明
│   │   ├── gpio.h
│   │   ├── stm32f1xx_hal_conf.h    ← 决定哪些 HAL 模块参与编译（可选裁剪）
│   │   └── stm32f1xx_it.h
│   ├── Src/
│   │   ├── main.c                  ← 程序入口 + 你自己的业务逻辑
│   │   ├── gpio.c                  ← MX_GPIO_Init()：引脚初始化
│   │   ├── stm32f1xx_it.c          ← 中断服务函数（SysTick_Handler 在这）
│   │   ├── stm32f1xx_hal_msp.c     ← HAL_MspInit：外设底层时钟/引脚
│   │   └── system_stm32f1xx.c      ← SystemInit：向量表、初始时钟
│   └── Startup/
│       └── startup_stm32f103c8tx.s ← 汇编启动文件：复位后第一条指令、中断向量表
├── Drivers/
│   ├── CMSIS/                      ← 内核头文件 + 内核函数
│   └── STM32F1xx_HAL_Driver/       ← HAL 库源码（HAL_GPIO_WritePin 等）
└── .cproject / .project / .mxproject
```

| 文件 | 干什么 | 你该不该改 |
| --- | --- | --- |
| `main.c` | 入口 + 业务逻辑 | **只改 `USER CODE` 段** |
| `main.h` | 引脚宏 + 函数声明 | 引脚宏**不要手写**（CubeMX 生成）；自己的常量放 `USER CODE PD` 段 |
| `gpio.c` | `MX_GPIO_Init()` | CubeMX 生成，正常不动 |
| `stm32f1xx_it.c` | 中断服务函数（`SysTick_Handler` 调 `HAL_IncTick()`） | 需要加中断时才动 |
| `stm32f1xx_hal_msp.c` | 外设底层初始化（时钟使能、引脚复用） | 一般不动 |
| `system_stm32f1xx.c` | `SystemInit` | 不动 |
| `startup_stm32f103c8tx.s` | 复位入口、向量表 | 不动 |
| `STM32F103C8TX_FLASH.ld` | Flash/RAM 布局 | 只做第 9 节的 `READONLY` 替换 |
| `CMakeLists.txt` | 构建规则 | 自己新增 `.c` 文件时，要把它加到源文件列表里 |

### 7.2 USER CODE 机制：CubeMX 为什么不会吃掉你的代码

打开 `main.c`，你会看到大量成对的注释：

```c
/* USER CODE BEGIN PV */

/* USER CODE END PV */
```

规则只有三条：

1. **你的代码只写在 `USER CODE BEGIN xxx` 和 `USER CODE END xxx` 之间。**
2. **不要删除、不要改名这两行注释本身**（CubeMX 靠它们定位切片）。
3. 这两个注释之外的部分叫"生成区"，**下一次 GENERATE CODE 会被整体覆盖**。

原理：CubeMX 重新生成时不是"从零写文件"，而是"读旧文件 → 找到所有 `USER CODE` 段 → 把里面的内容原样剪下来 → 生成新文件 → 再把内容贴回同样的段落"。所以它既能更新配置代码，又不会碰你的逻辑。

**反过来说**：如果你把逻辑写在生成区（比如直接写进 `while(1)` 里但漏了 `USER CODE BEGIN WHILE` 之内），下一次改配置重新生成 → 逻辑消失，而且没有任何警告。

---

## 8. 第 6 步 导入 CLion 2026.2.2

### 8.1 打开工程

1. 启动 `D:\CLion\CLion 2026.2.2\bin\clion64.exe`。
2. `File → Open`，选择 **`D:\_学习\RM电控\STM32F103C8T6_LED_Flow`**（选的是**文件夹**，不是某个文件）；或者直接把文件夹拖进 CLion 窗口。
3. 弹出 **Trust Project（信任工程）** → 选 **`Trust Project`**。不信任的话 CLion 不执行构建脚本，你会看到 CMake 一直转圈或功能灰掉。
4. 首次会问是"作为 CMake 工程打开"还是"作为别的"——保持默认 **CMake**，点 OK/Open。
5. 底部 **CMake 工具窗口** 出现，跑完显示 `[Finished]`，说明 `CMakeLists.txt` 被成功解析。

> 第一次培训文档里说"选信任、选跳过"，说的就是这个阶段：信任工程 + 跳过 CLion 自动生成的某些配置建议。

### 8.2 工具链：为什么"MinGW"指的是 ARM 的 gcc

`File → Settings`（`Ctrl+Alt+S`）→ `Build, Execution, Deployment → Toolchains`：

1. 点 `+` 新建一个工具链，Type 选 **MinGW**（类型只是个标签，不影响实际编译）。
2. 把编译器路径指到 ARM 工具链：

| 字段 | 填什么 |
| --- | --- |
| Name | `Debug-MinGW (STM32 gcc/g++)`（随便起，第一次作业截图里就是这个名） |
| C Compiler | `D:\CLion\CLion 2026.2.2\arm\bin\arm-none-eabi-gcc.exe` |
| C++ Compiler | `D:\CLion\CLion 2026.2.2\arm\bin\arm-none-eabi-g++.exe`（**注意是 g++，别和 gcc 选成同一个**） |
| Debugger | `D:\CLion\CLion 2026.2.2\arm\bin\arm-none-eabi-gdb.exe` |
| Build Tool | CLion 自带的 `ninja.exe`（在 `bin\ninja\win\x64\`） |
| CMake | CLion 自带的 `cmake.exe`（在 `bin\cmake\win\x64\bin\`） |

3. 用右边的 **↑** 把它提到列表最上面（设为默认）。

**为什么不用 CLion 自带的 MinGW**：那个是编译 Windows 桌面程序（x86 架构）的 gcc。用它编 ARM 工程会立刻报错，典型信息：

```
arm-none-eabi-gcc: error: unrecognized command line option '-mcpu=cortex-m3'
```

因为 x86 的 gcc 根本不认识 `-mcpu=cortex-m3` 这类 ARM 专用参数。**名字叫 MinGW 是假象，真正的判据是"编译器指向哪个 exe"。**

### 8.3 CMake 配置（Profile）

`Settings → Build, Execution, Deployment → CMake`：

| 项 | 值 |
| --- | --- |
| Name | `Debug` |
| Build type | `Debug` |
| Toolchain | 选 8.2 建的那个 ARM 工具链 |
| Generator | `Ninja`（或 `Let CMake decide`） |

首次配置如果红了一片，点 CMake 面板左上角的 🔄 **Reload CMake Project**；还不行就 `Tools → CMake → Reset Cache and Reload Project`。

### 8.4 Embedded Development（烧录相关）

`Settings → Build, Execution, Deployment → Embedded Development`：

| 项 | 值 | 说明 |
| --- | --- | --- |
| OpenOCD executable | `D:\OpenOCD\bin\openocd.exe` | 不填的话运行配置里没有 OpenOCD 选项 |
| STM32CubeMX executable | `D:\CubeMX\STM32CubeMX.exe` | 让 CLion 能直接打开 `.ioc` |
| 其余（CLT / ST-LINK 服务等） | 留空 | 培训文档明确说不用管 |

### 8.5 首次配置失败怎么读报错

CMake 面板里报错要**从下往上看**：最上面是它执行的 cmake 命令，中间是 CMake 自己的输出，**真正的原因通常在 `Error:` 或 `error:` 之后那一行**。常见三种：

| 报错关键词 | 原因 | 处理 |
| --- | --- | --- |
| `could not find any instance of Visual Studio` / `No CMAKE_C_COMPILER` | Toolchain 没选对，CLion 在找 MSVC | 回 8.3 把 Toolchain 指到 ARM 那个 |
| `The C compiler is not able to compile a simple test program` | 同上，或路径里有中文/空格 | 换纯英文路径重试 |
| `Error: ... READONLY` | 链接脚本问题 | 见第 9 节 |

---

## 9. 第 7 步 必改：链接脚本里的 READONLY

### 9.1 现象

不改这一步，第一次点编译就会在**链接阶段**失败（编译都过了，卡在最后一步），报错类似：

```
STM32F103C8TX_FLASH.ld:89: syntax error
collect2.exe: error: ld returned 1 exit status
```

或（不同版本措辞不同）：

```
Error: unrecognized section attribute 'READONLY'
```

### 9.2 原因：GCC 版本不匹配

CubeMX 新模板在"只读段"上加了一个段属性 `(READONLY)`，**这个关键字从 GCC 11 才开始支持**。而本机 `arm-none-eabi-gcc --version` 是 **10.3.1**，链接器（ld）不认识它，直接语法错误。

其实脚本自己在注释里写明了这件事：

```ld
  .ARM.extab (READONLY) : /* The "READONLY" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
```

翻译过来就是："`READONLY` 只有 GCC11 及以上支持，如果你用 GCC10 或更早的版本，就把它删掉。" 培训文档让你做的就是这个。

### 9.3 操作（30 秒）

1. 在 CLion 里打开工程根目录的 **`STM32F103C8TX_FLASH.ld`**。
2. 按 **`Ctrl+R`** 打开替换框。
3. 查找内容填 **`READONLY`**，替换内容**留空**。
4. 点 **`Replace All`**（全部替换）。
5. 搜索框里再搜一次 `READONLY`，确认**结果为 0**。

本工程原本有 **5 处**，分别在 `.ARM.extab`、`.ARM`、`.preinit_array`、`.init_array`、`.fini_array` 这几行。替换后变成：

```ld
  .ARM.extab () : /* The "" keyword is only supported in GCC11 and later, remove it if using GCC10 or earlier. */
```

**括号里空了是正常的、合法的**（空属性列表），不用再改注释。`Ctrl+S` 保存。

### 9.4 为什么要"关掉 CLion 再重新打开"

第一次培训文档的第 4.3 步要求关掉重开。原因是：CLion 会缓存 CMake/链接器的配置结果，改完链接脚本后有时不会自动重新链接，重开一次最省事。现在新版 CLion 一般会自动感知文件变化，**如果你改完直接编译能过，就不用重开**。

---

## 10. 第 8 步 改代码：三处、为什么、改哪

前面 9 步都是准备，**这一节才是作业的"写代码"部分**。

### 10.0 总规则（先记住，能省掉后面 90% 的麻烦）

| 规则 | 说明 |
| --- | --- |
| 只写 `USER CODE BEGIN/END` 之间 | 否则下次 GENERATE CODE 被覆盖 |
| 不要手写引脚宏 | `LED0_Pin` 等由 CubeMX 按 User Label 生成，手写会重定义 |
| 不要改生成区 | 包括 `SystemClock_Config()`、`MX_GPIO_Init()` 的函数体 |
| 自己的常量/结构体/函数 | 放在 `main.c` 的 `USER CODE PD` / `PV` / `PFP` / `4` 段里 |

本次工程一共动了**三处**，下面逐个说清楚"改哪、改成什么、为什么这么改"。

---

### 10.1 第一处：`Core/Inc/main.h` —— LED 宏（**不用你改，只要确认**）

打开 `main.h`，拉到第 65 行附近，会看到 CubeMX 自动生成的内容：

```c
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

/* Private defines -----------------------------------------------------------*/
#define LED0_Pin GPIO_PIN_12
#define LED0_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_13
#define LED1_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_14
#define LED2_GPIO_Port GPIOB
```

要点：

- 这 6 行在你点击 GENERATE CODE 的那一刻就已经存在了，**这是第 5 节 User Label 的直接产物**——你什么代码都不用写，宏就有了。
- 它们位于 `USER CODE END Private defines` **之后**，属于生成区，**不要手写、不要复制一份**，否则重新生成时会出现：

```
error: "LED0_Pin" redefined
```

- 如果你打开 `main.h` 发现没有这几行，说明第 5 节的 User Label 没生效（回 5.5 节检查 `gpio.c` 里有没有 `LED0_Pin`）。

---

### 10.2 第二处：`Core/Src/gpio.c` —— 引脚初始化（**同样不用你改，但要读懂**）

```c
void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};       /* ① 结构体清零 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();                 /* ② 先开外设时钟 */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED0_Pin|LED1_Pin|LED2_Pin, GPIO_PIN_RESET);  /* ③ 先定电平 */

  /*Configure GPIO pins : LED0_Pin LED1_Pin LED2_Pin */
  GPIO_InitStruct.Pin   = LED0_Pin|LED1_Pin|LED2_Pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;  /* 推挽输出 */
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);       /* ④ 一次性配三个脚 */
}
```

逐条讲"为什么"：

**① 结构体清零 `= {0}`**
`GPIO_InitTypeDef` 有 4 个成员。如果不清零，未赋值的成员就是栈上的随机垃圾值，可能把某个保留位写成 1。清零是嵌入式里"确定性"的基本功。

**② 为什么必须开时钟**
STM32 每个外设（包括 GPIOA~GPIOE）都有独立的时钟门控，复位后默认**全部关闭**以省电。时钟不开时，往 GPIO 寄存器写数据**不会报错、直接无效**——现象就是"代码逻辑完全正确，引脚就是不动"。这是新手最常卡的一种 bug，而且没有任何报错信息。
（这里连 GPIOC/GPIOD 一起开，是因为蓝pill 的 PC13/PC14/PD0/PD1 被配成晶振脚，CubeMX 顺手把整组时钟打开了，开着不用不花钱。）

**③ 为什么"先写电平，再配成输出"**
`HAL_GPIO_Init()` 执行的那一瞬间，引脚从"复位默认的输入/浮空"切换成"推挽输出"，此时输出的电平取自输出数据寄存器 ODR 的当前值。先 `WritePin(...RESET)` 把 ODR 置 0，再切输出，切换瞬间就是确定的低电平（灯灭）。反过来写，就可能在上电瞬间看到三个灯"闪一下"。

**④ 为什么三个脚一次配完**
`GPIO_InitStruct.Pin` 是**位掩码**：`GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14` = `0x7000`。同一个结构体里的参数（模式/上下拉/速度）对这三个脚都适用，所以一次 `HAL_GPIO_Init()` 全部搞定。如果三个灯参数不同（比如一个推挽一个开漏），就必须分两次调用。

> **这一处正常不需要你改。** 要改引脚就在 CubeMX 里改（第 5 节），重新生成，`gpio.c` 自动更新——这就是"配置和代码分离"的好处。

---

### 10.3 第三处：`Core/Src/main.c` —— 流水灯逻辑（**这才是你写的代码**）

`main.c` 一共 258 行，你写的部分分散在 5 个 `USER CODE` 段里，按文件从上到下的顺序：

#### (a) `USER CODE BEGIN PD`（第 34-46 行）：参数与宏

```c
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
```

**为什么这么写**：

- **`500U` 而不是 `500`**：`U` 是"无符号整数"后缀。`HAL_GetTick()` 返回 `uint32_t`，时间比较时两边类型一致，避免隐式转换告警，也让编译器放心优化。
- **`LED_FLOW_COUNT` 单独定义**：灯的个数在 4 个地方用到（映射表大小、循环上界、取余、初始化遍历）。写成宏 → 加灯时只改一处。
- **`LED_ON` / `LED_OFF` 单独定义**：这是"硬件极性"的抽象。如果哪天接线改成共阳（低电平点亮），只把这两行对调，**全程序逻辑一行不用动**。这就是为什么第 5.3 节强调极性要说清楚。

#### (b) `USER CODE BEGIN PV`（第 55-76 行）：映射表与状态变量

```c
/* USER CODE BEGIN PV */

typedef struct
{
  GPIO_TypeDef *port;    /* 端口：GPIOB */
  uint16_t      pin;     /* 引脚：GPIO_PIN_12 这种位掩码 */
} LedPin_t;

static const LedPin_t s_ledTable[LED_FLOW_COUNT] =
{
  { LED0_GPIO_Port, LED0_Pin },   /* PB12 */
  { LED1_GPIO_Port, LED1_Pin },   /* PB13 */
  { LED2_GPIO_Port, LED2_Pin },   /* PB14 */
};

static uint32_t s_tickBase = 0U;   /* 上一次切换的时间戳（单位 ms） */
static uint32_t s_ledIndex = 0U;   /* 当前点亮的灯在 s_ledTable 中的下标 */

/* USER CODE END PV */
```

**为什么用"表"而不是三个 `if`**：

| 写法 | 加第 4 个灯要改几处 | 调换灯序 | 可读性 |
| --- | --- | --- | --- |
| 三个 `if/else` 写死 | 改判断逻辑 + 加分支 | 改逻辑 | 越写越长 |
| **映射表 + 下标** | **表里加一行、`LED_FLOW_COUNT` 改 4** | **表里换两行顺序** | 数据与逻辑分离 |

这是"数据驱动"的入门版。表里直接用 CubeMX 生成的宏，所以**在 CubeMX 里把 LED0 从 PB12 改到 PB0，这张表一行都不用动**。

**为什么是 `static const`**：

- `static`：只在 `main.c` 内部可见，不污染全局命名空间（别的文件引用不到）。
- `const`：内容编译期确定，放 `.rodata` → 链接脚本把它放进 **Flash** 而不是 RAM。本工程 RAM 只有 20 KB，能省就省。

#### (c) `USER CODE BEGIN PFP`（第 80-85 行）：函数声明

```c
/* USER CODE BEGIN PFP */

static void LedFlow_Init(void);
static void LedFlow_Poll(uint32_t nowMs);

/* USER CODE END PFP */
```

C 语言规定"先声明后使用"。`Proc` 在 `main()` 里被调用，而 `LedFlow_Poll` 的定义在文件末尾第 208 行，所以要先在这里声明。`static` 让这两个函数只在本文件可见。

#### (d) `USER CODE BEGIN 2` + `USER CODE BEGIN WHILE`（第 121-139 行）：在 main 里接上逻辑

```c
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
```

**main() 里 5 个调用的顺序为什么不能乱**：

| 顺序 | 调用 | 作用 | 顺序错了会怎样 |
| --- | --- | --- | --- |
| 1 | `HAL_Init()` | 复位所有外设、初始化 Flash 接口、把 SysTick 配成 1 ms | 放后面 → `HAL_GetTick()` 不走、所有 HAL 延时全错 |
| 2 | `SystemClock_Config()` | 时钟从 8 MHz 升到 72 MHz | 放后面 → 后续外设按 8 MHz 算，串口波特率、定时器周期全偏 |
| 3 | `MX_GPIO_Init()` | 配 PB12/13/14 | 时钟没起来就配，无效 |
| 4 | `LedFlow_Init()` | 全灭 → 点亮第 0 个 → 记基准时间 | 不调 → 上电后三个灯状态不确定，第一拍时间错 |
| 5 | `while(1){ LedFlow_Poll(HAL_GetTick()); }` | 主循环反复检查节拍 | 不调 → 灯永远停在初始状态 |

> `HAL_Init()` 和 `SystemClock_Config()` 是 CubeMX 生成区里的，**不要动它们的顺序**。你自己的 `LedFlow_Init()` 也放进 `USER CODE 2` 段——因为它在 `MX_GPIO_Init()` 之后，此时引脚已经配好，写电平才有效。

#### (e) `USER CODE BEGIN 4`（第 182-227 行）：两个函数本体

```c
/* USER CODE BEGIN 4 */

static void LedFlow_Init(void)
{
  uint32_t i;

  for (i = 0U; i < LED_FLOW_COUNT; i++)          /* 先全部熄灭 */
  {
    HAL_GPIO_WritePin(s_ledTable[i].port, s_ledTable[i].pin, LED_OFF);
  }

  s_ledIndex = 0U;
  HAL_GPIO_WritePin(s_ledTable[0].port, s_ledTable[0].pin, LED_ON);  /* 点亮第 0 个 */

  s_tickBase = HAL_GetTick();                    /* 记下起始时刻 */
}

static void LedFlow_Poll(uint32_t nowMs)
{
  /* 无符号减法算时间差：即使 uwTick 溢出回绕（约 49.7 天一次），结果依然正确 */
  if ((uint32_t)(nowMs - s_tickBase) < LED_FLOW_INTERVAL_MS)
  {
    return;   /* 还没到 500 ms —— 立刻返回，主循环继续跑 */
  }

  /* 时间到：把基准时间推进一个周期（+= 而不是 = now，避免累积误差） */
  s_tickBase += LED_FLOW_INTERVAL_MS;

  /* 关掉当前灯，序号跳到下一个，点亮新灯 —— 形成"流水"效果 */
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_OFF);
  s_ledIndex = (s_ledIndex + 1U) % LED_FLOW_COUNT;
  HAL_GPIO_WritePin(s_ledTable[s_ledIndex].port, s_ledTable[s_ledIndex].pin, LED_ON);
}

/* USER CODE END 4 */
```

**这是加分项的核心，四个技术点必须能讲清楚（老师大概率会问）**：

**① `HAL_GetTick()` 的时间从哪来？**

```
SysTick 定时器（1 ms 中断）
        ↓ 每 1 ms 触发一次
SysTick_Handler()            （在 stm32f1xx_it.c）
        ↓ 调用
HAL_IncTick()
        ↓ 累加
uwTick++                     （全局变量，开机以来经过的毫秒数）
        ↓ 被读取
HAL_GetTick() 返回 uwTick
```

关键：**时间是硬件中断在后台自动累加的，和你有没有在 `while` 里等完全无关**。这就是非阻塞的物理基础。

**② 非阻塞的本质**

| | `HAL_Delay(500)` | `LedFlow_Poll()` 轮询 |
| --- | --- | --- |
| 执行方式 | 原地 `while` 等 `uwTick` 跳到目标值 | 比较一次差值，没到点立即 `return` |
| 占用 CPU | 500 ms 全程被占死 | 每次约几微秒 |
| 期间能做别的事 | **不能**（读按键、刷 OLED、收串口全停） | **能**（主循环下一轮继续） |
| 精度 | 取决于 `uwTick` | 取决于 `uwTick` + 主循环周期（ms 级） |

**③ 无符号减法为什么天然抗溢出**

`uwTick` 是 `uint32_t`，最大 4294967295 ms ≈ **49.7 天**后回绕成 0。如果写 `if (now < base + 500)`，回绕时 `base + 500` 会溢出，判断直接失效。而写成减法的形式：

```c
(uint32_t)(nowMs - s_tickBase) < 500
```

在无符号运算里，减法结果是**模 2³² 的环上距离**。设 `base = 4294967290`、`now = 5`（刚回绕），则 `5 - 4294967290 = 11`（模 2³²），正好是真实经过的 11 ms。**只要间隔小于 2³¹ ms（约 24.8 天），这个差值永远正确**，500 ms 远远满足。这就是"环形计数器"的标准写法，也是嵌入式里判断超时的通用范式。

**④ `+=` 而不是 `= now`**

每隔 500 ms 触发一次，实际触发时刻总会比理想晚一点点（几微秒到几毫秒，取决于主循环这一轮干了多少事）。

- `s_tickBase = nowMs;` → 每次都把"落后量"吞掉，误差单向累积，跑几小时后就明显偏慢。
- `s_tickBase += 500;` → 基准时间沿"理想时间轴"往前走，误差不累积，平均周期严格 500 ms。

代价是：如果程序被长时间阻塞（比如某轮循环花了 2 秒），接下来会连续快闪几次把欠的节拍补上。这对流水灯是合理行为（时间基准优先）。若你希望"落后就重新计时、不要补"，写成 `s_tickBase = nowMs;` 也完全正确——**两种写法对应两种语义，选哪种取决于你要"绝对节拍"还是"相对间隔"，能在 README 里说清楚就是加分**。

**⑤ `% LED_FLOW_COUNT` 取余**

`0 → 1 → 2 → 0 → 1 → 2 ...`，索引自动循环，不用写 `if (idx >= 3) idx = 0;`。

#### (f) 想换引脚 / 加灯，怎么办？

| 需求 | 要改哪里 |
| --- | --- |
| 换一个 LED 的引脚 | **只改 CubeMX**：改 User Label 所在引脚 → GENERATE → 编译。`main.c` 一行不用动 |
| 加第 4 个灯 | CubeMX 加一个脚，标签 `LED3` → 生成 → `main.c` 里 `LED_FLOW_COUNT` 改 `4U`、表里加一行 `{ LED3_GPIO_Port, LED3_Pin }` |
| 改流水速度 | 改 `LED_FLOW_INTERVAL_MS`（如 `200U`） |
| 改点亮极性 | 把 `LED_ON` / `LED_OFF` 两个宏对调 |

这就是这套代码结构的意义：**硬件变化 90% 在 CubeMX 里解决，逻辑代码几乎不动**。

---

## 11. 第 9 步 编译与读输出

### 11.1 怎么编译

| 操作 | 快捷键 / 位置 | 说明 |
| --- | --- | --- |
| 编译整个工程 | 底部工具栏 **🔨 小扳手**，或 `Ctrl+F9` | 只构建，不烧录 |
| 只重新编译改过的文件 | 同上（CLion 内部增量） | 正常情况秒级完成 |
| 清理重编 | `Build → Rebuild Project` | 改了 CMakeLists 或出现诡异错误时用 |

第一次培训文档里的"小扳手 = 编译，绿色三角 = 编译并烧录"，就是这个。

### 11.2 输出怎么读

一次成功构建的输出分三段：

```
====================[ Build | STM32F103C8T6_LED_Flow | Debug ]===================
[1/9] Building C object CMakeFiles/STM32F103C8T6_LED_Flow.dir/Core/Src/main.c.obj      ← ① 逐文件编译
...
[9/9] Linking C executable STM32F103C8T6_LED_Flow.elf                                 ← ② 链接
Memory region         Used Size  Region Size  %age Used                                ← ③ 内存占用
             FLASH:       5028 B       64 KB      7.67%
               RAM:       2664 B       20 KB     13.01%

Build finished                                                                        ← ④ 成功
```

**数字怎么读**：

- `FLASH 5028 B / 64 KB (7.67%)`：代码 + 常量占了 5 KB，芯片有 64 KB。还有大量空间做后续作业。
- `RAM 2664 B / 20 KB (13.01%)`：全局变量 + 栈 + 堆占了 2.6 KB。20 KB 很小，写大数组（比如 `uint8_t buf[1024]`）时要注意。
- 数字偏大不是错误；**红色报错才是**。

### 11.3 报错怎么定位

| 报错形态 | 属于 | 含义 | 怎么修 |
| --- | --- | --- | --- |
| `error: expected ';' before ...` | 编译器 | 语法错 | 双击报错行，CLion 自动跳转 |
| `error: 'LED3_Pin' undeclared` | 编译器 | 宏没生成 | 回 CubeMX 检查 User Label 是否拼对、是否 GENERATE 过 |
| `error: "LED0_Pin" redefined` | 编译器 | 你手写了同一宏 | 删掉自己写的那份（第 10.1 节） |
| `undefined reference to 'LedFlow_Poll'` | **链接器** | 函数只有声明没有定义 | 检查函数体是否被误删，或是不是写成了 `static` 但没实现 |
| `ld: STM32F103C8TX_FLASH.ld:89: syntax error` | **链接器** | `READONLY` 没删干净 | 回第 9 节 |
| `region 'FLASH' overflowed by N bytes` | 链接器 | 代码超 64 KB | 本次不可能，除非误把 `Drivers` 整个拷了多份 |

> 链接错误最容易被新手误判成"代码写错了"。记一条：**`undefined reference` 永远是"用到但没找到实现"**——要么没编译进去（新加的 `.c` 没写进 `CMakeLists.txt`），要么函数名拼错，要么只有声明。

---

## 12. 第 10 步 烧录、验收录像与接线

### 12.1 接线（画在纸上先算一遍）

LED 接法（**每个 LED 各串一个电阻**）：

```
PB12 ──[1kΩ]──▶|── GND        （▶| = LED，长脚是阳极 +）
PB13 ──[1kΩ]──▶|── GND
PB14 ──[1kΩ]──▶|── GND
```

ST-LINK 与最小系统板（4 根线，**必须共地**）：

| ST-LINK V2 | 最小系统板 | 说明 |
| --- | --- | --- |
| `3.3V` | `3V3` | 只在板子没有独立供电时接；板子已由 USB 供电就别接，避免两个电源打架 |
| `GND` | `GND` | **必接**，不共地 SWD 通信必然失败 |
| `SWDIO` | `DIO` / `SWIO`（PA13） | 数据线 |
| `SWCLK` | `CLK` / `SWCLK`（PA14） | 时钟线 |

> **不要**把 5V 接到 `3V3`。**不要**把 SWD 的 3.3V 与板子 USB 供电同时硬接（除非确认板上有二极管防反灌）。

**电阻与亮度**：3.3 V 供电、红色 LED 正向压降约 1.8~2.0 V，串 1 kΩ 时电流 ≈ 1.3 mA——室内能看清，但偏暗；220~330 Ω 时 ≈ 6~7 mA，正常亮度。器件清单给的是 1 kΩ，觉得暗可以两只 1 kΩ 并联（≈500 Ω）或直接换 220 Ω。**多个 LED 千万不能共用一个电阻**（电流分配不定，亮度不均，还可能烧 LED）。

### 12.2 配置烧录运行项

1. 右上角运行配置下拉框 → **`Edit Configurations...`** → 左上 `+` → 选 **`OpenOCD Download & Run`**。
2. 填两个关键字段：

| 字段 | 值 |
| --- | --- |
| Name | `STM32F103C8T6_LED_Flow` |
| Target / Executable | **`cmake-build-debug\STM32F103C8T6_LED_Flow.elf`**（点右侧文件夹图标去选；本机实测 CLion 输出在 `cmake-build-debug\`，**不是** `build\debug\`） |
| Board config file | `D:\_学习\RM电控\QQ群文件-电控组\enviroment\CFJ_STM32\stm32f1.cfg` |
| GDB / OpenOCD 其余项 | 保持默认 |

> 如果 `+` 里**没有** `OpenOCD Download & Run` 这个选项，说明第 8.4 节的 `OpenOCD executable` 没填对路径。

3. 点 **绿色三角 ▶**：CLion 会**先编译**，再调用
   `openocd -f stm32f1.cfg -c "program xxx.elf verify reset exit"`
   依次执行 烧写 → 校验 → 复位 → 运行。

### 12.3 烧录成功的样子

Console 里应看到（关键三行）：

```
** Programming Started **
** Programming Finished **
** Verified OK **
** Resetting Target **
```

看到 `Verified OK` 就说明代码已经正确写进 Flash 并复位运行了。

### 12.4 常见报错对照表

| 报错 | 原因 | 处理 |
| --- | --- | --- |
| `Error: open failed` / `could not find stlink` | ST-LINK 驱动没装 / USB 线只供电不传数据 | 设备管理器确认识别为 `STLink USB`；换数据线 |
| `Error: init mode failed` / `target not found` | SWDIO/SWCLK 接反、没共地、板子没供电、芯片被写保护 | 逐项排查；先按住板子复位键再点烧录 |
| `Error: expected 1 Tap, got 0` | 芯片没进调试态 / BOOT0 电平不对 | BOOT0 接 GND（正常从 Flash 启动）后重新上电 |
| `Error: flash download failed` | 选的芯片型号与目标不符（比如按 128 KB 的链接脚本烧 64 KB 芯片） | 回 CubeMX 确认型号是 C8T6 |
| 烧录成功但灯全不亮 | 极性接反（想用灌电流却按拉电流接）、引脚接错、电阻太大 | 万用表量 PB12 对 GND 电压，亮时约 3.3 V |
| 只亮一个灯，不流水 | `LedFlow_Init()` 没调用 / `s_tickBase` 没初始化 / 主循环被卡住 | 检查第 10.3(d) 节的位置 |
| 流水了但节奏不是 500 ms | `LED_FLOW_INTERVAL_MS` 写错 / 时钟没到 72 MHz（SysTick 反而更慢，但 1 ms 是独立计数器，一般不受影响） | 查宏值；用秒表/手机录屏数格子 |
| **烧录一次后 ST-LINK 再也连不上** | SYS → Debug 没选 `Serial Wire`，程序把 PA13/PA14 当普通 IO 用了 | **抢救**：BOOT0 接 3V3 上电（进系统存储器），此时 SWD 可用，重新烧一个 Debug 配对正确的程序，再把 BOOT0 接回 GND |

### 12.5 验收录像怎么拍（照作业要求的顺序）

作业要求：**视频要展示 CLion 编译通过 → 启动烧录 → 成功烧录到开发板 → 板上流水灯 500 ms 正常闪烁**。所以一条连续视频里要包含这 4 个镜头：

| 分镜 | 拍什么 | 时长 |
| --- | --- | --- |
| ① 编译通过 | CLion 界面，点小扳手，画面里出现 `Build finished` / `[100%] Built target ...` | 3~5 秒 |
| ② 启动烧录 | 点绿色三角，Console 出现 `** Programming Started **` | 2~3 秒 |
| ③ 烧录成功 | Console 出现 `** Verified OK **` / `** Resetting Target **` | 2~3 秒 |
| ④ 板上运行 | 镜头切到开发板，三个 LED 依次亮，节奏稳定在 500 ms | 10~15 秒 |

做法建议：

- **一次录完，不要剪接**（老师要看因果连贯），手机横屏对着屏幕和板子同框拍最省事；用 OBS 录屏 + 手机拍板子后拼接也可以，但要在 README 里说明。
- 时长 20~40 秒足够，文件别超过 100 MB（作业提交规范要求单文件 <100 MB）。
- 文件名 `验收视频.mp4` 放在**仓库根目录**，README 里用相对路径引用它。
- 多份代码就录多个视频（本次只有一份）。

### 12.6 专项：GDB 连不上 OpenOCD（`could not connect (error 138)`）

**现象**：点绿色三角后，Debug 控制台先打印 GDB 自检信息（版本号、`x86_64-w64-mingw32` 配置），然后：

```
could not connect (error 138): ϵͳ��ͼ���������ϲ����ϲ��������ϵ�Ŀ¼��
You can't do that when your target is `exec'
"monitor" command not supported by this target.
The program is not being run.
上传完成，但存在问题
monitor reset
"monitor" command not supported by this target.
continue &
The program is not being run.
```

**三句话读懂这段日志**：

1. GDB 起来了，但 **连不上 OpenOCD 的调试端口（默认 3333）**，后面全是"连不上"的连锁反应。
2. `target is exec` = 目标还停在本地可执行文件上，**没有挂到芯片**。所以 `monitor reset`（OpenOCD 专属命令）和 `continue` 自然不被支持。
3. 那串乱码是 **Windows 中文系统错误信息被按错误编码打印**（大意"系统找不到指定的文件或目录"）。这句话才是根因线索：**问题在系统层——找不到设备 / 找不到文件 / 路径解析失败，不是你写的代码有错。**

**按概率顺序排查**：

| 步 | 检查什么 | 怎么做 | 通过标准 |
| --- | --- | --- | --- |
| 1 | **ST-LINK 有没有被 Windows 认出来** | `Win+X → 设备管理器`，展开"通用串行总线设备" | 出现 **`STM32 STLink`**（或 `STLink USB`）且无黄色感叹号 |
| 2 | USB 线与 USB 口 | 换数据线（有些线只供电）、换主板后置口 | 同上 |
| 3 | 驱动 | 装 ST 官方 ST-LINK 驱动（`STM32CubeProgrammer` / `en.stsw-link009` 自带）；克隆版 ST-LINK 可用 Zadig 换成 **WinUSB** | 设备名正常、无感叹号 |
| 4 | **路径里有没有中文** | 把 `stm32f1.cfg` 复制到纯 ASCII 路径（如 `D:\STM32_CFG\stm32f1.cfg`），运行配置重新指向；工程整体建议迁到 `D:\Clion_Project\RM\` | 手动跑 OpenOCD 不再报"找不到" |
| 5 | 有没有残留 `openocd.exe` 占用 3333 | 任务管理器结束所有 `openocd.exe` | 端口空闲 |
| 6 | GDB 用的是哪一个 | `Settings → Build, Execution, Deployment → Embedded Development`，GDB 指到 `D:\CLion\CLion 2026.2.2\arm\bin\arm-none-eabi-gdb.exe` | 控制台首行变成 `arm-none-eabi-gdb` |

**手动验证（绕开 CLion，最快定位）**：PowerShell 里先给配置建个纯英文目录：

```powershell
New-Item -ItemType Directory -Force D:\STM32_CFG | Out-Null
Copy-Item 'D:\_学习\RM电控\QQ群文件-电控组\enviroment\CFJ_STM32\stm32f1.cfg' D:\STM32_CFG\
& 'D:\OpenOCD\bin\openocd.exe' -f 'D:\STM32_CFG\stm32f1.cfg'
```

**正常**输出（关键看最后几行）：

```
Info : STLINK V2J.. (API v2) VID:PID 0483:3748
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : Listening on port 3333 for gdb connections
```

对照判断：

| 现象 | 含义 | 去第几步 |
| --- | --- | --- |
| `Error: open failed` / `no device found` / 第一行就没出 STLINK | ST-LINK 没连上或驱动不对 | 1~3 |
| 一串乱码"系统找不到…" | 路径含中文或文件不存在 | 4 |
| `Can't find interface/stlink.cfg` | OpenOCD 的 `-s scripts` 路径问题 | 官方安装版一般不会遇到 |
| `Error: init mode failed (target not responding)` | SWDIO/SWCLK 接反、没共地、芯片被写保护 | 查接线；BOOT0 接 GND 后重新上电 |

**手动烧录（CLion 不配合时的兜底）**：

```powershell
$elf = 'D:\_学习\RM电控\STM32F103C8T6_LED_Flow\cmake-build-debug\STM32F103C8T6_LED_Flow.elf'
& 'D:\OpenOCD\bin\openocd.exe' -f 'D:\STM32_CFG\stm32f1.cfg' -c "program `"$elf`" verify reset exit"
```

看到 `** Programming Finished **` / `** Verified OK **` 就说明已经烧进去了，板子会立刻开始流水——**这条命令成功、CLion 里失败，说明是 CLion 调试会话的配置问题（GDB 路径/端口/运行配置里的 elf 路径），与硬件无关**，可以先跑通它完成作业，再回头修 CLion。

> **克隆版 ST-LINK 的 B 计划**：同目录 `stm32f103c8_blue_pill .cfg`（文件名带空格）用的是 `transport select hla_swd`，而 `stm32f1.cfg` 用的是 `transport select swd`。部分克隆版 ST-LINK V2 在 `swd` 模式下握手失败，把 `stm32f1.cfg` 里那一行改成 **`transport select hla_swd`** 往往就通了（hla 是 ST-LINK 老协议，兼容性更好）。

---

## 附录 A 常见坑速查表

按"从配置到运行"的顺序排列，遇到问题先在这里扫一眼：

| # | 现象 | 原因 | 解决 |
| --- | --- | --- | --- |
| 1 | CubeMX 里改完引脚，板子行为没变 | 忘记点 `GENERATE CODE`，或忘了在 CLion 重新编译+烧录 | 生成 → Build → 烧录，三步都要做 |
| 2 | 重新生成后自己写的代码没了 | 写在了 `USER CODE` 段之外 | 从 git 恢复，重写到 `USER CODE` 段内（第 7.2 节） |
| 3 | `error: "LED0_Pin" redefined` | 自己手写了一份引脚宏 | 删掉手写的，只用 CubeMX 生成的（第 10.1 节） |
| 4 | `error: 'LED1_Pin' undeclared` | User Label 拼错 / 忘了生成 / 标签点在别的脚 | 回 CubeMX 的 GPIO 表格核对（第 5.4 节） |
| 5 | `ld: ... syntax error` 指向 `.ld` | `READONLY` 没删干净（GCC10 不认） | `Ctrl+R` 全替换为空，确认 0 处（第 9 节） |
| 6 | CMake 报 `No CMAKE_C_COMPILER` / 找不到编译器 | Toolchain 指向了 x86 的 MinGW | 工具链 C 编译器改成 `arm-none-eabi-gcc.exe`（第 8.2 节） |
| 7 | `unrecognized command line option '-mcpu=cortex-m3'` | 用 x86 gcc 编 ARM 代码 | 同上 |
| 8 | 编译通过，灯全不亮 | GPIOB 时钟没开 / 引脚接错 / 极性反了 | 量引脚电压；检查 `gpio.c` 有没有 `__HAL_RCC_GPIOB_CLK_ENABLE()` |
| 9 | 上电瞬间三个灯闪一下才灭 | 初始电平没设，或"先配输出后写电平" | CubeMX 里 `GPIO output level = Low`（第 5.3 节） |
| 10 | 只亮一个灯，不流水 | `LedFlow_Init()` 没调用 / `s_tickBase` 没初始化 | 检查 `USER CODE 2` 段（第 10.3(d) 节） |
| 11 | 流水速度明显不是 500 ms | 宏写错，或心跳不是 1 ms | 查 `LED_FLOW_INTERVAL_MS`；确认 `HAL_Init()` 被调用 |
| 12 | 烧一次后 ST-LINK 再也连不上 | SYS → Debug 没选 `Serial Wire` | BOOT0=3V3 上电抢救，重烧正确程序（第 12.4 节末行） |
| 13 | `Error: open failed` | ST-LINK 驱动/线材问题 | 设备管理器确认，换数据线 |
| 14 | 烧录后芯片跑飞 / HardFault | 链接脚本 Flash/RAM 大小与芯片不符（选错型号） | 回 CubeMX 确认型号 C8T6，重新生成 |
| 15 | 工程 30 MB+，GitHub 推不上去 | 把 `cmake-build-*`、`.idea` 也提交了 | 加 `.gitignore`（附录 D） |
| 16 | 工具报奇怪的路径错误 | 路径含中文/空格 | 搬到 `D:\Clion_Project\RM\` 这类纯 ASCII 路径 |
| 17 | 中文注释变乱码 | 文件编码不是 UTF-8 | CLion 右下角切 `UTF-8`，保存后再编译 |

---

## 附录 B 术语表

| 术语 | 含义 |
| --- | --- |
| `.ioc` | CubeMX 的工程配置文件，纯文本 INI，记录芯片型号、引脚、时钟、工程选项。**双击可用 CubeMX 打开** |
| HAL | Hardware Abstraction Layer，ST 提供的硬件抽象库。`HAL_GPIO_WritePin()` 这类函数就是它，屏蔽了寄存器细节 |
| CMSIS | Cortex 微控制器软件接口标准，提供内核寄存器定义、`SysTick` 等内核相关代码 |
| CMake | 跨平台构建系统生成器，读 `CMakeLists.txt` 产出 Ninja/Make 能执行的构建文件 |
| Ninja | 一个很快的构建执行器，CLion 默认用它，比 make 快 |
| `.ld`（链接脚本） | 告诉链接器"哪些段放在 Flash 的哪个地址、哪些放 RAM、栈多大"。`STM32F103C8TX_FLASH.ld` 里定义了 `FLASH ORIGIN=0x8000000 LENGTH=64K`、`RAM ORIGIN=0x20000000 LENGTH=20K` |
| `.elf` | 带地址信息和符号表的可执行文件，OpenOCD 烧录用这个 |
| `.hex` / `.bin` | 纯烧录镜像（无调试信息），`.hex` 带地址、`.bin` 是裸数据 |
| `USER CODE` 段 | CubeMX 生成代码里的保护标记，重新生成时内容被保留（第 7.2 节） |
| SWD | Serial Wire Debug，两线调试接口（SWDIO + SWCLK），ST-LINK 用它烧录/调试 |
| JTAG | 五线调试接口，F1 与 SWD 共用 PA13~PA15、PB3、PB4，点灯场景用 SWD 就够 |
| OpenOCD | 开源片上调试工具，负责通过 ST-LINK 把 `.elf` 写进芯片并复位运行 |
| `-mcpu=cortex-m3 -mthumb` | 编译参数：目标内核 Cortex-M3、使用 Thumb 指令集（ARM 的 16/32 位混合指令，省空间） |
| 推挽输出（PP） | 引脚内部上下管都能导通，可主动输出高/低电平，有驱动能力 |
| 开漏输出（OD） | 只能拉低 + 高阻，需要外部上拉才能输出高电平；适合总线（I2C） |
| ODR | Output Data Register，输出数据寄存器。`HAL_GPIO_WritePin` 就是改它 |
| `uwTick` | HAL 的全局毫秒计数器，SysTick 中断里每 1 ms 加 1，`HAL_GetTick()` 返回它 |
| SysTick | Cortex-M 内核自带的 24 位递减定时器，HAL 用它做 1 ms 系统时基 |
| Flash / RAM | Flash 掉电不丢，存代码和常量（64 KB）；RAM 掉电即失，存变量和栈（20 KB） |
| HardFault | 最严重的异常，通常由访问非法地址、栈溢出、未对齐访问引起，表现为死循环 |

---

## 附录 C 验收自检清单

交作业前逐条打勾（对照《2026.10.24 电控组第二次招新培训作业》的要求）：

**基础任务**

- [ ] CubeMX 的 `.ioc` 里存在 `LED0` / `LED1` / `LED2` 三个 User Label（截一张 GPIO 配置页的图）
- [ ] 代码里**直接调用**了 `LED0_Pin` / `LED0_GPIO_Port` 等宏，而不是写死 `GPIO_PIN_12`
- [ ] CLion 编译 **0 error**，`Build finished`
- [ ] 三个灯按 `LED0 → LED1 → LED2 → LED0` 循环，间隔 500 ms
- [ ] 烧录到实物板子后确实在闪（不是只编译通过）

**加分项（非阻塞）**

- [ ] 全工程**没有** `HAL_Delay()`（`grep` 一下确认）
- [ ] README 里写清了非阻塞的原理（时间戳轮询 / `HAL_GetTick` / 无符号差值）
- [ ] README 里贴了核心代码片段（`LedFlow_Poll` 那一段）

**README 格式**

- [ ] 一级标题：`# 电控组第二次培训作业`
- [ ] 两行无序列表：`- **刘祥博**` 和 `- ***20263008899***`（名字粗体，学号**粗体+斜体** = 三个星号）
- [ ] 简要说明流水灯实现思路

**仓库与提交**

- [ ] GitHub 仓库是 **Public**（公开）
- [ ] 提交了：完整工程代码 + README.md + 验收视频 + 必要的截图
- [ ] **没有**混进 `cmake-build-*`、`.idea/`、`*.o`、`*.d`、`*.su`（用 `.gitignore` 排除）
- [ ] 单个文件 < 100 MB
- [ ] 仓库链接已填进 **电控培训 QQ 群作业提交表**
- [ ] 截止 **2026-11-2 24:00** 前完成

---

## 附录 D 本工程实际参数速查

**工程与路径**

| 项 | 值 |
| --- | --- |
| 工程目录 | `D:\_学习\RM电控\STM32F103C8T6_LED_Flow` |
| 配置文件 | `STM32F103C8T6_LED_Flow.ioc` |
| 链接脚本 | `STM32F103C8TX_FLASH.ld` |
| 主程序 | `Core\Src\main.c`（258 行） |
| 引脚初始化 | `Core\Src\gpio.c`（67 行） |
| 引脚宏 | `Core\Inc\main.h`（第 65-71 行） |
| 构建清单 | `CMakeLists.txt` |
| 构建产物 | `cmake-build-debug\STM32F103C8T6_LED_Flow.elf`（另有同名 `.hex`；CLion 的构建输出目录） |

**引脚分配**

| 逻辑名 | 引脚 | 宏 | 模式 |
| --- | --- | --- | --- |
| LED0 | PB12 | `LED0_Pin` / `LED0_GPIO_Port` | 推挽输出、初始低、2 MHz |
| LED1 | PB13 | `LED1_Pin` / `LED1_GPIO_Port` | 同上 |
| LED2 | PB14 | `LED2_Pin` / `LED2_GPIO_Port` | 同上 |
| SWDIO | PA13 | — | Serial Wire |
| SWCLK | PA14 | — | Serial Wire |

**关键宏（`main.c`）**

| 宏 | 值 | 作用 |
| --- | --- | --- |
| `LED_FLOW_INTERVAL_MS` | `500U` | 切换间隔 |
| `LED_FLOW_COUNT` | `3U` | 灯的数量 |
| `LED_ON` | `GPIO_PIN_SET` | 亮 = 高电平 |
| `LED_OFF` | `GPIO_PIN_RESET` | 灭 = 低电平 |

**编译结果（实测）**

| 项 | 值 |
| --- | --- |
| 编译器 | arm-none-eabi-gcc 10.3.1 20210824 |
| 优化等级 | `-Og -g`（Debug） |
| FLASH 占用 | 5028 B / 64 KB（**7.67%**） |
| RAM 占用 | 2664 B / 20 KB（**13.01%**） |
| 结论 | 0 error，0 warning（`-Wall -Wextra`） |

**建议的 `.gitignore`**

```gitignore
# CLion
cmake-build-*/
.idea/

# 编译中间产物
*.o
*.d
*.su
*.elf
*.hex
*.bin
*.map
```

**工具链路径**

| 组件 | 路径 |
| --- | --- |
| STM32CubeMX | `D:\CubeMX\STM32CubeMX.exe`（6.15.0） |
| CLion | `D:\CLion\CLion 2026.2.2` |
| arm-gcc / g++ / gdb | `D:\CLion\CLion 2026.2.2\arm\bin\` |
| cmake | `D:\CLion\CLion 2026.2.2\bin\cmake\win\x64\bin\cmake.exe` |
| ninja | `D:\CLion\CLion 2026.2.2\bin\ninja\win\x64\ninja.exe` |
| OpenOCD | `D:\OpenOCD\bin\openocd.exe` |
| 板级配置 | `D:\_学习\RM电控\QQ群文件-电控组\enviroment\CFJ_STM32\stm32f1.cfg` |
| 固件包 | `C:\Users\LXB\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.7` |

---

## 结语：这套流程以后能复用

本次作业走通的这条链路——**CubeMX 配硬件 → 生成 → CLion 改 USER CODE → 编译 → OpenOCD 烧录**——是后面所有作业（OLED、按键、蜂鸣器、TB6612 电机、舵机、串口通信）的同一套骨架。区别只有两个：

1. **CubeMX 里多勾几个外设**（I2C1、TIMx PWM、USART1、ADC……）；
2. **`main.c` 里多写几个 `xxx_Poll()`**，然后在 `while(1)` 里排队调用。

尤其第 10.3 节的 `s_tickBase + 差值判断` 和 `LedFlow_Poll(nowMs)` 这种"每轮查一次、没到点立刻返回"的写法，是后面**多任务并行**的基础：按键消抖、OLED 刷新、蜂鸣器节奏、电机保护，全部可以写成同构的 `Poll(now)` 并在一个 `while(1)` 里轮流跑，不需要 RTOS 也能让它们互不阻塞。把这套骨架吃透，后面加功能就是"往表里加一行"的事。


