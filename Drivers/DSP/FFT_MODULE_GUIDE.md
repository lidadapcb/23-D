# FFT 模块使用完整指南

## 📋 目录
1. [模块概述](#模块概述)
2. [文件结构](#文件结构)
3. [数据流图](#数据流图)
4. [初始化步骤](#初始化步骤)
5. [典型使用流程](#典型使用流程)
6. [参数配置](#参数配置)
7. [常见问题](#常见问题)

---

## 模块概述

### 核心功能
FFT 模块实现了**快速傅里叶变换**，将**时域信号**转换为**频域表示**，用于：
- 信号频谱分析
- 调制类型识别（AM、FM、PSK、FSK 等）
- 总谐波失真（THD）计算
- 信号参数测量

### 支持的规模
| FFT 点数 | 采样率 | 频率分辨率 | 处理时间(ms) | 使用库 |
|---------|-------|----------|------------|-------|
| 1024 | 500kHz | 488 Hz | ~1.0 | ARM CMSIS-DSP |
| 2048 | 500kHz | 244 Hz | ~2.2 | ARM CMSIS-DSP |
| 4096 | 500kHz | 122 Hz | ~4.5 | ARM CMSIS-DSP |
| 8192 | 500kHz | 61 Hz | ~12.5 | 安富莱 cfft |
| 16384 | 500kHz | 30.5 Hz | ~28 | 安富莱 cfft |

### 系统架构
```
┌─────────────────────────────────────────────┐
│     信号输入（模拟量）                       │
│           ↓                                  │
│     ┌───────────────┐                       │
│     │ ADC (12-bit)  │ ← 由 TIM3 触发        │
│     └───────┬───────┘  500kHz 采样率        │
│             ↓                               │
│     ┌────────────────┐                      │
│     │ DMA 转移       │ ADC 原始数据          │
│     │ ADC1_Val[]     │ (uint16_t[16384])    │
│     └────────┬───────┘                      │
│              ↓                              │
│     ╔════════════════════════════════════╗  │
│     ║  FFT_convert() 处理                ║  │
│     ║  • 加窗处理                        ║  │
│     ║  • 数据转换（raw → float）         ║  │
│     ║  • FFT 运算                        ║  │
│     ║  • 幅度计算                        ║  │
│     ║  • 峰值检测                        ║  │
│     ║  • THD 计算                        ║  │
│     ╚════════════════┬═══════════════════╝  │
│                      ↓                      │
│             ADC1_FFT[] - 幅度谱             │
│            (复数数组，real=幅度)            │
│                      ↓                      │
│     ┌─────────────────────────────────┐    │
│     │  调制识别决策逻辑                │    │
│     │  (2023Diansai_DSP.c)            │    │
│     │  根据频域特征判断调制类型        │    │
│     └─────────────────────────────────┘    │
│                      ↓                      │
│     输出：调制类型、参数（m_a、m_f等）      │
└─────────────────────────────────────────────┘
```

---

## 文件结构

### 文件依赖关系
```
Core/main.c
    ↓
    ├─→ Drivers/DSP/myFFT.c  [核心接口]
    │   ├─ FFT_convert()         [FFT 主函数]
    │   ├─ ADC_Update()          [启动采集]
    │   ├─ myFFT_init()          [初始化]
    │   ├─ DSPfft_cache_arr[]    [工作缓冲]
    │   └─ ADC1_Val[], ADC1_FFT[] [数据缓冲]
    │
    ├─→ Drivers/DSP/myFFT.h  [头文件]
    │   ├─ CHx_DSP              [DSP 参数结构体]
    │   ├─ COMPX                [复数结构体]
    │   └─ ADC1_MAX             [缓冲区大小宏]
    │
    ├─→ Drivers/DSP/Afl_FFT.c [底层实现]
    │   ├─ cfft()               [8192+ 点 FFT]
    │   ├─ InitTableFFT()       [表初始化]
    │   ├─ Init_window()        [窗函数初始化]
    │   ├─ sintab[]、costab[]   [三角函数表]
    │   └─ windowedSignal_*K[]  [窗函数表]
    │
    ├─→ Drivers/DSP/2023Diansai_DSP.c [特征提取]
    │   ├─ Afl_fft_getpeak()    [峰值检测]
    │   ├─ Afl_THD()            [THD 计算]
    │   └─ ADC1_Peak_ARR[]      [峰值数组]
    │
    └─→ HAL库 (stm32f407xx_hal_adc.c)
        └─ HAL_ADC_Start_DMA() [ADC-DMA 控制]
```

### 关键宏定义 (myFFT.h)
```c
#define ADC1_MAX        16384        // FFT 点数
#define ADC_RESOLUTION  4096         // 12bit ADC 分辨率
#define ADC_VREF        3.3f         // ADC 参考电压 3.3V
```

---

## 数据流图

### 详细的数据变换过程

#### 阶段 1：ADC 采集 → 原始缓冲
```
ADC 采样流程（硬件自动）：
TIM3 计时器（500kHz 触发）
    ↓ 每 2μs 触发一次
ADC1 进行 12-bit 转换
    ↓ 转换结果范围 0~4095
DMA 自动搬入 ADC1_Val[]
    ↓ 经过 16384 次转移后触发中断

结果：ADC1_Val[0..16383] 中存储 32.768ms 的采样数据
```

#### 阶段 2：加窗 + 单位转换
```
对于每个采样点 i：
───────────────────────────────────────
原始值：        adc_val[i]  (0~4095)
    ↓
窗函数应用：    adc_val[i] × window[i]
    ↓
单位转换：      × k           (k = 3.3/4096 ≈ 0.000806)
    ↓           结果范围 0~3.3V
幅度扩大：      × 10.0f       (改善精度)
    ↓
缓冲存储：      DSPfft_cache[(i-1)*2] 

结果：复数缓冲区（实部=已处理数据，虚部=0）
```

#### 阶段 3：FFT 变换
```
输入：
  DSPfft_cache[] 或 ADC1_FFT[] (时域复数信号)

FFT 运算：
  时域 {x[0], x[1], ..., x[N-1]}
    ↓ 位反序
  重排序列
    ↓ 蝶形运算 × log₂(N) 级
  逐级计算
    ↓
输出：频域复数
  {X[0], X[1], ..., X[N-1]}
  
其中 X[k] = Σ x[n]×e^(-j2πnk/N)
```

#### 阶段 4：幅度计算
```
对每个频点 k：
───────────────────────────────────
复数：       X[k] = real[k] + j×imag[k]
    ↓
模值计算：   |X[k]| = √(real[k]² + imag[k]²)
    ↓
存储结果：   ADC1_FFT[k].real = |X[k]|  (覆盖原值)
            ADC1_FFT[k].imag = 0（清零）

结果：幅度谱（频域单边谱，只关心 0~N/2）
```

#### 阶段 5：特征提取
```
幅度谱：ADC1_FFT[0..N/2]
    ↓
搜索峰值：Afl_fft_getpeak()
    ↓ 返回峰值个数和位置
峰值数组：ADC1_Peak_ARR[]
    ↓
计算 THD：Afl_THD()
    ↓ 基频能量 vs 谐波能量
结果参数：ADC1_Param.THD
```

---

## 初始化步骤

### Step 1: 程序启动时的初始化

```c
// main.c 中的初始化流程
void main(void) {
    // 硬件初始化（由 STM32CubeMX 生成）
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();      // ADC 初始化
    MX_TIM3_Init();      // 500kHz 采样触发器
    MX_USART1_Init();    // 调试输出
    
    // ========== FFT 模块初始化 ==========
    
    // 1️⃣ 初始化三角函数表
    InitTableFFT(8192);  // 或 16384（如果硬件支持）
    
    // 2️⃣ 初始化窗函数表
    Init_window();
    
    // 3️⃣ 初始化 FFT 参数
    myFFT_init();
    
    // 4️⃣ 启动定时器 - TIM3 开始产生 500kHz 触发信号
    HAL_TIM_Base_Start(&htim3);
    
    printf("FFT 模块初始化完成\r\n");
    
    // ========== 主循环 ==========
    while (1) {
        // ... 其他处理
    }
}
```

### Step 2: 运行时的采集流程

```c
// DMA 完成中断处理函数（在 stm32f407xx_it.c 中）
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_adc1);
}

// ADC DMA 完成回调
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        // ✓ 采集完成，ADC1_Val[] 中已填满 16384 个数据点
        
        // 计算 ADC 转换系数
        float k = ADC1_Param.adc_voltage_range / ADC1_Param.adc_resolution_ratio;
        
        // 执行 FFT 变换
        FFT_convert(ADC1_Val,     // 输入原始数据
                    ADC1_FFT,     // 输出 FFT 结果
                    ADC1_Param.N, // FFT 点数
                    1,            // 窗函数模式（1=汉宁窗）
                    k);           // 单位转换系数
        
        // ✓ FFT 完成，ADC1_FFT[] 现在包含幅度谱
        // ✓ ADC1_Param.THD 已填入
        
        // 在这里进行调制识别或其他处理
        recognize_modulation();
        
        // 重新启动采集（采集下一个 32.768ms 的数据块）
        ADC_Update(1);
    }
}
```

---

## 典型使用流程

### 完整示例代码

```c
// 全局变量
uint32_t fft_count = 0;      // 采集块计数
float latest_thd = 0;         // 最后一次 THD

void run_fft_analysis(void) {
    // 前置条件：已调用 myFFT_init()、InitTableFFT()、Init_window()
    
    // 1. 启动首次采集
    ADC_Update(1);
    printf("第 %d 块采集开始...\r\n", ++fft_count);
    
    // 2. 等待 DMA 完成（在中断中自动处理）
    // 中断触发 → FFT_convert() 执行 → 回调函数处理
    
    // 3. 在回调中获取结果
    // 如需在主程序中访问结果，使用信号量同步
}

// DMA 完成后的结果访问
void process_fft_results(void) {
    // ✓ FFT 结果已保存在 ADC1_FFT[] 中
    
    printf("=== FFT 分析结果 #%d ===\r\n", fft_count);
    
    // 显示前 5 个峰值
    printf("检测到 %d 个峰值\r\n", peak_count);
    for (int i = 0; i < peak_count && i < 5; i++) {
        float freq = ADC1_Peak_ARR[i].freq;  // 频率
        float mag  = ADC1_Peak_ARR[i].mag;   // 幅度
        printf("  峰值 %d：%.1f Hz，幅度 %.2f\r\n", i, freq, mag);
    }
    
    // 显示总谐波失真
    printf("THD = %.2f%%\r\n", ADC1_Param.THD);
    latest_thd = ADC1_Param.THD;
    
    // 显示部分频谱数据
    printf("前 16 个频点幅度：\r\n");
    for (int i = 0; i < 16; i++) {
        printf("  [%2d] %.4f\r\n", i, ADC1_FFT[i].real);
    }
}
```

---

## 参数配置

### ADC1_Param 结构体成员

| 成员 | 类型 | 默认值 | 说明 |
|-----|------|-------|------|
| `N` | uint32_t | 16384 | FFT 点数（必须是 2^n） |
| `SampFreq` | uint32_t | 500000 | 采样频率（Hz） |
| `adc_arr` | uint16_t* | ADC1_Val | ADC 原始数据指针 |
| `FFT_arr` | COMPX* | ADC1_FFT | FFT 结果指针 |
| `adc_resolution_ratio` | uint16_t | 4096 | ADC 分辨率（12bit=4096） |
| `adc_voltage_range` | float | 3.3 | ADC 参考电压（V） |
| `VPP` | float | 0 | 峰峰值（V） |
| `mean_value` | float | 0 | 平均值 |
| `DC` | float | 0 | 直流分量（V） |
| `THD` | float | 0 | 总谐波失真（%） |

### 常用配置变更

#### 🔧 改变采样率
```c
// 从 500kHz 改为 250kHz
ADC1_Param.SampFreq = 250000;
// 需要同时修改 TIM3 预分频器
// 具体见 tim.c 中的 TIM3 配置
```

#### 🔧 改变 FFT 点数
```c
// 如果改为 8192 点（更好的频率分辨率）
#define ADC1_MAX 8192  // 在 myFFT.h 中修改

// 重新编译并重新初始化
ADC1_Param.N = 8192;
InitTableFFT(8192);
```

#### 🔧 调整 ADC 参考电压（不同硬件可能不同）
```c
// 如果硬件参考电压不是 3.3V
ADC1_Param.adc_voltage_range = 2.5f;  // 改为 2.5V

// 或在 FFT_convert() 调用时调整 k 值
float k = 2.5f / 4096.0f;  // 2.5V 参考
FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
```

---

## 常见问题

### Q1: FFT 点数为什么必须是 2 的幂？

**A:** FFT 基于 Cooley-Tukey 分治算法，将一个 N 点 DFT 分解为两个 N/2 点 DFT：
```
X[k] = Σ(n even) + Σ(n odd)
     = E[k] + W^k × O[k]

只有当 N = 2^m 时才能递归进行，否则无法进行分治。
```

### Q2: THD = -1 表示什么？

**A:** THD = -1 是错误标记，表示：
- 未检测到有效的频域峰值
- 输入信号可能太弱或全是噪声
- Afl_fft_getpeak() 的阈值参数需调整

**排查方法**：
```c
printf("峰值个数：%d\n", peak_count);    // 应该 > 0
printf("第一峰幅度：%.2f\n", ADC1_Peak_ARR[0].mag);
printf("ADC DC 分量：%.2f\n", ADC1_Param.DC);  // 检查是否全 0
```

### Q3: FFT 的幅度值如何转换为电压？

**A:** FFT 输出的幅度值已经通过加窗处理过，需要反向计算：
```c
// ADC1_FFT[k].real 是加窗后的 FFT 结果
// 反推电压信号：

float magnitude = ADC1_FFT[k].real;

// 去掉扩大系数 10.0
magnitude /= 10.0f;

// 转换为电压（乘以 k 的反数）
float voltage = magnitude * 4096 / ADC1_Param.adc_voltage_range;

// 对于单频信号，幅度应该乘以 2（单边到双边） 
// 但这取决于后续处理的具体需求
```

### Q4: 如何提高频率分辨率？

**A:** 频率分辨率 = 采样率 / FFT 点数

有两种方法：
```
方法 1：增加 FFT 点数（推荐）
  Δf = 500k / 16384 = 30.5 Hz  ✓ 更好
  vs
  Δf = 500k / 4096 = 122 Hz

方法 2：降低采样率（不推荐）
  需要修改 TIM3 分频，可能错过高频分量
```

### Q5: 内存不足时的应对方案

**A:** F407 内部 SRAM 只有 192KB，16384 点 FFT 需要 128KB：

| 方案 | 优点 | 缺点 |
|-----|------|------|
| **外接 SDRAM** | 支持 16384 点，频率分辨率最高 | 硬件成本 |
| **改用 8192 点** | 内存足够（64KB），仍有不错分辨率 | 频率分辨率 ~61Hz |
| **改用 4096 点** | 充足内存，处理最快 | 频率分辨率 ~122Hz，可能无法分辨近频率 |

### Q6: FFT 结果的频率与索引的对应关系？

**A:** 
```c
频率 = (索引 × 采样率) / FFT点数
     = (i × 500000) / 16384
     = i × 30.5 Hz

例如：
  ADC1_FFT[33].real  对应  33 × 30.5 ≈ 1007 Hz
  ADC1_FFT[66].real  对应  66 × 30.5 ≈ 2013 Hz
  
// 对于实信号，只看前 N/2 个频点（0 到 250kHz）
```

### Q7: 如何调试 FFT 是否正确工作？

**A:** 验证步骤：

```c
// Step 1: 生成已知频率的测试信号
void test_with_known_signal(void) {
    float test_freq = 1000;  // 1kHz 正弦波
    float amplitude = 1.0f;   // 1V 幅度
    
    for (int i = 0; i < 16384; i++) {
        float angle = 2 * M_PI * test_freq * i / 500000;
        // 转换为 ADC 值（0~4095）
        ADC1_Val[i] = (uint16_t)(
            (amplitude / 3.3f) * 4096 * 0.5f + 
            2048 + 2048 * sin(angle)
        );
    }
}

// Step 2: 手动执行 FFT
float k = 3.3f / 4096.0f;
FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);

// Step 3: 检查结果
int expected_bin = (1000 * 16384) / 500000;  // ≈ 33
printf("预期峰值在索引 %d\r\n", expected_bin);
printf("实际值 ADC1_FFT[%d] = %.2f\r\n", expected_bin, 
       ADC1_FFT[expected_bin].real);

// Step 4: 可视化前 256 个频点
for (int i = 0; i < 256; i++) {
    printf("%3d: %.4f\r\n", i, ADC1_FFT[i].real);
}
```

---

## 性能优化建议

### 1. 运算时间优化
```c
// 使用更小的 FFT 点数（如果可接受频率分辨率）
4096 点 FFT:  4.5ms  ✓ 较快
vs
8192 点 FFT:  12.5ms  ✗ 较慢
```

### 2. 内存优化
```c
// 删除不必要的全局数组
extern float freamp[50];  // 如果不用，注释掉

// 使用栈上的局部数组（对小规模数据）
float small_buffer[256];  // OK
float large_buffer[16384];  // ✗ 栈溢出风险
```

### 3. 功耗优化
```c
// 只在需要时执行 FFT，其他时间关闭 ADC
ADC_Update(1);      // 启动采集
while (!dma_done);  // 等待完成
FFT_convert(...);   // 快速处理
HAL_ADC_Stop_DMA(&hadc1);  // 关闭 ADC 降低功耗
```

---

*最后更新：2024-04-24*
*基于 STM32F407 + CMSIS-DSP 实现*
