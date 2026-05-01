# F407_23D 项目初始化文档

## 一、项目简介

本项目为 **2023年全国大学生电子设计竞赛 D 题** 参赛作品 —— **信号调制方式识别与参数估计装置**。

- **核心功能**：通过 ADC 对输入调制信号进行采样，经 FFT 频域分析后，自动识别调制方式（CW / AM / FM / 2ASK / 2FSK / 2PSK），并估计调制参数（调制频率、调幅系数 ma、最大频偏 Δf_max、码元速率 Rc、键控系数 h 等）。
- **MCU**：STM32F407ZGT6（ARM Cortex-M4F，168MHz，192KB SRAM，1MB Flash）
- **语言**：C（C99 标准）
- **构建工具**：VSCode + Embedded IDE (EIDE) + ARM Compiler 5 (AC5)

---

## 二、硬件资源

### 2.1 芯片

| 参数 | 值 |
|------|-----|
| 型号 | STM32F407ZGT6 |
| 内核 | Cortex-M4F（硬件 FPU） |
| 主频 | 168 MHz |
| SRAM | 192 KB（含 64KB CCM） |
| Flash | 1 MB |

### 2.2 外设配置一览

| 外设 | 引脚 | 功能 | 关键参数 |
|------|------|------|----------|
| ADC1 | PA1 (IN1) | 信号采样 | 12bit, 500kHz, DMA2_Stream0 |
| TIM3 | — | 采样触发时钟 | 500kHz TRGO, 时钟源 APB1=84MHz |
| DMA2_Stream0 | — | ADC数据搬运 | 普通模式, 半字宽度, 8192点缓冲区 |
| USART1 | PA9(TX) / PA10(RX) | 调试输出 (printf) | 115200bps |
| USART2 | PA2(TX) / PA3(RX) | 外部通信 | 中断接收 |
| GPIO | PC0~PC5, PB0~PB2, PF11~PF15 | AD9959 控制 | 位脉冲 SPI |

### 2.3 AD9959 DDS 引脚分配

| 引脚 | 功能 |
|------|------|
| PC0 | RESET (复位) |
| PC1 | CS (片选，高无效) |
| PC2 | SCLK (SPI时钟) |
| PC3 | IO_UPDATE (触发更新) |
| PC4 | SDIO0 (数据线0) |
| PC5 | PDC (电源关闭) |
| PB0 | PS0 (Profile选择0) |
| PB1 | PS1 |
| PB2 | PS2 |
| PF11 | PS3 |
| PF13 | SDIO1 |
| PF14 | SDIO2 |
| PF15 | SDIO3 |

### 2.4 时钟树

```
HSE 8MHz --> PLL (M=4, N=168, P=2)
  ├── SYSCLK = 168MHz
  │   ├── HCLK   = 168MHz
  │   ├── APB2   =  84MHz (TIM1/8/9/10/11 定时器时钟 = 168MHz)
  │   └── APB1   =  42MHz (TIM2/3/4/5/6/7 定时器时钟 =  84MHz)
  └── PLLQ = 84MHz (USB/随机数发生器)
```

---

## 三、项目目录结构

```
F407_23D/
├── Core/
│   ├── Inc/                    # 头文件
│   │   ├── main.h              # 主头文件（AD9959引脚宏、缓冲区声明）
│   │   ├── adc.h / dma.h / tim.h / usart.h / gpio.h
│   │   ├── stm32f4xx_hal_conf.h
│   │   └── stm32f4xx_it.h
│   └── Src/                    # 源文件（CubeMX生成）
│       ├── main.c              # 应用程序入口、主循环、ADC回调
│       ├── adc.c / dma.c / tim.c / usart.c / gpio.c
│       ├── stm32f4xx_hal_msp.c # HAL底层初始化
│       ├── stm32f4xx_it.c      # 中断服务程序
│       └── system_stm32f4xx.c  # 系统初始化
├── Drivers/
│   ├── AD9959/
│   │   ├── ad9959.h            # AD9959寄存器/通道枚举
│   │   └── ad9959.c            # AD9959驱动（初始化/频率/相位/幅度设置）
│   ├── DSP/                    # 数字信号处理模块
│   │   ├── myFFT.h / myFFT.c           # FFT处理核心(缓冲区/转换调度/THD)
│   │   ├── 2023Diansai_DSP.h / .c      # 调制识别决策树
│   │   ├── Afl_FFT.h / Afl_FFT.c       # 安富莱自定义FFT实现(cfft/窗函数)
│   │   ├── usart_Connect.h / .c        # 串口通信辅助
│   │   └── MY_Delay.h / .c / delay.h   # 延时函数
│   ├── CMSIS/                  # CMSIS核心 + DSP库
│   ├── STM32F4xx_HAL_Driver/   # HAL库
│   └── arm_cortexM4lf_math.lib # CMSIS-DSP预编译库
├── MDK-ARM/
│   ├── F407_23D.uvprojx        # Keil MDK工程（备用）
│   └── startup_stm32f407xx.s   # 启动文件
├── build/                      # EIDE构建输出
├── F407_23D.ioc                # CubeMX工程配置
├── GEMINI.md                   # 项目开发文档
└── D题_信号调制方式识别与参数估计装置.pdf  # 竞赛题目
```

---

## 四、开发环境搭建

### 4.1 所需软件

| 软件 | 版本 | 用途 |
|------|------|------|
| Visual Studio Code | 最新 | 代码编辑 |
| Embedded IDE (EIDE) | v3.26.7+ | 项目管理/构建 |
| ARM Compiler 5 (AC5) | — | 编译工具链（Keil MDK 自带） |
| STM32CubeMX | 6.17.0+ | 外设/引脚配置 |

### 4.2 安装步骤

1. 安装 VSCode，并安装 **Embedded IDE (EIDE)** 扩展
2. 安装 Keil MDK（ARM Compiler 5），默认路径 `d:\keil32\ARM\ARMCC`
3. 使用 VSCode 打开项目根目录 `D:\stm32_project\F407_23D`
4. EIDE 会自动识别 `.eide/eide.yml` 中的工程配置
5. 构建参数位于 `build/F407_23D/builder.params`

### 4.3 编译配置

| 参数 | 值 |
|------|-----|
| 编译器 | ARMCC v5 (AC5) |
| 优化 | -O3 |
| C标准 | C99 |
| 标准库 | microLIB |
| FPU | Cortex-M4 SP (单精度) |
| 预定义宏 | USE_HAL_DRIVER, STM32F407xx, ARM_MATH_CM4 |
| 堆大小 | 512 B |
| 栈大小 | 1024 B |

---

## 五、软件架构

### 5.1 主循环流程

```
while(1):
  1. ADC_Update(1)              → 启动 DMA 采集 8192 点
  2. 等待 adc_cplt_flag == 1   → DMA完成中断置位
  3. HAL_ADC_Stop_DMA(&hadc1)  → 停止ADC DMA
  4. FFT_convert(...)           → 执行8192点FFT + 幅度谱 + 峰值检测 + THD
  5. Modulation_Judge(...)      → 调制方式识别
  6. printf(...)                → USART1输出结果
  7. HAL_Delay(500)             → 延时500ms
```

### 5.2 ADC 采样参数

| 参数 | 值 |
|------|-----|
| 采样率 | 500 kHz（TIM3 TRGO） |
| 分辨率 | 12 位 |
| 采样点数 | 8192 |
| FFT 分辨率 | ~61 Hz/bin |
| 触发方式 | TIM3 上升沿触发，DMA 普通模式 |

### 5.3 FFT 处理流水线

```
ADC原始数据[8192]
  → DC偏移消除（动态均值计算）
  → Hanning窗 × 10倍增益
  → 8192点 FFT（安富莱 cfft）
  → 幅度谱计算（sqrt(real² + imag²)）
  → 峰值检测（分组扫描 + 局部极大值验证 + 有效值验证）
  → 软件带通滤波（73kHz ~ 128kHz）
  → THD 计算
```

### 5.4 调制识别决策树

```
输入: 峰值列表
  ├── 0个峰值 → _ERR_
  ├── 1个峰值 → CW（连续波）
  ├── 模拟调制判定（AM/FM）
  │   ├── 校验频率对称性（误差 < 150Hz）
  │   ├── 计算调制频率 fm
  │   ├── fm ≤ 5500Hz？
  │   │   ├── 是 → ma < 1.1？→ AM / FM
  │   │   └── 否 → 进入数字调制分支
  └── 数字调制判定（2FSK）
      └── ΔF ≥ 4000Hz？→ 2FSK（计算Rc和h）
```

---

## 六、关键全局变量

| 变量名 | 位置 | 类型 | 说明 |
|--------|------|------|------|
| `ADC1_Val[8192]` | myFFT.c | uint16_t | ADC原始采样数据缓冲区 |
| `ADC1_FFT[8192]` | myFFT.c (CCM) | float | FFT幅度谱结果（64KB CCM） |
| `adc_cplt_flag` | main.c | uint8_t | DMA传输完成标志 |
| `ADC1_Peak_ARR[]` | 2023Diansai_DSP.h | _PEAK | 峰值检测结果数组 |
| `Modem_ARR[]` | 2023Diansai_DSP.h | _MODEM | 调制识别结果数组 |
| `THD_ARR[]` | myFFT.h | _THD | 谐波失真计算结果 |

---

## 七、输出格式

通过 USART1 以 115200bps 输出，格式示例：

```
=================================
调制方式: AM
调制频率 F: 1000.0 Hz
调幅系数 ma: 0.500
=================================
```

---

## 八、常见问题

1. **编译错误找不到 arm_math.h**：确认 `Drivers/CMSIS/Include` 已添加到 include path

2. **链接错误 arm_cfft_radix4_f32 未定义**：确认 `Drivers/arm_cortexM4lf_math.lib` 已添加到链接器输入

3. **FFT 结果全为零**：检查 ADC 输入引脚 PA1 是否有信号，TIM3 是否正确输出 500kHz TRGO

4. **识别率低**：
   - 确认输入信号幅度在 ADC 量程内（0~3.3V）
   - 检查载波频率是否在 73kHz~128kHz 带通范围内
   - 可调整 `fft_osc_filter()` 中的起始/结束索引和底噪门限

5. **内存不足**：8192点 FFT 已接近 192KB SRAM 极限，不要尝试增大采样点数

---

## 九、参考文档

| 文档 | 路径 |
|------|------|
| 竞赛题目 | `D题_信号调制方式识别与参数估计装置.pdf` |
| 开发文档 | `GEMINI.md` |
| CubeMX工程 | `F407_23D.ioc` |
| AD9959 驱动 | `Drivers/AD9959/ad9959.h` / `.c` |
| FFT 实现详解 | `Drivers/DSP/README_CN.md` |

---

*最后更新: 2026-05-01*
