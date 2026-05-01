#include "myFFT.h"
#include "stdio.h"
#include "2023Diansai_DSP.h"

/**
 * ================================================================
 * 模块说明：FFT 数据处理模块 (myFFT.c)
 * 功能描述：
 *   1. 管理 ADC 采样数据的存储
 *   2. 执行 FFT 变换（支持 1024/2048/4096/8192 点）
 *   3. 计算幅度谱、直流分量和总谐波失真 (THD)
 *   4. 控制 ADC-DMA 数据采集流程
 * ================================================================
 */

// 频率幅值数组 - 存储检测到的频率分量的幅度值（最多 50 个）
float freamp[50];	

/* ==================== 全局缓冲区定义 ==================== */

/**
 * ADC1_Val[ADC1_MAX]
 * - 功能：存储 ADC1 采集的原始 12 位采样数据
 * - 类型：uint16_t 数组，分辨率 0-4095
 * - 大小：取决于 ADC1_MAX 宏定义（如 1024/4096/16384）
 * - 位置：保留在主 SRAM 中（DMA 直接写入）
 * - 说明：由 DMA 控制器自动转移 ADC 采集的原始数据，不进行任何转换
 */
uint16_t ADC1_Val[ADC1_MAX];	

/**
 * ADC1_FFT[ADC1_MAX]
 * - 功能：存储 FFT 转换后的复数结果和幅度谱
 * - 类型：COMPX 数组（复数结构体，包含 real 和 imag）
 * - 大小：ADC1_MAX × 8 字节（每个复数 4字节实部 + 4字节虚部）
 * - 位置：放在 CCM RAM 中以加速访问
 * - 说明：
 *   • FFT 运算后，real 成员存储该频率点的幅度值（取模后结果）
 *   • 对于 16384 点 FFT，需要外部 SDRAM 支持（如果硬件无 SDRAM 需要删除 .sdram 标记）
 * 
 * ⚠️ 重要提示：F407 的 CCM RAM 只有 64KB
 *   - 16384 点 FFT 结果数组需要 128KB（远超 CCM 大小）
 *   - 无外部 SRAM 情况下，应改用 @attribute((section(".bss"))) 放入主 SRAM
 *   - 详见 GEMINI.md 内存约束章节
 */
COMPX ADC1_FFT[ADC1_MAX] __attribute__((section(".ccmram"))); 

/**
 * ADC1_Param
 * - 功能：管理 ADC1 通道的所有 DSP 参数
 * - 成员：采样点数、采样率、ADC 分辨率、输入电压范围、计算结果（VPP、DC、THD）等
 * - 说明：初始化通过 myFFT_init() 完成，运算结果在此结构体中累积更新
 */
CHx_DSP ADC1_Param;


/**
 * myFFT_init()
 * ================================================================
 * 功能：初始化 FFT 处理模块的所有参数
 * 调用时机：程序启动阶段，FFT 运算前必须调用一次
 * 
 * 初始化内容：
 *   1. 设置 FFT 点数（N）
 *   2. 设置采样频率（SampFreq）
 *   3. 绑定数据缓冲区指针
 *   4. 配置 ADC 硬件参数（分辨率、参考电压）
 *   5. 清零所有计算结果（VPP、DC、THD）
 * 
 * 参数：void
 * 返回：void
 * ================================================================
 */
void myFFT_init(void)
{
    ADC1_Param.N                    = ADC1_MAX;         // FFT 点数 = 缓冲区大小
    ADC1_Param.SampFreq             = 500000;           // 采样频率：500kHz
    ADC1_Param.adc_arr              = ADC1_Val;         // 原始采样数据指针
    ADC1_Param.FFT_arr              = ADC1_FFT;         // FFT 结果数组指针
    ADC1_Param.adc_resolution_ratio = 4096;             // ADC 分辨率：12 bit = 4096 个刻度
    ADC1_Param.adc_voltage_range    = 3.3;              // ADC 参考电压：3.3V
    ADC1_Param.VPP                  = 0;                // 初始化：峰峰值
    ADC1_Param.mean_value           = 0;                // 初始化：平均值
    ADC1_Param.DC                   = 0;                // 初始化：直流分量
    ADC1_Param.THD                  = 0;                // 初始化：总谐波失真
}



/**
 * DSPfft_cache_arr 和 DSPfft_cache
 * ================================================================
 * 功能：FFT 运算中间缓存
 * 
 * DSPfft_cache_arr[4096 * 2]
 * - 大小：4096 × 2 = 8192 个浮点数 = 32KB
 * - 用途：ARM 官方库 arm_cfft_radix4_f32 的工作缓冲区
 * - 格式：交错存储，偶数索引存实部，奇数索引存虚部
 * - 位置：主 SRAM（足够大，不需要 CCM）
 * 
 * DSPfft_cache (指针)
 * - 指向 DSPfft_cache_arr[0]，用于 FFT 运算输入输出
 * 
 * ⚠️ 重要：该缓存仅支持 1024/2048/4096 点的 ARM Radix-4 算法
 *    8192/16384 点使用 cfft() 函数，不依赖此缓存
 * ================================================================
 */
float DSPfft_cache_arr[4096 * 2];   // 32KB 缓存
float* DSPfft_cache = DSPfft_cache_arr;

/**
 * FFT_convert()
 * ================================================================
 * 功能：执行快速傅里叶变换，支持多种点数和算法选择
 * 
 * 参数：
 *   @adc_arr[]:   输入的 ADC 原始采样数据数组（12bit 整数）
 *   @fft_arr[]:   输出的 FFT 结果数组（复数类型）
 *   @fft_N:       FFT 点数（支持 1024/2048/4096/8192）
 *   @mod:         窗函数模式（1=汉宁窗）
 *   @k:           ADC 转压转换系数 = (参考电压 / 4096)
 *                 例：3.3V 的 ADC → k = 3.3 / 4096 ≈ 0.000806
 * 
 * 返回值：void
 * 
 * 算法选择策略：
 *   • 1024/2048/4096 点：使用 ARM Radix-4 浮点快速算法
 *     - 优点：计算快速，支持硬件 FPU，自动计算 THD
 *     - 使用 DSPfft_cache 中间缓冲区
 *   
 *   • 8192 点：使用安富莱 cfft() 算法
 *     - 支持大规模 FFT，结果直接存入 fft_arr
 *     - 点数 8192 需要较长运算时间
 * 
 * 处理流程（以 4096 点为例）：
 *   1. 初始化 ARM FFT 实例
 *   2. 将原始 ADC 数据加窗处理并转换为浮点复数
 *     • 实部 = ADC_val × 窗函数 × k × 10.0f
 *     • 虚部 = 0.0f（实信号）
 *   3. 调用 arm_cfft_radix4_f32 执行 FFT 运算
 *   4. 计算幅度谱（模值 = √(real² + imag²)）
 *   5. 调用 Afl_fft_getpeak 提取频域峰值
 *   6. 调用 Afl_THD 计算总谐波失真
 * 
 * ⚠️ 重要细节：
 *   • 乘以 10.0f 是为了扩大信号幅度以改善数值精度
 *   • 8192 点时，使用 windowedSignal_16K 窗，索引需要 ×2
 *   • 运算结束后 THD 值存储在 ADC1_Param.THD 中
 * ================================================================
 */
void FFT_convert(uint16_t adc_arr[], COMPX fft_arr[], u16 fft_N, u8 mod, float32_t k)
{
    int32_t i;
    arm_cfft_radix4_instance_f32 fft_inst;

    // ==================== 分支 1：小规模 FFT (1024~4096 点) ====================
    if((fft_N == 1024) || (fft_N == 2048) || (fft_N == 4096))
    {
        // 清空峰值检测数组
        memset(ADC1_Peak_ARR, 0, sizeof(ADC1_Peak_ARR));
        
        // 初始化 ARM Radix-4 FFT 引擎
        // 参数解释：(FFT 实例, 点数, ifft标志位, 正向=1)
        arm_cfft_radix4_init_f32(&fft_inst, fft_N, 0, 1);
        
        // 阶段 1：数据预处理 - 原始数据 → 加窗浮点复数
        for (i = fft_N; i > 0; i--) 
        {
            /**
             * 关键计算：信号转换与窗函数应用
             * 
             * windowedSignal_1K 是预计算的汉宁窗函数表
             * 每个时域点都与对应的窗函数相乘，以降低频谱泄露
             * 
             * 乘以 10.0f 的作用：
             *   • ADC 原值范围：0~4095
             *   • 乘以 k（~0.0008）后变为 0~3.3V
             *   • 再乘以 10 使其变为 0~33，改善 FFT 精度
             */
            DSPfft_cache[(i-1)*2] = (float)(adc_arr[i-1] * windowedSignal_1K[i-1] * k * 10.0f);
            DSPfft_cache[(i-1)*2+1] = 0.0f;  // 虚部初始化为 0（实信号）
        }
        
        // 阶段 2：执行 FFT 运算（Cooley-Tukey 算法）
        arm_cfft_radix4_f32(&fft_inst, DSPfft_cache);
        
        // 阶段 3：计算幅度谱（模值）
        // 对于复数 z = a + bj，幅度 |z| = √(a² + b²)
        for (i = 0; i < fft_N; i++) 
        {
            // 从工作缓冲区读取 FFT 结果
            fft_arr[i].real = DSPfft_cache[2*i];
            fft_arr[i].imag = DSPfft_cache[2*i+1];
            
            // 计算幅度：√(real² + imag²)，结果存回 real（覆盖原值）
            // 这是一个标准的浮点平方根计算
            arm_sqrt_f32((float32_t)(fft_arr[i].real * fft_arr[i].real + fft_arr[i].imag * fft_arr[i].imag), 
                         &fft_arr[i].real); 
        }
        
        // 阶段 4：检测峰值并计算 THD
        // 返回值 i = 检测到的峰值个数
        i = Afl_fft_getpeak(fft_arr, &ADC1_Peak_ARR[0], fft_N/2, 5, 5, 300);
        
        if(i != 0)  // 找到峰值
        {
            Afl_THD(ADC1_Peak_ARR, &ADC1_Param);  // 计算总谐波失真
        }
        else  // 未找到有效峰值
        {
            ADC1_Param.THD = -1.0f;  // 标记为无效
        }
            
        printf("THD=%.2f\r\n", ADC1_Param.THD);
        return;  // 处理结束
    }
    
    // ==================== 分支 2：大规模 FFT (8192 点) ====================
    else if((fft_N == 8192))
    {
        
        // // 阶段 1：数据预处理 - 消除直流偏置、加窗、转换为浮点
        // for (i = 0; i < fft_N; i++) 
        // {
        //     /**
        //      * 直流消除：(adc_val - 2048.0f)
        //      * - 12bit ADC 的中点是 2048
        //      * - 通过减去中点，将信号转换为 -2048~+2048 范围
        //      * - 这样处理可以消除不需要的直流分量
        //      * 
        //      * windowedSignal_16K[i * 2]：
        //      * - 存储 8192 点的汉宁窗函数
        //      * - 注意索引为 i×2，因为表中可能存储了更高密度的样本
        //      */
        //     float32_t ac_val = (float32_t)adc_arr[i] - 2048.0f; 
        //     fft_arr[i].real = (float32_t)(ac_val * windowedSignal_16K[i * 2] * k * 10.0f);
        //     fft_arr[i].imag = 0.0f;
        // }            
        // ---------------------------------------------------------
        // 【从这里开始因为我的f407只能采集正电压】
        // 阶段 1：数据预处理 - 动态消除直流偏置、加窗、转换为浮点
        
        // 1. 计算当前 8192 个采样点的真实物理平均值（直流偏置）
        uint64_t sum = 0;
        for(i = 0; i < fft_N; i++) 
        {
            sum += adc_arr[i];
        }
        float32_t real_dc_offset = (float32_t)sum / fft_N;

        // 2. 预处理循环：减去动态均值并加窗
        for (i = 0; i < fft_N; i++) 
        {
            float32_t ac_val = (float32_t)adc_arr[i] - real_dc_offset; 
            fft_arr[i].real = (float32_t)(ac_val * windowedSignal_16K[i * 2] * k * 10.0f);
            fft_arr[i].imag = 0.0f;
        }
        // 阶段 2：强制同步全局参数（cfft 内部可能需要 N 值）
        ADC1_Param.N = fft_N; 
        
        // 阶段 3：调用安富莱 FFT 算法
        // 该函数直接在 fft_arr 上进行原地 FFT 变换
        cfft(fft_arr, fft_N);
        
        // 阶段 4：计算幅度谱
        // 与前面相同的处理流程
        for(i = 0; i < fft_N; i++)
        {
            arm_sqrt_f32((float32_t)(fft_arr[i].real * fft_arr[i].real + fft_arr[i].imag * fft_arr[i].imag), 
                         &fft_arr[i].real); 
        }
        
        // 阶段 5：提取峰值与 THD
        i = Afl_fft_getpeak(fft_arr, &ADC1_Peak_ARR[0], fft_N/2, 5, 5, 300);
        if(i != 0)
        {
            Afl_THD(ADC1_Peak_ARR, &ADC1_Param);
        }
        else
        {
            ADC1_Param.THD = -1.0f;
        }
                    // ==================== FFT 调试打印代码段 ====================
         // ==================== FFT 调试打印代码段 (修复版) ====================
					// 假设载波中心频率为 100kHz，分辨率约为 61.035Hz
					// 中心索引 = 100000 / 61.035156 ≈ 1638

//					int center_idx = 1638; 
//					int search_span = 100; 

//					printf("\r\n--- FFT Amplitude Spec (Near Carrier +/- 6kHz) ---\r\n");
//					for(int j = center_idx - search_span; j <= center_idx + search_span; j++) 
//					{
//							if(j >= 0 && j < 4096) 
//							{
//									// 设定阈值，过滤底噪
//									if(fft_arr[j].real > 100.0f) 
//									{
//											float freq = j * (500000.0f / 8192.0f);
//											printf("Idx: %4d | Freq: %7.1f Hz | Amp: %.2f\r\n", j, freq, fft_arr[j].real);
//									}
//							}
//					}
//					printf("---------------------------------------\r\n");

//					printf("--- Peaks Extracted by Algorithm ---\r\n");
//					for(int k = 0; k < i; k++) // 变量 i 是 Afl_fft_getpeak 返回的有效峰值数量
//					{
//							/* * 修复 Error #144: 
//							 * 访问 _PEAK 结构体内部的成员。
//							 * 注意：如果编译依然提示找不到 ".index" 成员，
//							 * 请按住 Ctrl 键左键点击 "_PEAK" 查看其定义，
//							 * 将 ".index" 改为结构体实际定义的名字（可能是 .pos 或 .idx）
//							 */
//							int peak_idx = ADC1_Peak_ARR[k].index; 
//							
//							float peak_freq = peak_idx * (500000.0f / 8192.0f);
//							printf("Peak %d: Idx: %d | Freq: %.1f Hz | Amp: %.2f\r\n", 
//										 k+1, peak_idx, peak_freq, fft_arr[peak_idx].real);
//					}
//					printf("=======================================\r\n");
    }
}



#include "adc.h"

/**
 * ADC_Update()
 * ================================================================
 * 功能：启动 ADC 采样和 DMA 数据转移
 * 
 * 参数：
 *   @ADC_N: ADC 通道号
 *           - 1 表示 ADC1
 *           - 可扩展支持 ADC2、ADC3 等
 * 
 * 返回值：void
 * 
 * 工作流程：
 *   1. 检查指定的 ADC 通道
 *   2. 配置 DMA，将采样点数设置为 ADC1_MAX
 *   3. 启动 HAL ADC DMA 模式
 *   4. 采样数据自动写入 ADC1_Val 缓冲区
 * 
 * 采样流程（硬件自动）：
 *   1. 定时器 TIM3 以 500kHz 频率触发 ADC 转换
 *   2. ADC 完成 12bit 转换后，DMA 将结果搬入 ADC1_Val
 *   3. 采集满 ADC1_MAX 个点后，DMA 触发中断通知应用程序
 * 
 * ⚠️ 实现细节：
 *   • F407 不含 L1 缓存，无需调用 SCB_InvalidateDCache_by_Addr
 *   • DMA 与 ADC 硬件自动同步，应用程序无需轮询
 *   • 推荐在 DMA 完成中断中调用 FFT_convert 处理数据
 * ================================================================
 */
void ADC_Update(u8 ADC_N)
{
    if(ADC_N == 1)
    {
        // 启动 ADC1 的 DMA 模式采集
        // 参数：(ADC 句柄, 数据目标地址, 采样点数)
        // 数据会自动存入 ADC1_Val 数组
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)ADC1_Val, ADC1_MAX);
    }
}

