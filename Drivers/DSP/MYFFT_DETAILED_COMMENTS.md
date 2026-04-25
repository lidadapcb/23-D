# myFFT.h & myFFT.c 详细注释指南

## 📄 文件总览

### myFFT.h - 头文件和宏定义
存储所有的数据结构定义、常数配置和函数声明。

### myFFT.c - 实现文件
包含 FFT 处理的核心逻辑，是应用层与硬件 FFT 库之间的桥梁。

---

## 一、核心数据结构（在 myFFT.h 中）

### 1️⃣ COMPX 结构体 - 复数表示

```c
struct compx {
    float32_t real;  // 实部（Real part）
    float32_t imag;  // 虚部（Imaginary part）
};

typedef struct compx COMPX;
```

**用途**：表示复数 z = real + j×imag

**应用场景**：
- FFT 输入：加窗后的实信号以复数形式表示（虚部=0）
- FFT 输出：频域复数结果 X[k] = real[k] + j×imag[k]
- 幅度计算：|X[k]| = √(real² + imag²)

**示例**：
```c
COMPX c1 = {3.0f, 4.0f};      // 3 + 4j
float magnitude = sqrt(3*3 + 4*4);  // = 5
float phase = atan2(4.0f, 3.0f);    // ≈ 0.927 弧度
```

---

### 2️⃣ CHx_DSP 结构体 - DSP 参数管理器

```c
struct CHx_DSP {
    uint32_t N;                      // FFT 点数
    uint32_t SampFreq;               // 采样频率 (Hz)
    uint16_t* adc_arr;               // 指向 ADC 原始数据数组
    COMPX*    FFT_arr;               // 指向 FFT 结果数组
    uint16_t adc_resolution_ratio;   // ADC 分辨率（2^bits）
    float32_t adc_voltage_range;     // ADC 参考电压（V）
    
    // 计算结果（由 FFT 处理后更新）
    float32_t VPP;        // 峰峰值 (Voltage Peak-to-Peak)
    float32_t mean_value; // 平均值
    float32_t DC;         // 直流分量 (DC offset)
    float32_t THD;        // 总谐波失真 (Total Harmonic Distortion, %)
};

typedef struct CHx_DSP CHx_DSP;
```

**成员详解**：

| 成员 | 类型 | 单位 | 说明 | 初始值 |
|-----|------|------|------|--------|
| `N` | uint32_t | - | FFT 点数，必须 2^n | 16384 |
| `SampFreq` | uint32_t | Hz | 采样频率 | 500000 |
| `adc_arr` | uint16_t* | - | 指向原始 ADC 数据 | &ADC1_Val[0] |
| `FFT_arr` | COMPX* | - | 指向 FFT 结果 | &ADC1_FFT[0] |
| `adc_resolution_ratio` | uint16_t | - | 分辨率等级 | 4096 |
| `adc_voltage_range` | float32_t | V | 参考电压 | 3.3 |
| `VPP` | float32_t | V | 信号峰峰值 | 0 |
| `mean_value` | float32_t | V | 平均电压 | 0 |
| `DC` | float32_t | V | 直流偏置 | 0 |
| `THD` | float32_t | % | 谐波失真度 | 0 |

**计算公式**：
```
ADC 转换系数 k = adc_voltage_range / adc_resolution_ratio
               = 3.3 / 4096 ≈ 0.000806 V/step

电压值 = ADC_raw_value × k

直流分量 = 平均值（信号的中心电压）
峰峰值 = 最大值 - 最小值
THD = (√(E2² + E3² + ... + En²)) / E1 × 100%
      (2~n 次谐波能量 vs 基频能量)
```

**初始化示例**：
```c
void myFFT_init(void) {
    ADC1_Param.N                    = 16384;      // 16384 点 FFT
    ADC1_Param.SampFreq             = 500000;     // 500kHz 采样
    ADC1_Param.adc_arr              = ADC1_Val;   // 指向采集缓冲
    ADC1_Param.FFT_arr              = ADC1_FFT;   // 指向结果缓冲
    ADC1_Param.adc_resolution_ratio = 4096;       // 12-bit ADC
    ADC1_Param.adc_voltage_range    = 3.3f;       // 3.3V 参考
    ADC1_Param.VPP    = 0;
    ADC1_Param.DC     = 0;
    ADC1_Param.THD    = 0;
}
```

---

## 二、全局缓冲区详解（在 myFFT.c 中）

### 1️⃣ ADC1_Val[] - 原始采样缓冲

```c
uint16_t ADC1_Val[ADC1_MAX];
```

**属性**：
- **大小**：ADC1_MAX × 2 字节（如 16384 点 = 32KB）
- **类型**：无符号 16 位整数
- **值范围**：0 ~ 4095（12-bit ADC）
- **所有者**：DMA 控制器自动写入
- **位置**：主 SRAM（非 CCM）

**生命周期**：
```
1. ADC_Update(1) 启动
   ↓
2. DMA 自动填充 ADC1_Val[] - 需要 32.768ms（16384点@500kHz）
   ↓
3. DMA 完成中断触发
   ↓
4. 调用 FFT_convert() 处理数据
   ↓
5. 重新启动采集 ADC_Update(1)
```

**数据示例**：
```
ADC1_Val[0]    = 2045   (对应 2.1V)
ADC1_Val[1]    = 2156   (对应 2.25V)
...
ADC1_Val[16383]= 1932   (对应 1.96V)

转换：V = ADC_value × 3.3 / 4096
```

---

### 2️⃣ ADC1_FFT[] - FFT 结果缓冲

```c
COMPX ADC1_FFT[ADC1_MAX] __attribute__((section(".ccmram")));
```

**属性**：
- **大小**：ADC1_MAX × 8 字节（如 16384 点 = 128KB）
- **类型**：复数数组（每个元素 8 字节）
- **所有者**：FFT 运算引擎填充
- **位置**：CCM RAM（Cortex-M4 缓存相关内存）
- **生命周期**：FFT_convert() 中被覆盖

**数据含义**（运算前后不同）：

**运算前**（FFT_convert 开始）：
```c
ADC1_FFT[i].real = 经过加窗处理的模拟值（浮点）
ADC1_FFT[i].imag = 0.0f（虚信号设为 0）
```

**运算后**（FFT_convert 完成）：
```c
ADC1_FFT[i].real = 第 i 频点的幅度值 = √(real² + imag²)
ADC1_FFT[i].imag = 0（被清零，不再需要）

注意：只需关注 0 ~ N/2 范围内的点（实信号的共轭对称性）
```

**频率对应关系**：
```c
频点 i 对应的频率：freq[i] = (i × f_s) / N
                            = (i × 500000) / 16384
                            = i × 30.5 Hz

例如：
  i = 33   →  freq ≈ 1007 Hz    （1kHz 正弦波）
  i = 66   →  freq ≈ 2013 Hz    （2kHz 正弦波）
  i = 100  →  freq ≈ 3050 Hz    （3kHz 正弦波）
```

**示例**：
```c
// 打印频谱数据
for (int i = 0; i < 256; i++) {
    float frequency = (i * 500000) / 16384;
    float magnitude = ADC1_FFT[i].real;
    printf("%.1f Hz: %.4f\r\n", frequency, magnitude);
}
```

---

### 3️⃣ DSPfft_cache_arr[] - FFT 工作缓冲

```c
float DSPfft_cache_arr[4096 * 2];  // 8192 个浮点数 = 32KB
float* DSPfft_cache = DSPfft_cache_arr;
```

**属性**：
- **大小**：8192 个 float32_t = 32KB
- **用途**：ARM CMSIS-DSP 的 FFT 函数工作空间
- **位置**：主 SRAM
- **访问**：仅在 1024/2048/4096 点 FFT 时使用
- **格式**：交错存储 [real0, imag0, real1, imag1, ...]

**工作流程**：
```
1. 输入准备：
   for (i = 0; i < N; i++) {
       DSPfft_cache[2*i]   = windowed_real[i];
       DSPfft_cache[2*i+1] = 0.0f;
   }

2. FFT 运算：
   arm_cfft_radix4_f32(&fft_inst, DSPfft_cache);
   // 原地修改 DSPfft_cache

3. 结果提取：
   for (i = 0; i < N; i++) {
       real = DSPfft_cache[2*i];
       imag = DSPfft_cache[2*i+1];
       magnitude = sqrt(real*real + imag*imag);
       ADC1_FFT[i].real = magnitude;
   }
```

**注意**：
- 不支持 8192 点（此时使用 cfft() 函数，直接操作 ADC1_FFT[]）
- 只有 4096 点 FFT 需要此缓冲区
- 8192+ 点 FFT 绕过此缓冲区

---

### 4️⃣ freamp[] - 频率分量数组（可选）

```c
float freamp[50];
```

**属性**：
- **大小**：50 个浮点数
- **用途**：存储检测到的主要频率分量
- **使用**：可选（项目中可能未充分使用）

---

## 三、函数详解

### 函数 1：myFFT_init()

```c
void myFFT_init(void)
```

**功能**：一次性初始化所有 FFT 参数

**调用时机**：程序启动时（在 main 中），仅调用一次

**工作流程**：
```c
{
    // 设置 FFT 点数
    ADC1_Param.N = ADC1_MAX;  // 从宏定义读取（16384）
    
    // 设置采样参数
    ADC1_Param.SampFreq = 500000;  // 500 kHz
    
    // 绑定缓冲区指针
    ADC1_Param.adc_arr = ADC1_Val;   // 原始数据
    ADC1_Param.FFT_arr = ADC1_FFT;   // FFT 结果
    
    // 配置 ADC 硬件参数
    ADC1_Param.adc_resolution_ratio = 4096;  // 12-bit = 2^12
    ADC1_Param.adc_voltage_range = 3.3f;     // 3.3V
    
    // 清零结果字段
    ADC1_Param.VPP = 0;
    ADC1_Param.mean_value = 0;
    ADC1_Param.DC = 0;
    ADC1_Param.THD = 0;
}
```

**前置条件**：
- 无特殊要求

**后置条件**：
- ADC1_Param 所有字段已初始化
- 可以开始采集和 FFT 处理

**关键常数**：
```
ADC1_MAX        = 16384  (或 4096、8192，取决于硬件)
500000          = 500kHz 采样率（由 TIM3 控制）
4096            = 2^12（12-bit ADC 分辨率）
3.3             = 3.3V 参考电压
```

**使用示例**：
```c
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_DMA_Init();
    MX_TIM3_Init();
    
    // ✓ 初始化 FFT 模块
    myFFT_init();
    
    printf("FFT 初始化完成\r\n");
    printf("FFT 点数: %d\r\n", ADC1_Param.N);
    printf("采样率: %d Hz\r\n", ADC1_Param.SampFreq);
    
    // 其他初始化...
    
    while (1) {
        // 主程序循环
    }
}
```

---

### 函数 2：FFT_convert()

```c
void FFT_convert(uint16_t adc_arr[], 
                 COMPX fft_arr[], 
                 u16 fft_N, 
                 u8 mod, 
                 float32_t k)
```

**功能**：FFT 变换的核心处理函数

**参数详解**：

| 参数 | 类型 | 说明 | 示例 |
|-----|------|------|------|
| `adc_arr[]` | uint16_t* | 输入的 ADC 原始采样数据 | ADC1_Val |
| `fft_arr[]` | COMPX* | 输出的 FFT 结果数组 | ADC1_FFT |
| `fft_N` | uint16_t | FFT 点数（2^n） | 4096 or 8192 |
| `mod` | uint8_t | 窗函数模式 | 1 = 汉宁窗 |
| `k` | float32_t | ADC 转换系数 | 3.3/4096 ≈ 0.000806 |

**调用时机**：
- 在 ADC DMA 完成中断回调中
- 或在主程序中等待 DMA 完成后手动调用

**工作流程**（对于 4096 点 FFT）：

```
输入：ADC1_Val[] (uint16_t 类型，范围 0~4095)
 ↓
[阶段 1] 数据预处理 - 加窗转换
  ├─ 消除直流偏置（减去 2048）
  ├─ 应用窗函数（汉宁窗）
  ├─ 转换为浮点（乘以 k × 10）
  └─ 存入 DSPfft_cache[] (real + 0j)
 ↓
[阶段 2] FFT 运算
  ├─ arm_cfft_radix4_init_f32() - 初始化 FFT 实例
  └─ arm_cfft_radix4_f32() - 执行蝶形运算
 ↓
[阶段 3] 幅度计算
  ├─ 对每个频点计算模值
  │  magnitude[k] = √(real[k]² + imag[k]²)
  └─ 结果存入 ADC1_FFT[k].real
 ↓
[阶段 4] 特征提取
  ├─ Afl_fft_getpeak() - 检测频域峰值
  └─ Afl_THD() - 计算总谐波失真
 ↓
输出：
  • ADC1_FFT[] (幅度谱)
  • ADC1_Param.THD (总谐波失真 %)
  • ADC1_Peak_ARR[] (峰值列表)
```

**计算系数 k**：
```c
k = 参考电压 / ADC 分辨率
  = 3.3V / 4096
  = 0.0008056640625

// 或者在函数外部计算后传入
float k = 3.3f / 4096.0f;
FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
```

**窗函数参数 mod**：
```
mod = 1  : 汉宁窗（Hanning Window）
          权重 = 0.5 - 0.5×cos(2π×n/N)
          优点：边瓣抑制好，频率泄露小
          缺点：主瓣宽度稍大

// 其他窗函数（未在代码中实现）
// mod = 0  : 矩形窗（无加窗）
// mod = 2  : 汉明窗
// mod = 3  : 黑曼窗
```

**详细处理步骤（4096 点示例）**：

#### 步骤 1：初始化与清零
```c
// 清空峰值数组
memset(ADC1_Peak_ARR, 0, sizeof(ADC1_Peak_ARR));

// 初始化 ARM FFT 实例
arm_cfft_radix4_instance_f32 fft_inst;
arm_cfft_radix4_init_f32(&fft_inst, 4096, 0, 1);
// 参数：(实例指针, 点数, IFFT标志, 正向FFT标志)
```

#### 步骤 2：数据预处理
```c
// 对每个采样点进行加窗处理
for (i = 4095; i > 0; i--) {  // 从后向前处理（性能考虑）
    // 原始值 -> 浮点值
    float adc_raw = (float)adc_arr[i-1];
    
    // 乘以窗函数
    float windowed = adc_raw * windowedSignal_1K[i-1];
    
    // 转换为电压（乘以 k）
    float voltage = windowed * k;
    
    // 扩大以改善精度
    DSPfft_cache[(i-1)*2]   = voltage * 10.0f;  // 实部
    DSPfft_cache[(i-1)*2+1] = 0.0f;             // 虚部（实信号）
}
```

#### 步骤 3：FFT 运算
```c
// 执行快速傅里叶变换
arm_cfft_radix4_f32(&fft_inst, DSPfft_cache);

// DSPfft_cache 现在包含频域复数结果
// 格式：[real0, imag0, real1, imag1, ..., real4095, imag4095]
```

#### 步骤 4：幅度计算
```c
for (i = 0; i < 4096; i++) {
    float real = DSPfft_cache[2*i];
    float imag = DSPfft_cache[2*i+1];
    
    // 计算模值（幅度谱）
    float magnitude = sqrt(real*real + imag*imag);
    
    // 存入结果数组
    fft_arr[i].real = magnitude;
    fft_arr[i].imag = 0.0f;
}
```

#### 步骤 5：特征提取
```c
// 检测频域峰值（寻找频谱中的突出点）
int peak_count = Afl_fft_getpeak(
    fft_arr,            // FFT 结果
    &ADC1_Peak_ARR[0],  // 输出峰值列表
    2048,               // 搜索范围（N/2）
    5, 5, 300           // 参数：最小宽度、间距、阈值
);

// 计算总谐波失真
if (peak_count > 0) {
    Afl_THD(ADC1_Peak_ARR, &ADC1_Param);
} else {
    ADC1_Param.THD = -1.0f;  // 错误标记
}
```

**8192 点 FFT 的差异**：
```c
// 8192 点使用不同算法（cfft）
else if (fft_N == 8192) {
    // 预处理时消除 DC（2048 是 12-bit 的中点）
    for (i = 0; i < 8192; i++) {
        float ac_val = (float)adc_arr[i] - 2048.0f;
        fft_arr[i].real = ac_val * windowedSignal_16K[i*2] * k * 10.0f;
        fft_arr[i].imag = 0.0f;
    }
    
    // 直接调用安富莱 cfft 函数
    cfft(fft_arr, 8192);  // 原地变换
}
```

**常见错误**：
```c
// ❌ 错误：点数不是 2 的幂
FFT_convert(adc_arr, fft_arr, 5000, 1, k);

// ✓ 正确
FFT_convert(adc_arr, fft_arr, 4096, 1, k);
FFT_convert(adc_arr, fft_arr, 8192, 1, k);
```

---

### 函数 3：ADC_Update()

```c
void ADC_Update(u8 ADC_N)
```

**功能**：启动 ADC 和 DMA 开始采集

**参数**：
- `ADC_N` = 1 表示 ADC1

**调用流程**：
```
主程序              HAL库            硬件
    ↓                ↓
ADC_Update(1)
    ↓                ↓
            HAL_ADC_Start_DMA()
                      ↓
                    DMA Controller
                      ↓ 每 2μs（@500kHz）
                    ADC 采集一个点
                      ↓
                    DMA 写入 ADC1_Val[]
                      ↓ 16384 次后
                    DMA 完成中断
                              ↓
                          IRQHandler
                              ↓
                        HAL_ADC_ConvCpltCallback()
                              ↓
                            FFT_convert()
```

**代码**：
```c
void ADC_Update(u8 ADC_N) {
    if (ADC_N == 1) {
        // 启动 ADC1 DMA 模式
        // 参数：(ADC 句柄, 目标地址, 数据个数)
        HAL_ADC_Start_DMA(&hadc1, 
                          (uint32_t *)ADC1_Val,  // 数据存储地址
                          ADC1_MAX);              // 采集点数（16384）
    }
}
```

**采集时间**：
```
采集时间 = 点数 / 采样率
        = 16384 / 500000
        = 32.768 ms
        = 约 33ms
```

**中断处理**：
```c
// 在 stm32f407xx_it.c 中
void DMA2_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_adc1);  // DMA 中断处理
}

// 在 main.c 中的回调
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        // ✓ ADC1_Val[] 已填满
        
        // 立即处理 FFT
        float k = 3.3f / 4096.0f;
        FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
        
        // ✓ 结果已保存在 ADC1_FFT[]
        
        // 可选：重新启动下一次采集
        ADC_Update(1);
    }
}
```

**重要细节**：
- F407 无 L1 缓存，注释掉了 `SCB_InvalidateDCache_by_Addr` 调用
- DMA 与 ADC 硬件自动同步，不需要轮询
- 推荐在中断回调中立即处理，避免数据被覆盖

---

## 四、典型代码示例

### 示例 1：最小化配置

```c
int main(void) {
    // 硬件初始化
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_DMA_Init();
    MX_TIM3_Init();
    MX_USART1_Init();
    
    // FFT 模块初始化
    InitTableFFT(16384);  // Afl_FFT.c
    Init_window();        // Afl_FFT.c
    myFFT_init();         // myFFT.c
    
    // 启动定时器触发器
    HAL_TIM_Base_Start(&htim3);
    
    printf("系统启动\r\n");
    
    // 主循环
    while (1) {
        // 其他任务...
    }
}

// DMA 完成回调
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        // FFT 处理
        float k = ADC1_Param.adc_voltage_range / 
                  ADC1_Param.adc_resolution_ratio;
        FFT_convert(ADC1_Val, ADC1_FFT, 
                    ADC1_Param.N, 1, k);
        
        // 结果处理
        printf("THD: %.2f%%\r\n", ADC1_Param.THD);
        
        // 重启采集
        ADC_Update(1);
    }
}
```

### 示例 2：频谱显示

```c
void print_spectrum(void) {
    printf("=== 频谱显示（前 128 个频点）===\r\n");
    printf("索引\t频率(Hz)\t幅度\r\n");
    printf("─────────────────────\r\n");
    
    for (int i = 0; i < 128; i++) {
        float freq = (i * 500000) / 16384;  // 频率
        float mag = ADC1_FFT[i].real;       // 幅度
        
        printf("%3d\t%.1f\t%.4f\r\n", i, freq, mag);
    }
}
```

### 示例 3：特定频率查询

```c
float get_amplitude_at_frequency(float target_freq) {
    // 计算应该查看的 FFT 索引
    int bin = (target_freq * 16384) / 500000;
    
    // 检查索引范围
    if (bin >= 0 && bin < 8192) {
        return ADC1_FFT[bin].real;
    }
    
    return -1.0f;  // 超出范围
}

// 使用
float amp_1khz = get_amplitude_at_frequency(1000);
printf("1kHz 处的幅度: %.4f\r\n", amp_1khz);
```

---

## 五、关键概念

### 1. 频率分辨率 (Frequency Resolution)

```
Δf = 采样率 / FFT 点数

例如：
  500kHz / 4096 = 122 Hz    ← 频率分辨率
  500kHz / 16384 = 30.5 Hz  ← 更高精度
```

**含义**：相邻两个 FFT 频点的间隔

### 2. 加窗处理 (Windowing)

```
目的：降低频谱泄露（Spectral Leakage）
原因：FFT 假设信号是周期的，但实际采样段不是整数周期
      导致信号在 FFT 频点处产生失真

汉宁窗：w[n] = 0.5 - 0.5×cos(2π×n/N)
        给定序列两端赋予较小权重，中间权重较大
```

### 3. 实信号的对称性

```
对于实信号的 FFT 结果有对称性：
X[N-k] = X*[k]  （共轭对称）

因此只需看前 N/2 个频点
剩余的是冗余信息
```

---

## 六、调试技巧

### 技巧 1：验证 FFT 工作正常

```c
// 生成 1kHz 正弦波测试信号
for (int i = 0; i < 16384; i++) {
    float t = (float)i / 500000;
    ADC1_Val[i] = (uint16_t)(
        2048 + 1024 * sin(2 * M_PI * 1000 * t)
    );
}

// 执行 FFT
float k = 3.3f / 4096.0f;
FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);

// 检查结果
int expected_bin = (1000 * 16384) / 500000;  // ≈ 33
printf("预期峰值在索引 %d\r\n", expected_bin);
printf("实际幅度: %.4f\r\n", ADC1_FFT[33].real);
```

### 技巧 2：内存使用检查

```c
// 计算内存占用
printf("内存使用统计\r\n");
printf("ADC1_Val: %d bytes\r\n", ADC1_MAX * sizeof(uint16_t));
printf("ADC1_FFT: %d bytes\r\n", ADC1_MAX * sizeof(COMPX));
printf("DSPfft_cache: %d bytes\r\n", 4096 * 2 * sizeof(float));
printf("总计: %d KB\r\n", 
       (ADC1_MAX * (sizeof(uint16_t) + sizeof(COMPX)) + 
        4096 * 2 * sizeof(float)) / 1024);
```

---

*文档完成日期：2024-04-24*
