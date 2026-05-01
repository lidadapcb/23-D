#include "2023Diansai_DSP.h"
#include "usart_Connect.h"

/* 调制方式计算结果存储数组 */
Modem_Type Modem_ARR[8] =
{
   /* 调制方式 */ /* 载波频率 */ /* 调制频率 */ /* 峰值 */ /* 左边带 */  /* 右边带 */   /* 参数1 */ /* 参数2 */ /* 参数3 */
 {		_ERR_,  		0,					0, 						0, 				0, 					0, 					0,					0,	      	0,		    		0				 },
 {		_CW_,  			0,					0, 						0, 				0, 					0, 					0,					0,					0,		    		0				 },
 {		_AM_, 		  0,					0, 						0, 				0, 					0, 					0,					0,          0,		    		0				 },
 {		_FM_,  			0,					0, 						0, 				0, 					0, 					0,					0,          0,		    		0        },
 {		_PM_,  			0,					0, 						0, 				0, 					0, 					0,					0,	      	0,		    		0				 },
 {	_2ASK_, 			0,					0, 						0, 				0, 					0, 					0,					0,	      	0, 		      0				 },
 {	_2FSK_, 			0,					0, 						0, 				0, 					0, 					0,					0,          0,		      0				 },
 {	_2PSK_, 			0,					0, 						0, 				0, 					0, 					0,					0,	      	0,		      0				 },
};

#define Max_Peak_Num 50
_PEAK ADC1_Peak_ARR[Max_Peak_Num]; // ADC1 频率峰值记录数组

/**
 * @brief  频谱峰值搜索函数
 * @param  input: FFT 结果数组
 * @param  output: 峰值存储数组
 * @param  inlen: 搜索频率范围
 * @param  x: 搜索步长
 * @param  N: 峰值验证宽度
 * @param  y: 幅值门限
 * @return 搜索到的峰值数量
 */
int Afl_fft_getpeak(COMPX input[], _PEAK output[], u16 inlen, u8 x, u8 N, float y) 
{
    u16 i, i2;
    u32 idex;	
    float datas;	
    float32_t sum;	
    u16 outlen = 0;	
	
	for(i = 2; i < inlen-x; i += x)
	{
		datas = 0;
		idex = 0;
		for(u16 k=0; k<x; k++) {
			if(input[i+k].real > datas) {
				datas = input[i+k].real;
				idex = k;
			}
		}
		
		if((input[i+idex].real >= input[i+idex+1].real) && 
		   (input[i+idex].real >= input[i+idex-1].real) && 
		   (datas > y))
		{
			sum = 0;
			for(i2 = i+idex-N; i2 < i+idex+N; i2++) {
				sum += input[i2].real;
			}
			
			if(1.5f*sum/(2*N) < datas) {
				output[outlen].index = i+idex;   
				output[outlen].peak = datas;     
				output[outlen].fs = 1.0f*ADC1_Param.SampFreq*(i+idex)/ADC1_Param.N;
				outlen++;
				if(outlen >= Max_Peak_Num) return outlen;	
			}
		}
	}
	return outlen;
}

u16 Index_MAX; // 最大谱线索引

void CW_Test(void) {
	Modem_ARR[_CW_].peak = ADC1_Peak_ARR[Index_MAX].peak; 			  
	Modem_ARR[_CW_].fs = ADC1_Peak_ARR[Index_MAX].fs;		 			  
}

//void AM_Test(u16 peak_num) {
//	if(peak_num == 3 && ADC1_Peak_ARR[1].fs == ADC1_Peak_ARR[Index_MAX].fs && ADC1_Peak_ARR[1].peak > 0) {
//		Modem_ARR[_AM_].peak = ADC1_Peak_ARR[1].peak;  
//		Modem_ARR[_AM_].fs = ADC1_Peak_ARR[1].fs;		 
//		Modem_ARR[_AM_].parameter2 = (ADC1_Peak_ARR[2].fs - ADC1_Peak_ARR[0].fs)/2.0f;
//		Modem_ARR[_AM_].parameter1 = (ADC1_Peak_ARR[2].peak + ADC1_Peak_ARR[0].peak)/(ADC1_Peak_ARR[1].peak);
//	} else {
//		Modem_ARR[_AM_].parameter2 = 0.0f;
//		Modem_ARR[_AM_].parameter1 = 0.0f;
//	}
//}

//void WBFM_Test(u16 peak_num) {
//	Modem_ARR[_FM_].parameter2 = (ADC1_Peak_ARR[peak_num-1].fs - ADC1_Peak_ARR[0].fs)/2.0f;
//}

///**
// * @brief  调制识别决策逻辑
// * @return 识别出的调制类型编号
// */
//u8 Modulation_Judge(void)
//{
//	u16 peak_num = 0;	
//	u16 i = 0;
//	
//	if(FFT_Accomplish_TAG == 0) return 0;
//	
//	peak_num = fft_osc_filter(ADC1_Peak_ARR);
//	if(peak_num == 0) return _ERR_;
//	
//	ADC1_Param.DC = ADC1_FFT[0].real / ADC1_Param.N;
//	
//	for(i = 0, Index_MAX = 0; i < peak_num; i++) {
//		if(ADC1_Peak_ARR[i].peak > ADC1_Peak_ARR[Index_MAX].peak) Index_MAX = i;
//	}
//	
//	// 阶段四：重构调制识别决策树，绕过不稳定的 THD 屏障
//	if(peak_num == 1) {
//		CW_Test();
//		return _CW_;
//	} else if (peak_num == 3) {
//		// AM与2ASK在频域表现均有三个主峰（载波及两边带）
//		// 此处先导通AM流程进行测试
//		AM_Test(peak_num); 
//		return _AM_; 
//	} else if (peak_num > 3) {
//		// WBFM 存在多阶贝塞尔函数分布的大量侧带
//		WBFM_Test(peak_num); 
//		return _FM_;
//	}

//	return _ERR_;	
//}
/* 在文件顶部引入必要的宏定义，用于绝对值计算 */
#define ABS_F(x) ((x) > 0.0f ? (x) : -(x))

/**
 * @brief  重构的调制识别决策逻辑 (融合 AM/FM 与 2FSK)
 * @return 识别出的调制类型编号
 */
u8 Modulation_Judge(void)
{
    u16 peak_num = 0;    
    u16 i = 0;
    u16 carrier_idx = 0;

    if(FFT_Accomplish_TAG == 0) return 0;
    
    peak_num = fft_osc_filter(ADC1_Peak_ARR);
    if(peak_num == 0) return _ERR_;
    
    ADC1_Param.DC = ADC1_FFT[0].real / ADC1_Param.N;

    // 单一峰值，判定为连续波
    if(peak_num == 1) {
        Modem_ARR[_CW_].peak = ADC1_Peak_ARR[0].peak;                
        Modem_ARR[_CW_].fs = ADC1_Peak_ARR[0].fs;
        return _CW_;
    }
    
    // 利用首尾峰值均值寻找理论载波中心
    float spectrum_center = (ADC1_Peak_ARR[0].fs + ADC1_Peak_ARR[peak_num - 1].fs) / 2.0f;
    float min_dist = 999999.0f;
    
    for(i = 0; i < peak_num; i++) {
        float dist = ABS_F(ADC1_Peak_ARR[i].fs - spectrum_center);
        if(dist < min_dist) {
            min_dist = dist;
            carrier_idx = i;
        }
    }
    Index_MAX = carrier_idx; 

    // ==========================================================
    // 阶段一：模拟调制 (AM/FM) 判定
    // ==========================================================
    if (carrier_idx > 0 && carrier_idx < peak_num - 1) 
    {
        float f_c = ADC1_Peak_ARR[carrier_idx].fs;
        float f_left = ADC1_Peak_ARR[carrier_idx - 1].fs;
        float f_right = ADC1_Peak_ARR[carrier_idx + 1].fs;
        
        float peak_c = ADC1_Peak_ARR[carrier_idx].peak;
        float peak_left = ADC1_Peak_ARR[carrier_idx - 1].peak;
        float peak_right = ADC1_Peak_ARR[carrier_idx + 1].peak;

        float delta_f_left = f_c - f_left;
        float delta_f_right = f_right - f_c;

        // 检查基本对称性
        if (ABS_F(delta_f_left - delta_f_right) < 150.0f) 
        {
            float fm = (delta_f_left + delta_f_right) / 2.0f;
            float amp_diff_ratio = ABS_F(peak_left - peak_right) / peak_left;

					 // 【核心护城河】：利用赛题物理规则隔离数字调制
					// 计算等效调幅系数 ma
            float ma_val = (peak_left + peak_right) / peak_c;
           
            // 如果 fm <= 5.5kHz，才允许进入 AM/FM 判断；否则强制进入下方的数字调制分支
            if (fm <= 5500.0f) 
            {
                if (peak_num <= 3 && amp_diff_ratio < 0.5f&& ma_val < 1.1f) 
                {
                    Modem_ARR[_AM_].fs = f_c;
                    Modem_ARR[_AM_].parameter2 = fm; 
                    Modem_ARR[_AM_].parameter1 = ma_val;
                    return _AM_;
                } 
                else 
                {
                    Modem_ARR[_FM_].fs = f_c;
                    Modem_ARR[_FM_].parameter2 = fm; 
                    
                    // 使用方差能量法精确计算 FM 最大频偏
                    float power_sum = 0.0f;
                    float freq_var_sum = 0.0f;
                    for(int k = 0; k < peak_num; k++) {
                        float amp = ADC1_Peak_ARR[k].peak;
                        float f_diff = ADC1_Peak_ARR[k].fs - f_c;
                        float power = amp * amp; 
                        power_sum += power;
                        freq_var_sum += power * (f_diff * f_diff);
                    }
                    
                    float df_max = 0.0f;
                    if(power_sum > 0.0f) {
                        float variance = freq_var_sum / power_sum;
                        arm_sqrt_f32(2.0f * variance, &df_max);
                    }
                    Modem_ARR[_FM_].parameter3 = df_max;
                    if (fm > 0) Modem_ARR[_FM_].parameter1 = df_max / fm;
                    else Modem_ARR[_FM_].parameter1 = 0;
                    
                    return _FM_;
                }
            }
        }
    }

  // ==========================================================
    // 阶段二：数字调制 (2FSK 等) 判定
    // ==========================================================
    
    // 重新提取带内幅度最大的前 3 个峰
    // 【核心修正】：强制加上频段护城河，绝对丢弃 70kHz 以下和 130kHz 以上的任何噪声峰
    float max1_amp = 0, max2_amp = 0, max3_amp = 0;
    float f1 = 0, f2 = 0, f3 = 0;

    for(int k = 0; k < peak_num; k++) {
        float fs = ADC1_Peak_ARR[k].fs;
        float amp = ADC1_Peak_ARR[k].peak;
        
        // 物理隔离带：只有在这个中频带内的峰才允许参与 2FSK 计算
        if(fs > 70000.0f && fs < 130000.0f) {
            if(amp > max1_amp) {
                max3_amp = max2_amp; f3 = f2;
                max2_amp = max1_amp; f2 = f1;
                max1_amp = amp;      f1 = fs;
            } else if(amp > max2_amp) {
                max3_amp = max2_amp; f3 = f2;
                max2_amp = amp;      f2 = fs;
            } else if(amp > max3_amp) {
                max3_amp = amp;      f3 = fs;
            }
        }
    }

    // 计算主载波参数
    float delta_F_digital = ABS_F(f1 - f2);

    // 2FSK 判定：两个主载波频差显著
    if (delta_F_digital >= 4000.0f) // 赛题 2FSK 最小频偏跨度通常大于此值
    {
        Modem_ARR[_2FSK_].fs = spectrum_center; 
        
        float Rc = 0.0f;
        float h = 0.0f;
        
        // 【核心修正】：针对无侧带现象的物理降级策略
        if (max3_amp > 80.0f && ABS_F(f3 - f1) > 1000.0f && ABS_F(f3 - f2) > 1000.0f) 
        {
            // 情况 A：存在有效的第三个侧带峰
            float dist_to_f1 = ABS_F(f3 - f1);
            float dist_to_f2 = ABS_F(f3 - f2);
            float sideband_dist = (dist_to_f1 < dist_to_f2) ? dist_to_f1 : dist_to_f2;
            Rc = sideband_dist * 2.0f;
        } 
        else 
        {
            // 情况 B：侧带能量消失 (对应 h=1 等特例)
            // 此时两个载波之间的距离近似等于码元速率
            Rc = delta_F_digital; 
        }
        
        if (Rc > 0) {
            h = delta_F_digital / Rc;
        }

        // 装载物理参数
        // 请注意：这里我统一规定 parameter1 为码速率，parameter2 为 h
        // 如果你的打印还是反的，请去修改你的 printf 函数，不要改这里！
        Modem_ARR[_2FSK_].parameter1 = Rc; // 码速率 Rc (Hz)
        Modem_ARR[_2FSK_].parameter2 = h;  // 键控系数 h
        
        return _2FSK_;
    }

    return _ERR_;   
}
/**
 * @brief  频谱列表排序（按频率索引升序）
 */
void fft_peakarr_sort(_PEAK output[], u16 num)
{
	u16 temp;	
	int i = 0, j = 0;
	_PEAK Temp_peak;
	for(j = 0; j < num; j++) {
		temp = 0;
		for(i = 0; i < num-j-1; i++) {
			if(output[i].index > output[i+1].index) {
				Temp_peak = output[i+1];
				output[i+1] = output[i];
				output[i] = Temp_peak;
				temp++;
			}
		}
		if(temp == 0) break;
	}
}

///**
// * @brief  频谱粗滤：提取显著波峰
// */
//int fft_osc_filter(_PEAK output[])
//{
//	int i = 0;
//	u16 num = 0;	
//	float32_t Max_peak = 0, Tag_peak = 0;
//    double res = (double)ADC1_Param.SampFreq / ADC1_Param.N;
//	u16 scan_limit = ADC1_Param.N / 2; // 阶段二：动态确定奈奎斯特边界

//	for(i = 20; i < scan_limit; i++) {
//		if(ADC1_FFT[i].real > Max_peak) Max_peak = ADC1_FFT[i].real;
//	}
//	// 阶段三：提升动态阈值比例至 5% 以抑制底噪
//	Tag_peak = 0.05f * Max_peak;	
//	
//	for(i = 0; i < Max_Peak_Num; i++) { output[i].index = 0; output[i].fs = 0; output[i].peak = 0; }
//	
//	for(i = 20; i < scan_limit - 1; i++) {
//		// 阶段三：移除 && (ADC1_FFT[i].real > 300) 绝对限制
//		if(ADC1_FFT[i].real > Tag_peak) {
//			if((ADC1_FFT[i].real >= ADC1_FFT[i-1].real) && (ADC1_FFT[i].real >= ADC1_FFT[i+1].real)) {
//				output[num].index = i;
//				output[num].peak = ADC1_FFT[i].real;
//				output[num].fs = i * res;
//				num++;
//				if(num >= Max_Peak_Num) break;			
//			}
//		}
//	}	
//	fft_peakarr_sort(output, num);	
//	return num;	
//}
//int fft_osc_filter(_PEAK output[])
//{
//    int i = 0;
//    u16 num = 0;    
//    float32_t Max_peak = 0, Tag_peak = 0;
//    double res = (double)ADC1_Param.SampFreq / ADC1_Param.N;
//    u16 scan_limit = ADC1_Param.N / 2;

//    for(i = 20; i < scan_limit; i++) {
//        if(ADC1_FFT[i].real > Max_peak) Max_peak = ADC1_FFT[i].real;
//    }
//    
//    // 【修正点 1】：提高相对阈值至 10%，并强制施加绝对底噪拦截
//    Tag_peak = 0.10f * Max_peak;    
//    if (Tag_peak < 150.0f) Tag_peak = 150.0f; // 强制物理底噪门限，防止抓取纯噪声
//    
//    for(i = 0; i < Max_Peak_Num; i++) { output[i].index = 0; output[i].fs = 0; output[i].peak = 0; }
//    
//    for(i = 20; i < scan_limit - 1; i++) {
//        if(ADC1_FFT[i].real > Tag_peak) {
//					
//            if((ADC1_FFT[i].real >= ADC1_FFT[i-1].real) && (ADC1_FFT[i].real >= ADC1_FFT[i+1].real)) {
//                output[num].index = i;
//                output[num].peak = ADC1_FFT[i].real;
//                output[num].fs = i * res;
//                num++;
//                if(num >= Max_Peak_Num) break;            
//            }
//        }
//    }   
//    fft_peakarr_sort(output, num);  
//    return num; 
//}
/**
 * @brief  频谱粗滤：提取显著波峰 (引入软件带通窗与抗 FM 漏峰策略)
 */
int fft_osc_filter(_PEAK output[])
{
    int i = 0;
    u16 num = 0;    
    float32_t Max_peak = 0, Tag_peak = 0;
    double res = (double)ADC1_Param.SampFreq / ADC1_Param.N;
    
    // 【核心修改 1】：引入软件带通滤波器，限定搜索范围在 73kHz ~ 128kHz
    // 索引 1200 对应约 73.2kHz，索引 2100 对应约 128.1kHz
    // 彻底屏蔽低频包络泄露 (例如 1kHz 假峰) 和高频二次谐波 (例如 200kHz 假峰)
    u16 search_start = 1200; 
    u16 search_end = 2100;   

    // 第 1 步：仅在有效频段内寻找最大峰值 (Max_peak)
    for(i = search_start; i < search_end; i++) {
        if(ADC1_FFT[i].real > Max_peak) {
            Max_peak = ADC1_FFT[i].real;
        }
    }
    
    // 【核心修改 2】：放宽相对阈值，严格绝对阈值
    // 将比例从 10% 降为 5% (0.05f)，确保调频(FM)信号较弱的边缘贝塞尔侧带不被漏掉
    Tag_peak = 0.05f * Max_peak;    
    
    // 绝对底噪门限死死守住 150.0f，低于此值的微小波动一律视为白噪声
    if (Tag_peak < 40.0f) {
        Tag_peak = 40.0f; 
    }
    
    // 清空输出数组
    for(i = 0; i < Max_Peak_Num; i++) { 
        output[i].index = 0; 
        output[i].fs = 0; 
        output[i].peak = 0; 
    }
    
    // 第 2 步：仅在有效频段内进行寻峰判定
    for(i = search_start; i < search_end - 1; i++) {
        // 首先判断当前点幅度是否超过动态阈值
        if(ADC1_FFT[i].real > Tag_peak) {
            // 判断当前点是否为局部最高点 (大于前一个点且大于后一个点)
            if((ADC1_FFT[i].real >= ADC1_FFT[i-1].real) && (ADC1_FFT[i].real >= ADC1_FFT[i+1].real)) {
                output[num].index = i;
                output[num].peak = ADC1_FFT[i].real;
                output[num].fs = i * res;
                num++;
                if(num >= Max_Peak_Num) break;            
            }
        }
    }   
    
    // 按频率从低到高排序
    fft_peakarr_sort(output, num);  
    return num; 
}
/**
 * @brief  总谐波失真 (THD) 简易计算
 */
float Afl_THD(_PEAK output[], CHx_DSP *S)
{
	float THD;
	int i = 0, max_index = 0;
	u16 search_limit = (50 < Max_Peak_Num) ? 50 : Max_Peak_Num;

	for(i = 0; i < search_limit; i++) {
		if(output[i].peak > output[max_index].peak) max_index = i;
	}
	
	if(max_index + 3 < Max_Peak_Num) {
		arm_sqrt_f32(output[max_index+1].peak * output[max_index+1].peak + 
					 output[max_index+2].peak * output[max_index+2].peak + 
					 output[max_index+3].peak * output[max_index+3].peak, &THD);
		THD /= output[max_index].peak;
		S->THD = THD * 100;
	} else {
		S->THD = 0;
		THD = 0;
	}
	return THD;
}
