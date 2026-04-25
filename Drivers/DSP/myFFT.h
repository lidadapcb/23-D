#ifndef __MYFFT_H__
#define __MYFFT_H__

#include "usart.h"
#include "math.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "usart_Connect.h"
#include "dsp/window_functions.h"
#include "Afl_FFT.h"
#include "MY_Delay.h"

// 为了适配 STM32F407 的 128KB 主 SRAM，将最大点数设置为 8192
// 16384 点复数 FFT 需要 128KB，会挤占堆栈空间导致溢出
#define ADC1_MAX 8192

/* 通道 DSP 处理结构体：管理采样与计算结果 */
typedef struct
{
    int N;                          // 采样点数（即 FFT 点数）
    uint32_t SampFreq;              // 采样频率 (Hz)
    uint16_t* adc_arr;              // 指向 ADC 原始数据缓冲区的指针
    COMPX* FFT_arr;                 // 指向 FFT 复数结果缓冲区的指针
    uint16_t adc_resolution_ratio;  // ADC 分辨率 (如 4096)
    float adc_voltage_range;        // ADC 电压量程 (如 3.3V)
    double VPP;                     // 计算出的峰峰值
    double mean_value;              // 计算出的平均值 (直流分量)
    float DC;                       // 直流分量幅值
    float THD;                      // 总谐波失真 (THD)
} CHx_DSP;

extern CHx_DSP ADC1_Param;  

/* 核心缓冲区 */
extern COMPX ADC1_FFT[ADC1_MAX];    // FFT 结果缓冲区 (实部存幅值，虚部存相位)
extern uint16_t ADC1_Val[ADC1_MAX]; // ADC 原始数据缓冲区

/* 初始化与执行函数 */
void myFFT_init(void);
void FFT_convert(uint16_t adc_arr[], COMPX fft_arr[], u16 fft_N, u8 mod, float32_t k);
void ADC_Update(u8 ADC_N);

#endif
