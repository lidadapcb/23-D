# DSP 模块快速参考

## 📚 文档导航

| 文档 | 用途 | 适合人群 |
|-----|------|---------|
| **README_CN.md** ← 你在这里 | 快速查找、概述 | 所有人 |
| **MYFFT_DETAILED_COMMENTS.md** | myFFT.c/h 详细注释 | 想深入理解代码结构 |
| **FFT_MODULE_GUIDE.md** | FFT 使用完整指南 | 想学会用 FFT 模块 |
| **AFL_FFT_COMMENTS.md** | Afl_FFT.c 详细注释 | 想理解底层算法 |

---

## 🚀 快速开始

### 初始化（启动时执行一次）
```c
InitTableFFT(16384);   // 初始化三角函数表
Init_window();         // 初始化窗函数
myFFT_init();          // 初始化 FFT 参数
HAL_TIM_Base_Start(&htim3);  // 启动 500kHz 采样触发
```

### 采集和处理（循环执行）
```c
// 在 DMA 完成中断回调中
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        float k = 3.3f / 4096.0f;
        FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
        
        // ✓ 结果在 ADC1_FFT[] 和 ADC1_Param.THD 中
        printf("THD: %.2f%%\r\n", ADC1_Param.THD);
        
        ADC_Update(1);  // 继续下一次采集
    }
}
```

---

## 🔧 核心参数

### ADC1_Param（全局参数结构体）

```c
CHx_DSP ADC1_Param;

// 主要成员
ADC1_Param.N                    = 16384;      // FFT 点数
ADC1_Param.SampFreq             = 500000;     // 采样率 (Hz)
ADC1_Param.adc_voltage_range    = 3.3f;       // ADC 参考电压 (V)
ADC1_Param.adc_resolution_ratio = 4096;       // ADC 分辨率 (12-bit)

// 运算结果
ADC1_Param.THD;      // 总谐波失真 (%)
ADC1_Param.DC;       // 直流分量 (V)
ADC1_Param.VPP;      // 峰峰值 (V)
```

### 数据缓冲区

| 缓冲区 | 大小 | 类型 | 用途 |
|-------|------|------|------|
| `ADC1_Val[]` | 16384×2B | uint16_t | 原始 ADC 采样（DMA 填充） |
| `ADC1_FFT[]` | 16384×8B | COMPX | FFT 结果（幅度谱） |
| `DSPfft_cache[]` | 8192×4B | float | ARM 库工作缓冲（1K-4K点用） |

---

## 📊 频率对应关系

```c
// 频点 i 对应的频率
frequency[i] = (i × 500000) / 16384 = i × 30.5 Hz

// 常见频率的索引
1 kHz   → bin 33
10 kHz  → bin 328
100 kHz → bin 3276
```

---

## ⚠️ 常见错误

| 错误 | 原因 | 解决方案 |
|-----|------|---------|
| FFT 点数非 2^n | 算法要求 | 改用 1024, 2048, 4096, 8192, 16384 |
| THD = -1 | 未检测到峰值 | 检查输入信号强度 |
| 内存溢出 | 缓冲区不足 | 减少 FFT 点数或配置外部 SRAM |
| 频率偏移 | 采样率配置错误 | 检查 TIM3 分频系数 |

---

## 🎯 常用操作

### 查询特定频率的幅度
```c
float get_mag_at_freq(float target_hz) {
    int bin = (target_hz * 16384) / 500000;
    if (bin >= 0 && bin < 8192) {
        return ADC1_FFT[bin].real;
    }
    return -1.0f;
}

float mag_1k = get_mag_at_freq(1000);
```

### 找出最强的频率分量
```c
float max_mag = 0;
int max_bin = 0;

for (int i = 0; i < 8192; i++) {
    if (ADC1_FFT[i].real > max_mag) {
        max_mag = ADC1_FFT[i].real;
        max_bin = i;
    }
}

float peak_freq = (max_bin * 500000) / 16384;
printf("最强分量: %.1f Hz, 幅度: %.4f\r\n", peak_freq, max_mag);
```

### 计算频谱占用带宽
```c
// 找出 3dB 下降点（-3dB = 0.707 × max_mag）
float threshold = max_mag * 0.707f;
int left_edge = max_bin, right_edge = max_bin;

while (left_edge > 0 && ADC1_FFT[left_edge].real > threshold)
    left_edge--;
while (right_edge < 8192 && ADC1_FFT[right_edge].real > threshold)
    right_edge++;

float bandwidth = ((right_edge - left_edge) * 500000) / 16384;
printf("信号带宽: %.1f Hz\r\n", bandwidth);
```

---

## 🔌 硬件配置检查清单

- [ ] ADC1 初始化（12-bit，3.3V 参考）
- [ ] DMA2 Stream0 配置（ADC1 → 内存）
- [ ] TIM3 设置为 500kHz（**采样率决定因素**）
- [ ] CCM RAM 可用（用于 FFT 结果存储）
- [ ] 16384 点 FFT 时需检查 SRAM 是否充足（160KB）

---

## ⏱️ 性能指标

| FFT 点数 | 处理时间 | 频率分辨率 |
|---------|---------|---------|
| 1024 | ~1ms | 488 Hz |
| 4096 | ~4.5ms | 122 Hz |
| 8192 | ~12.5ms | 61 Hz |
| 16384 | ~28ms | 30.5 Hz |

---

## 📡 信号处理流程

```
原始模拟信号
    ↓
ADC 采样 (500kHz)
    ↓ DMA 自动转移
ADC1_Val[] (原始数据)
    ↓
加窗处理 (汉宁窗)
    ↓
FFT 运算 (Cooley-Tukey)
    ↓
幅度计算 (√(real²+imag²))
    ↓
ADC1_FFT[] (幅度谱)
    ↓
峰值检测、THD 计算
    ↓
调制识别决策
    ↓
输出结果 (类型、参数)
```

---

## 💡 设计原理

### 为什么使用 FFT？
- **快速**：O(N log N) 而非 O(N²)
- **频域分析**：易于识别信号频率成分
- **调制识别**：不同调制类型有不同频谱特征

### 为什么加窗？
- 减少频谱泄露（Spectral Leakage）
- 降低旁瓣干扰
- 改善频率分辨率

### 为什么乘以 10.0f？
- 防止数值精度丢失
- 改善浮点运算动态范围
- 不影响最终结果（归一化时消除）

---

## 🐛 调试命令

```c
// 打印全部频谱
void dump_spectrum(void) {
    for (int i = 0; i < 256; i++) {
        printf("%d: %.4f\r\n", i, ADC1_FFT[i].real);
    }
}

// 检查内存使用
void check_memory(void) {
    printf("ADC1_Val: %d KB\r\n", 
           ADC1_MAX * sizeof(uint16_t) / 1024);
    printf("ADC1_FFT: %d KB\r\n", 
           ADC1_MAX * sizeof(COMPX) / 1024);
}

// 显示系统状态
void show_status(void) {
    printf("===== FFT 系统状态 =====\r\n");
    printf("FFT 点数: %d\r\n", ADC1_Param.N);
    printf("采样率: %d Hz\r\n", ADC1_Param.SampFreq);
    printf("频率分辨率: %.1f Hz\r\n", 
           500000.0f / ADC1_Param.N);
    printf("总谐波失真: %.2f%%\r\n", ADC1_Param.THD);
    printf("直流分量: %.3f V\r\n", ADC1_Param.DC);
}
```

---

## 📖 相关函数一览

| 函数 | 来源 | 功能 |
|-----|------|------|
| `myFFT_init()` | myFFT.c | 初始化 FFT 参数 |
| `FFT_convert()` | myFFT.c | 核心 FFT 处理 |
| `ADC_Update()` | myFFT.c | 启动 ADC 采集 |
| `InitTableFFT()` | Afl_FFT.c | 初始化三角函数表 |
| `Init_window()` | Afl_FFT.c | 初始化窗函数表 |
| `cfft()` | Afl_FFT.c | 8192+ 点 FFT 算法 |
| `Afl_fft_getpeak()` | 2023Diansai_DSP.c | 峰值检测 |
| `Afl_THD()` | 2023Diansai_DSP.c | THD 计算 |

---

## 🔄 工作流程时序

```
时间轴 (ms)
│
0ms    ├─ ADC_Update(1) 启动
│      │
~33ms  ├─ DMA 完成（16384 点 @ 500kHz）
│      │
       ├─ HAL_ADC_ConvCpltCallback() 触发
│      │
       ├─ FFT_convert() 执行 (~28ms)
│      │  ├─ 加窗处理
│      │  ├─ FFT 运算
│      │  ├─ 幅度计算
│      │  └─ 峰值检测
│      │
       ├─ 调制识别（在 2023Diansai_DSP.c）
│      │
56ms   ├─ ADC_Update(1) 重启
│      │
       ...（重复）
```

---

## 📌 配置修改指南

### 改变 FFT 点数为 8192
```c
// myFFT.h
#define ADC1_MAX 8192  // 改为 8192

// myFFT.c 中 myFFT_init()
ADC1_Param.N = 8192;  // 同步更新

// main.c
InitTableFFT(8192);   // 初始化表
```

### 改变采样率为 250kHz
```c
// myFFT.c 中 myFFT_init()
ADC1_Param.SampFreq = 250000;  // 改为 250kHz

// tim.c（由 STM32CubeMX 生成）
// 修改 TIM3 预分频器使得输出为 250kHz
// PSC 值应该加倍
```

### 使用不同的 ADC 参考电压
```c
// myFFT.c
float k = 2.5f / 4096.0f;  // 如果参考电压改为 2.5V
FFT_convert(ADC1_Val, ADC1_FFT, 16384, 1, k);
```

---

## ✅ 测试清单

- [ ] FFT 初始化完成，无硬错误
- [ ] DMA 能正常传输数据
- [ ] FFT 输入信号幅度正常（非全 0 或全 0xFFF）
- [ ] 输出频谱合理（有明显的峰值）
- [ ] THD 在合理范围（-1 表示异常）
- [ ] 多次运行结果稳定一致
- [ ] 内存未溢出（观察栈空间）

---

## 🎓 学习资源

- **FFT 基础**：Cooley-Tukey 算法，复数运算
- **窗函数**：汉宁窗、汉明窗的时频特性
- **调制识别**：各调制类型的频谱特征
- **DSP 优化**：ARM CMSIS-DSP 库使用

---

## 🆘 故障排查

**问题：FFT 全是 0**
- 检查 ADC1_Val[] 是否被 DMA 填充
- 确认采样触发（TIM3）是否启动
- 检查输入信号是否连接

**问题：频谱显示异常（噪声多）**
- 增加 FFT 点数以改善分辨率
- 检查输入信号 SNR
- 尝试不同的窗函数

**问题：内存不足**
- 减少 FFT 点数（4096 而非 16384）
- 使用外部 SRAM
- 清理其他不必要的全局数组

---

*快速参考版本：1.0  
最后更新：2024-04-24*

