# GEMINI.md

## Project Overview

这是一个基于 **STM32F407ZGTx** 微控制器的嵌入式项目，专门用于 **2023年全国大学生电子设计竞赛 (Diansai)**。该项目的核心功能是实现多种**信号调制方式的自动识别**及其参数测量。

### Main Technologies
- **Hardware Platform:** STM32F407 (Cortex-M4, 168MHz)
- **Framework:** STM32Cube HAL
- **DSP & Mathematics:** ARM CMSIS-DSP (用于 FFT 计算、峰值搜索、平方根等)
- **Modulation Recognition:** 支持识别 CW, AM, FM, NBFM, WBFM, 2ASK, 2FSK, 2PSK 等信号。
- **Peripherals:** ADC1 (信号采集), DMA (数据搬运), TIM3 (采样触发), USART1 (调试输出)。

### Architecture
1. **Signal Acquisition:** 通过 ADC1 和 DMA 采集原始信号。
2. **Frequency Domain Analysis:** 使用 FFT 将时域信号转换为频域。
3. **Feature Extraction:** 提取频域中的峰值、频率点、总谐波失真 (THD) 等特征。
4. **Decision Logic:** 根据特征（如峰值数量、THD 大小、边带分布）判断调制类型并计算参数（如调幅度 $m_a$、调频指数 $m_f$ 等）。

---

## Building and Running

### Prerequisites
- **VS Code** 配合 **Embedded IDE (EIDE)** 扩展。
- **ARM Compiler 5 (AC5)** (根据 `eide.yml` 配置)。
- **ST-Link** 调试器（或 J-Link，需在 EIDE 中切换）。

### Key Commands
该项目主要通过 VS Code 的 EIDE 插件界面进行操作，相关任务定义在 `.vscode/tasks.json` 中：

- **Build:** 在 EIDE 面板点击 "Build" 图标，或在 VS Code 任务中运行 `build`。
- **Flash:** 点击 "Upload" 图标，或运行 `flash` 任务将固件烧录到 STM32。
- **Clean:** 运行 `clean` 任务清理构建产物。

---

## Project Structure

- `Core/`: 包含 HAL 库生成的初始化代码和主程序逻辑。
- `Drivers/DSP/`: **核心算法目录**。
  - `myFFT.c/h`: FFT 算法及其参数管理器的核心封装。
  - `2023Diansai_DSP.c/h`: 调制识别决策逻辑和特征提取函数。
- `MDK-ARM/`: 包含 EIDE 配置文件 (`.eide/`) 和构建脚本。

---

## FFT Module Implementation (myFFT.c/h)

该模块封装了信号处理的全过程：数据采集 -> 加窗 -> FFT -> 幅度计算 -> 特征提取。

### 1. 核心数据结构: `CHx_DSP`
该结构体管理一个通道的所有 DSP 状态，包括采样率 (`SampFreq`)、点数 (`N`)、原始数据指针及计算出的参数（VPP, DC, THD）。

### 2. 500k 采样率 / 16384 点 FFT 使用指南
- **频率分辨率 ($\Delta f$):** $500\text{kHz} / 16384 \approx 30.5\text{Hz}$。这意味着 FFT 数组中索引为 $i$ 的元素对应的频率为 $i \times 30.5\text{Hz}$。
- **配置步骤:**
    1. 修改 `myFFT.h` 中的 `ADC1_MAX` 为 16384。
    2. 在 `myFFT_init` 中设置 `ADC1_Param.SampFreq = 500000` 且 `ADC1_Param.N = 16384`。
    3. 调用 `ADC_Update(1)` 启动采集。
    4. 调用 `FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k)` 进行转换。
- **读取结果:** 转换后，`ADC1_FFT[i].real` 存储该频率点的**幅度值**。

✦ 这两个文件是该项目的数字信号处理（DSP）核心封装层。它们不仅封装了 FFT 算法，还集成了 ADC
  数据采集、加窗处理、幅度计算以及谐波分析（THD）。

  以下是对这两个文件的详细解读，以及针对你需求的 500k 采样率、16384 点 FFT 的使用指南。

---

  一、 文件功能概述

  1. myFFT.h (定义与配置)
   * CHx_DSP 结构体：这是最关键的设计。它将一个通道的所有信息打包，包括采样点数 ($N$)、采样率 ($f_s$)、原始数据指针、FFT
     结果指针，以及计算出的 VPP、直流分量、THD 等。
   * 宏定义：
       * ADC0_MAX 和 ADC1_MAX：定义了采集缓冲区的最大长度。注意：代码中 ADC1_MAX 默认为 1024，如果你要跑 16384
         点，需要修改此宏。
   * COMPX 结构体：代表复数，包含 real (实部) 和 imag (虚部)。

  2. myFFT.c (算法实现)
   * 数据存储：
       * ADCx_Val：存储 ADC 采集的原始 12 位整数。
       * ADCx_FFT：存储 FFT 运算的复数结果。代码中使用 __attribute__ ((section(".sdram"))) 将其放在外部 SDRAM 中（因为
         16384 点 FFT 消耗内存较大）。
   * 核心函数 FFT_convert：
       * 分支处理：它根据点数 ($N$) 自动选择算法。
           * 1024~4096 点：使用 ARM 官方的 arm_cfft_radix4_f32 (基4浮点 FFT)，速度极快，且带加窗和 THD 自动计算。
           * 8192~16384 点：使用 cfft (安富莱实现的 FFT 算法)。
       * 幅度计算：运算结束后，它会调用 arm_sqrt_f32 计算每个频率点的模值，并将结果重新存回 fft_arr[i].real
         中。这意味着运算后，real 成员存储的是幅度谱。

---

  二、 针对你的需求 (500k 采样率, 16384 点) 的解读

  要在你的项目中使用 500k 采样率和 16384 点 FFT，你需要关注以下几个核心点：

  1. 频率分辨率 (Frequency Resolution)
   * 公式：$\Delta f = \frac{f_s}{N}$
   * 你的参数：$\Delta f = \frac{500,000\text{ Hz}}{16,384} \approx 30.517\text{ Hz}$
   * 解读：FFT 结果数组中，第 $i$ 个元素代表的频率为 $f = i \times 30.517\text{ Hz}$。

  2. 代码需要修改的地方
    由于代码默认是为 8.2M 采样率和较小点数设计的，你需要做以下调整：

   * 修改缓冲区大小 (myFFT.h)：

   1     #define ADC1_MAX 16384  // 必须改为 16384 或更大
   * 初始化参数 (myFFT.c 中的 myFFT_init)：

   1     ADC1_Param.N = 16384;
   2     ADC1_Param.SampFreq = 500000; // 设置为 500k
   * 校准系数 $k$：
      $k$ 用于将 ADC 原始值转换为电压。如果你的 ADC 范围是 0-3.3V，分辨率 12位（4096），则 $k \approx 3.3 / 4096$。

---

  三、 如何调用 FFT (使用步骤)

  你可以按照以下逻辑在 main.c 或任务循环中调用：

  第一步：初始化
  在程序启动时调用一次。

   1 myFFT_init();
   2 // 手动修正你的参数
   3 ADC1_Param.N = 16384;
   4 ADC1_Param.SampFreq = 500000;

  第二步：采集数据
  调用 ADC_Update(1)，它会触发 DMA 开始采集 16384 个点。你需要确保采样触发源（通常是定时器 TIM）的频率已经配置为
  500kHz。

   1 ADC_Update(1);
   2 // 实际应用中可能需要等待 DMA 完成标志位，或者在回调函数中处理

  第三步：执行转换
  调用 FFT_convert 处理数据。

   1 float k = 3.3f / 4096.0f; // 假设 ADC 参考电压 3.3V
   2 FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
   3 // 16384 点会自动走“安富莱算法”分支，mod 参数在此分支主要影响加窗

  第四步：读取结果
  转换完成后，ADC1_FFT 数组中就存好了幅度谱。
   * 直流分量：ADC1_FFT[0].real
   * 某个频率点的幅度：如果你想看 1kHz 附近的信号，计算索引 $i = 1000 / 30.517 \approx 33$。查看 ADC1_FFT[33].real
     即可。

---

  四、 重要提示

   1. 内存占用：16384 点 FFT 需要两个大型数组（原始数据 uint16_t 和复数结果 COMPX）。
       * ADC1_Val: $16384 \times 2\text{ bytes} = 32\text{ KB}$
       * ADC1_FFT: $16384 \times 8\text{ bytes} = 128\text{ KB}$
       * 总计约 160KB。STM32F407 的内部 SRAM 通常为 192KB，非常吃力。如果代码中配置了 SDRAM（如 myFFT.c
         所示），请确保你的硬件板载了外部 SDRAM 芯片并已初始化。如果没有外部内存，你需要取消 .sdram
         段定义，尝试将其挤入内部 SRAM。
   2. 采样频率控制：ADC_Update 只是启动 DMA，真正的采样率是由触发 ADC 的定时器决定的。你需要检查 tim.c 中触发 ADC
      的定时器频率是否为 500kHz。
   3. 镜像对称：FFT 结果是关于 $N/2$ 对称的。对于 16384 点，你只需要看 0 到 8191 个点即可（即 0 到 250kHz 的频率范围）。

### 3. 重要：内存与性能约束 (无外部 SRAM 方案)
**警告：16384 点 FFT 消耗极大内存。**
- `ADC1_Val` (uint16_t[16384]): 32KB
- `ADC1_FFT` (COMPX[16384]): 128KB (复数 8 字节/点)
- **总计: 160KB**。STM32F407 的内部 SRAM 总量为 192KB。
- **操作建议:** 
    1. **移除 SDRAM 绑定:** 在 `myFFT.c` 中，必须删除数组定义后的 `__attribute__ ((section(".sdram")))` 声明，否则在无 SRAM 的硬件上会因非法访问导致硬 HardFault。
    2. **缩减任务栈:** 由于 160KB 已占满大部分 SRAM，必须确保 RTOS 任务栈（如有）或其他全局变量不会导致内存溢出 (Stack Overflow)。
    3. **算法选择:** 16384 点将自动使用 `cfft` (安富莱实现)，不支持基4优化，计算耗时相对较长，请确保在实时性允许范围内使用。

---

## Development Conventions

- **HAL Support:** 用户代码应写在 `/* USER CODE BEGIN */` 和 `/* USER CODE END */` 块之间。
- **DSP Optimization:** 优先使用 `arm_math.h` 中提供的 CMSIS-DSP 函数以利用 Cortex-M4 的 FPU 性能。
- **Modulation Constants:** 调制类型识别结果通过 `Modem_Type` 结构体和预定义的宏（如 `_AM_`, `_FM_`）进行管理。
