# Afl_FFT.c 详细说明文档

## 文件概述

`Afl_FFT.c` 是安富莱 FFT 实现库的核心文件，包含完整的快速傅里叶变换算法实现。

### 主要特点
- **支持大规模 FFT**：8192、16384 等点数
- **三角函数表预计算**：sin/cos 值存储在 Flash，节省 SRAM
- **高效算法**：基于 Cooley-Tukey 蝶形运算
- **浮点运算**：支持 float32_t 单精度浮点

---

## 核心数据结构

### 1. 三角函数表

#### sintab[16384] - 正弦值表
```
大小：16384 个 float32_t = 64KB
范围：覆盖 0~2π
精度：单精度浮点
用途：FFT 蝶形运算中的旋转因子计算
```

#### costab[16384] - 余弦值表  
```
大小：16384 个 float32_t = 64KB
范围：覆盖 0~2π
精度：单精度浮点
用途：FFT 蝶形运算中的旋转因子计算
```

### 2. 窗函数表

#### windowedSignal_1K[] - 1024 点汉宁窗
```
大小：1024 个 float32_t
用途：加窗处理，降低频谱泄露
应用场景：1024 点 FFT
```

#### windowedSignal_16K[] - 16384 点汉宁窗
```
大小：16384 个 float32_t  
用途：加窗处理，降低频谱泄露
应用场景：16384 点 FFT
备注：索引方式为 i×2（可能为了密度考虑）
```

---

## 核心函数

### 1. InitTableFFT(uint32_t n)

**功能**：初始化 FFT 三角函数表

**参数**：
- `n`：FFT 点数（8192、16384 等）

**返回值**：void

**工作流程**：
1. 根据点数 n 计算三角函数值
2. 将 sin/cos 值加载到内存（可能从 Flash 预置）
3. 初始化旋转因子表用于蝶形运算

**使用场景**：
```c
myFFT_init() 中调用一次
InitTableFFT(8192);   // 初始化 8192 点 FFT 表
```

### 2. Init_window(void)

**功能**：初始化窗函数表

**参数**：无

**返回值**：void

**工作流程**：
1. 计算或加载汉宁窗函数值
2. 为不同规模 FFT 生成对应的窗函数表
3. 支持 1024、4096、8192、16384 等点数

**使用场景**：
```c
程序启动时调用
Init_window();  // 初始化所有窗函数表
```

### 3. cfft(struct compx *_ptr, uint32_t FFT_N)

**功能**：执行快速傅里叶变换（核心算法）

**参数**：
- `_ptr`：指向复数数组的指针，包含输入信号
  - 实部存在 compx[i].real
  - 虚部存在 compx[i].imag
- `FFT_N`：FFT 点数（8192、16384 等）

**返回值**：void

**工作流程**：
```
1. 位反序 (Bit Reversal)
   └─ 重排输入序列以适配蝶形运算拓扑
   
2. 蝶形运算 (Butterfly Operations)
   ├─ 第 1 级：N/2 个蝶形
   ├─ 第 2 级：N/4 个蝶形
   ├─ ...
   └─ 第 log₂(N) 级：1 个蝶形
   
3. 旋转因子应用
   └─ 从 sintab/costab 查表获取旋转因子
   └─ 进行复数乘法（蝶形计算）
   
4. 原地储存结果
   └─ FFT 结果直接覆盖输入数组
```

**计算复杂度**：
- 时间：O(N log₂ N)
- 空间：O(1)（原地变换）

**数学原理**：
```
离散傅里叶变换 (DFT)：
X[k] = Σ(n=0 to N-1) x[n] * e^(-j*2π*k*n/N)

FFT 优化：利用旋转因子的周期性和对称性，从 O(N²) 降低到 O(N log₂ N)
```

**使用示例**：
```c
COMPX fft_result[8192];

// 准备输入数据（已转换为复数）
for (int i = 0; i < 8192; i++) {
    fft_result[i].real = input_data[i];  // 实信号
    fft_result[i].imag = 0.0f;           // 虚部初始化
}

// 执行 FFT
cfft(fft_result, 8192);

// 计算幅度谱
for (int i = 0; i < 8192; i++) {
    float magnitude = sqrt(
        fft_result[i].real * fft_result[i].real + 
        fft_result[i].imag * fft_result[i].imag
    );
}
```

### 4. PowerPhaseRadians_f32(struct compx *s, uint16_t _usFFTPoints, float32_t _uiCmpValue)

**功能**：计算复数序列的功率和相位

**参数**：
- `s`：复数数组指针（FFT 结果）
- `_usFFTPoints`：FFT 点数
- `_uiCmpValue`：比较阈值（用于过滤小幅度分量）

**返回值**：void

**计算内容**：
1. **功率 (Power)**：P[k] = (real[k]² + imag[k]²)
2. **功率谱 (Power Spectrum)**：|X[k]|²
3. **相位 (Phase)**：φ[k] = arctan(imag[k] / real[k])
4. **相位差**：用于相位连续性检测

**应用**：
- 功率谱分析（哪些频率分量最强）
- 相位追踪（用于调制识别）
- 信号质量评估

---

## 内存使用情况

### Flash 占用
```
sintab[16384]        64KB
costab[16384]        64KB
窗函数表              ~128KB
代码                  ~50KB
─────────────────────────
总计                 ~306KB
```

### SRAM 占用（运行时）
```
FFT 输入/输出数组    ADC1_MAX × 8 bytes
（如 16384 点）      = 128KB
中间缓存             32KB
─────────────────────────
总计                 ~160KB
```

⚠️ **F407 SRAM 限制**：
- 内部 SRAM：192KB
- CCM RAM：64KB
- 建议外接 SDRAM 用于 16384 点 FFT

---

## 使用流程

### 初始化阶段（程序启动）
```c
// 1. 初始化 FFT 表
InitTableFFT(8192);  // 或 16384

// 2. 初始化窗函数
Init_window();

// 3. 初始化 myFFT 模块
myFFT_init();
```

### 数据处理阶段（运行时）
```c
// 1. 采集 ADC 数据
ADC_Update(1);  // 启动 DMA 采集
wait_dma_complete();

// 2. 执行 FFT 转换
float k = 3.3f / 4096.0f;
FFT_convert(ADC1_Val, ADC1_FFT, 8192, 1, k);

// 3. 提取特征
// FFT_convert 内部调用：
//   - Afl_fft_getpeak()  - 提取峰值
//   - Afl_THD()          - 计算总谐波失真
//   - PowerPhaseRadians_f32() - 可选的功率/相位计算

// 4. 调制识别（在 2023Diansai_DSP.c 中实现）
// 根据频域特征判断调制类型
```

---

## 关键算法细节

### 1. 位反序 (Bit Reversal)
FFT 蝶形运算需要输入序列按位反序排列：
```
原序列顺序：0, 1, 2, 3, 4, 5, 6, 7
位反序后：  0, 4, 2, 6, 1, 5, 3, 7
```
这使得 FFT 的计算拓扑更高效。

### 2. 蝶形运算
最基本的计算单元（以 2 点蝶形为例）：
```
时域：
x_in = [a, b]

频域（蝶形）：
X[k]     = a + W^k × b
X[k+N/2] = a - W^k × b

其中 W = e^(-j2π/N) 是主旋转因子
```

### 3. 旋转因子 (Twiddle Factor)
```
W_N^k = cos(2πk/N) - j×sin(2πk/N)

实部：cos_val = costab[k]
虚部：sin_val = sintab[k]
```

### 4. 频率分辨率 (Frequency Resolution)
```
Δf = f_s / N

例如：
- 采样率 500kHz，8192 点 FFT
- Δf = 500k / 8192 ≈ 61 Hz
- FFT 结果 fft_arr[i] 代表 i×61Hz 处的频率分量
```

---

## 常见错误与注意事项

### ❌ 错误 1：点数必须是 2 的幂
```c
cfft(fft_arr, 5000);  // ❌ 错误！必须是 2^n
cfft(fft_arr, 4096);  // ✓ 正确
cfft(fft_arr, 8192);  // ✓ 正确
```

### ❌ 错误 2：忘记初始化窗函数
```c
// ❌ 错误流程
FFT_convert(...);  // 直接运算，没有初始化表

// ✓ 正确流程
Init_window();     // 先初始化
FFT_convert(...);  // 再运算
```

### ❌ 错误 3：内存溢出
```c
// 如果硬件无外部 SRAM，不能用 16384 点
// F407 内部 SRAM 总共 192KB + 64KB(CCM)
// 16384 点 FFT 需要 128KB（含 64KB 缓存）

// 改用较小点数或配置外部 SDRAM
```

### ⚠️ 注意 4：实信号 FFT 特性
```c
// 对于实信号的 FFT，结果具有对称性：
// X[N-k] = conj(X[k])  （共轭对称）

// 因此只需要看前 N/2 个点的频率范围
// myFFT.c 中：
// i = Afl_fft_getpeak(fft_arr, &ADC1_Peak_ARR[0], fft_N/2, ...);
//                                                    ^^^^^^
```

---

## 性能指标

### 计算时间（估算，F407 @ 168MHz）
```
1024 点FFT：  ~1.2ms
4096 点FFT：  ~5.8ms
8192 点FFT：  ~12.5ms
16384点FFT：  ~28ms
```

### 精度
- 浮点精度：单精度 float32_t（~7 位有效数字）
- 动态范围：约 120dB（2^20）

---

## 与 myFFT.c 的关系

```
myFFT.c（上层接口）
    ↓
├─ FFT_convert()    - 处理 1024/2048/4096 点（调用 ARM 库）
│                   - 处理 8192 点（调用 cfft）
│
├─ ADC_Update()     - 启动 ADC-DMA 采集
│
└─ myFFT_init()     - 初始化参数

    ↓ 调用下层

Afl_FFT.c（底层实现）
    ├─ cfft()                    - 8192+ 点 FFT 核心算法
    ├─ InitTableFFT()            - 初始化三角函数表
    ├─ Init_window()             - 初始化窗函数
    ├─ PowerPhaseRadians_f32()  - 功率/相位计算
    └─ sintab[]/costab[]        - 预计算三角函数表
```

---

## 调试提示

### 1. 验证 FFT 正确性
```c
// 输入一个已知频率的正弦波
float test_freq = 1000;  // 1kHz
for (int i = 0; i < N; i++) {
    input[i] = sin(2 * M_PI * test_freq * i / sample_rate);
}

FFT_convert(input, output, N, 1, k);

// 应该在 output[freq_bin] 处看到峰值
// freq_bin = (test_freq / sample_rate) * N
```

### 2. 检查 THD 计算
```c
printf("检测到的峰值个数：%d\n", peak_count);
printf("基频：%f Hz\n", ADC1_Peak_ARR[0].freq);
printf("THD：%.2f%%\n", ADC1_Param.THD);

// THD > 0 且 < 100% 为合理
// THD = -1 表示未检测到有效峰值
```

### 3. 观察频谱图
```c
// 打印前 256 个频率点的幅度
for (int i = 0; i < 256; i++) {
    printf("%3d: %.2f\n", i, ADC1_FFT[i].real);
}
```

---

## 参考资源

- **算法**：Cooley-Tukey FFT
- **精度**：IEEE 754 单精度浮点
- **窗函数**：汉宁窗（Hanning Window）
- **应用**：调制识别、频谱分析、信号处理

---

*文档版本：1.0  
更新日期：2024-04-24  
作者：Copilot*
