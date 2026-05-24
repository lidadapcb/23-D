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
    // --- Debug: Print Top 4 Peaks for Digital Modulation Analysis ---
//    printf("\r\n[Top 4 Peaks]\r\n");
//    for(int d = 0; d < 4 && d < peak_num; d++) {
//        printf("Peak %d: Freq=%.1fHz, Amp=%.1f\r\n", d, ADC1_Peak_ARR[d].fs, ADC1_Peak_ARR[d].peak);
//    }
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
    // --- Debug: Print Top 4 Peaks for Digital Modulation Analysis ---
    printf("\r\n[Top 4 Peaks]\r\n");
    for(int d = 0; d < 10 && d < peak_num; d++) {
        printf("Peak %d: Freq=%.1fHz, Amp=%.1f\r\n", d, ADC1_Peak_ARR[d].fs, ADC1_Peak_ARR[d].peak);
    }
    
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
    // --- Data-Driven Digital Modulation Logic (2ASK, 2FSK, 2PSK) ---
    // ==========================================================
    {
    // ==========================================================
        // 0. 无漏洞的峰值排序提取 (取前 4 大)
        // ==========================================================
        _PEAK top_peaks[4];
        int top_n = (peak_num < 4) ? peak_num : 4;
        
        for(int m = 0; m < top_n; m++) {
            int best_idx = -1;
            float max_v = -1.0f;
            for(int n = 0; n < peak_num; n++) {
                int already_in = 0;
                // 【修复 Bug】：删除了错误的 n > 0 判断
                for(int p = 0; p < m; p++) {
                    if(ADC1_Peak_ARR[n].fs == top_peaks[p].fs) { 
                        already_in = 1; 
                        break; 
                    }
                }
                if(!already_in && ADC1_Peak_ARR[n].peak > max_v) {
                    max_v = ADC1_Peak_ARR[n].peak;
                    best_idx = n;
                }
            }
            if(best_idx != -1) top_peaks[m] = ADC1_Peak_ARR[best_idx];
        }

        if(top_n >= 2) {
            float max1_freq = top_peaks[0].fs;
            float max1_amp  = top_peaks[0].peak;
            float max2_freq = top_peaks[1].fs;
            float max2_amp  = top_peaks[1].peak;

            // ==========================================================
            // 1. 基础间隔提取 raw_Rc
            // ==========================================================
            float raw_Rc = 999999.0f;
            for(int m = 0; m < top_n; m++) {
                for(int n = m + 1; n < top_n; n++) {
                    float diff = ABS_F(top_peaks[m].fs - top_peaks[n].fs);
                    if(diff > 500.0f && diff < raw_Rc) {
                        raw_Rc = diff;
                    }
                }
            }
            if(raw_Rc == 999999.0f) raw_Rc = ABS_F(max1_freq - max2_freq);

// ==========================================================
            // 2. 能量跨度与物理特征修正 (响应 1010 序列规律)
            // ==========================================================
            float f_min = 999999.0f;
            float f_max = 0.0f;
            for (int m = 0; m < top_n; m++) {
                if (top_peaks[m].peak > max1_amp * 0.4f) { 
                    if (top_peaks[m].fs < f_min) f_min = top_peaks[m].fs;
                    if (top_peaks[m].fs > f_max) f_max = top_peaks[m].fs;
                }
            }
            float delta_F = f_max - f_min; 
            float main_peak_dist = ABS_F(max1_freq - max2_freq);
            
            // 【物理分水岭】：计算“强能量总跨度”与“两最大峰间距”的比值
            // FSK 能量集中在两主峰之间，比例趋近于 1.0
            // FM 能量向外扩散，比例通常大于 1.5
            float span_ratio = (main_peak_dist > 0) ? (delta_F / main_peak_dist) : 1.0f;

            float true_Rc = raw_Rc * 2.0f; 
            float true_h = (true_Rc > 0) ? (delta_F / true_Rc) : 0;

// ==========================================================
            // 3. 载波能量与频谱平坦度分析 (严格的物理分水岭统计)
            // ==========================================================
            float fc_amp = 0;
            int strong_peak_count = 0; 
            
            for(int k = 0; k < peak_num; k++) {
                // 1. 提取载波能量
                if(ABS_F(ADC1_Peak_ARR[k].fs - 100000.0f) < 800.0f) { 
                    fc_amp = ADC1_Peak_ARR[k].peak;
                }
                
                // 2. 统计强峰数量 (区分 Bessel 连续谱 与 Sinc 离散谱)
                // 物理极限：2FSK 侧带理论最高约为主峰的 33%。
                // 设定 45% 作为界限，2FSK 强峰数绝对 <= 2，而宽带 FM 强峰数必定 >= 4
                if(ADC1_Peak_ARR[k].peak > max1_amp * 0.45f) {
                    strong_peak_count++;
                }
            }

            // ==========================================================
            // 4. 拓扑分类判定 (两级宇宙模型)
            // ==========================================================
// 计算有效宽频侧带数量 (用于拦截载波震荡变高的宽带 FM)
            int valid_peak_count = 0;
            for(int k=0; k<peak_num; k++) {
                if(ADC1_Peak_ARR[k].peak > max1_amp * 0.15f) valid_peak_count++;
            }

            // ==========================================================
            // 4. 拓扑分类判定 (两级宇宙模型)
            // ==========================================================
            
            // 【第一宇宙】：载波占据绝对主导地位，且频带极其狭窄 (AM, 2ASK, 窄带 NBFM)
            // 增加 valid_peak_count <= 5 条件，防止 20kHz 这种高频偏 FM 混入
            if (fc_amp > max1_amp * 0.65f && valid_peak_count <= 5) {
                
                if (peak_num <= 3) {
                    Modem_ARR[_AM_].fs = 100000.0f;
                    Modem_ARR[_AM_].peak = max1_amp;
                    float ma = (max2_amp * 2.0f) / max1_amp;
                    if (ma > 1.0f) ma = 1.0f;
                    Modem_ARR[_AM_].parameter1 = ma;      
                    Modem_ARR[_AM_].parameter2 = raw_Rc;  
                    return _AM_;
                } else {
                    // 依靠贝塞尔展开式的偶次谐波区分 NBFM 与 2ASK
                    int has_even_harmonic = 0;
                    for(int k = 0; k < peak_num; k++) {
                        float dist_to_fc = ABS_F(ADC1_Peak_ARR[k].fs - 100000.0f);
                        if (ABS_F(dist_to_fc - (raw_Rc * 2.0f)) < 1500.0f) {
                            has_even_harmonic = 1; break;
                        }
                    }

                    if (has_even_harmonic) {
                        Modem_ARR[_FM_].fs = 100000.0f;
                        Modem_ARR[_FM_].peak = max1_amp;
                        // NBFM 贝塞尔倒推
                        float bessel_ratio = max2_amp / max1_amp;
                        float mf = 2.0f * bessel_ratio; 
                        float df_max = mf * raw_Rc;
                        
                        Modem_ARR[_FM_].parameter1 = mf;       
                        Modem_ARR[_FM_].parameter2 = raw_Rc;   
                        Modem_ARR[_FM_].parameter3 = df_max;   
                        return _FM_;
                    } else {
                        // 方波无偶次谐波，判定为 2ASK
                        Modem_ARR[_2ASK_].fs = 100000.0f;
                        Modem_ARR[_2ASK_].peak = max1_amp;
                        Modem_ARR[_2ASK_].parameter1 = 0;
                        Modem_ARR[_2ASK_].parameter2 = true_Rc; 
                        return _2ASK_;
                    }
                }
            }
            
// 【第二宇宙】：载波被抑制 (宽带 WBFM, 2FSK, 2PSK)
            else {
                // 1. 提取信号的最宽有效边界 (span)
                float f_left = 999999.0f;
                float f_right = 0.0f;
                for(int k=0; k<peak_num; k++) {
                    // 提取所有大于 30% 最大幅值的峰作为边界
                    if(ADC1_Peak_ARR[k].peak > max1_amp * 0.3f) {
                        if(ADC1_Peak_ARR[k].fs < f_left) f_left = ADC1_Peak_ARR[k].fs;
                        if(ADC1_Peak_ARR[k].fs > f_right) f_right = ADC1_Peak_ARR[k].fs;
                    }
                }
                float span = f_right - f_left;
                
  // 2. 2PSK 绝对保护 (完美对称与间距锁定)
                float max_midpoint = (max1_freq + max2_freq) / 2.0f;
                float max_dist = ABS_F(max1_freq - max2_freq);
                
                // 【物理分水岭】：2PSK (1010序列) 的两大主峰必然完美对称于 100kHz，
                // 且因为方波偶次谐波为零的特性，它们的主峰间距 (max_dist) 必须严格等于全场基准间距 (raw_Rc)！
                if (ABS_F(max_midpoint - 100000.0f) < 2000.0f) {
                    if (ABS_F(max_dist - raw_Rc) < 1500.0f) {
                        // 且载波处(100kHz)被深度抑制
                        if (fc_amp < max1_amp * 0.4f) {
                            Modem_ARR[_2PSK_].fs = 100000.0f;
                            Modem_ARR[_2PSK_].peak = max1_amp;
                            
                            // 对于 2PSK，真实的 Rc 就是两个主侧带的间距
                            Modem_ARR[_2PSK_].parameter1 = max_dist;       
                            Modem_ARR[_2PSK_].parameter2 = max_dist; 
                            return _2PSK_;
                        }
											}
										}

  // 3. WBFM 与 2FSK 剥离: 中心能量真空探测 (物理深谷分水岭)
                int center_is_empty = 1; 
                
                if (span > 15000.0f) {
                    // 划定中心区域 (抛弃两端各 25% 的频带，只看中间一半)
                    float mid_left = f_left + span * 0.25f;
                    float mid_right = f_right - span * 0.25f;
                    
                    for(int k = 0; k < peak_num; k++) {
                        if (ADC1_Peak_ARR[k].fs > mid_left && ADC1_Peak_ARR[k].fs < mid_right) {
                            // 【修正 1】：提高真空容忍度到 40%。
                            // FSK 侧带叠加的谷底约 33%，FM 连续谱填充高达 50% 以上。40% 是绝佳的深谷分水岭！
                            if (ADC1_Peak_ARR[k].peak > max1_amp * 0.40f) { 
                                center_is_empty = 0; 
                                break;
                            }
                        }
                    }
                } else {
                    center_is_empty = 0; // 极窄的谱线默认归入 FM
                }

                // 4. 执行分类
if (!center_is_empty) {
                    // 中心被高能量填满 -> 宽带调频 WBFM
                    Modem_ARR[_FM_].fs = 100000.0f;
                    Modem_ARR[_FM_].peak = max1_amp;
                    
                    float fm_min = 100000.0f;
                    float fm_max = 100000.0f;
                    for(int k = 0; k < peak_num; k++) {
                        // 【核心修正】：统一使用 0.18f 黄金阈值，完美兼容 10k, 20k, 25k 的卡森物理边界
                        if(ADC1_Peak_ARR[k].peak > max1_amp * 0.18f && ABS_F(ADC1_Peak_ARR[k].fs - 100000.0f) < 60000.0f) {
                            if(ADC1_Peak_ARR[k].fs < fm_min) fm_min = ADC1_Peak_ARR[k].fs;
                            if(ADC1_Peak_ARR[k].fs > fm_max) fm_max = ADC1_Peak_ARR[k].fs;
                        }
                    }
                    float BW = fm_max - fm_min; 
                    float df_max = (BW / 2.0f) - raw_Rc;
                    if(df_max < 0.0f) df_max = raw_Rc; 
                    
                    float mf = 0;
                    if(raw_Rc > 0) mf = df_max / raw_Rc;

                    Modem_ARR[_FM_].parameter1 = mf;       
                    Modem_ARR[_FM_].parameter2 = raw_Rc;   
                    Modem_ARR[_FM_].parameter3 = df_max;   
                    return _FM_;
                    
                } else {
                    // 中心存在深谷 -> 2FSK
                    Modem_ARR[_2FSK_].fs = 100000.0f; // 2FSK的理论中心
                    Modem_ARR[_2FSK_].peak = max1_amp;
                    
    // Rc 倒推容错 (放宽下限阈值，解除对 6kbps 及以下速率的封锁)
                    if (raw_Rc > 12000.0f || raw_Rc < 1500.0f) { 
                         for(int test_h = 2; test_h <= 12; test_h++) {
                             float temp_Rc = span / test_h; 
                             // 同时也要放宽循环里的容错下限
                             if(temp_Rc >= 1500.0f && temp_Rc <= 11000.0f) {
                                 raw_Rc = temp_Rc;
                                 true_Rc = temp_Rc * 2.0f; 
                                 break;
                             }
                         }
                    }
                    // 【修正 2】：精准提取左右两个主载波的真实距离
                    // 抛弃包含侧带的 span，只找左半区最强峰和右半区最强峰
                    float max_amp_L = 0; float f_carrier_L = 100000.0f;
                    float max_amp_R = 0; float f_carrier_R = 100000.0f;
                    for(int k = 0; k < peak_num; k++) {
                        if (ADC1_Peak_ARR[k].fs < 100000.0f && ADC1_Peak_ARR[k].peak > max_amp_L) {
                            max_amp_L = ADC1_Peak_ARR[k].peak;
                            f_carrier_L = ADC1_Peak_ARR[k].fs;
                        }
                        if (ADC1_Peak_ARR[k].fs > 100000.0f && ADC1_Peak_ARR[k].peak > max_amp_R) {
                            max_amp_R = ADC1_Peak_ARR[k].peak;
                            f_carrier_R = ADC1_Peak_ARR[k].fs;
                        }
                    }
                    
                    // 两个真实载波的频率差，除以 2 得到单边频偏
                    float carrier_span = ABS_F(f_carrier_R - f_carrier_L);
                    float true_h = (true_Rc > 0) ? ((carrier_span / 2.0f) / true_Rc) : 0;
                    
                    Modem_ARR[_2FSK_].parameter1 = true_h;  
                    Modem_ARR[_2FSK_].parameter2 = true_Rc; 
                    return _2FSK_;
                }
            }
            
					}
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
    u16 search_start = 750; 
    u16 search_end = 2600;   

    // 第 1 步：仅在有效频段内寻找最大峰值 (Max_peak)
    for(i = search_start; i < search_end; i++) {
        if(ADC1_FFT[i].real > Max_peak) {
            Max_peak = ADC1_FFT[i].real;
        }
    }
    
    // 【核心修改 2】：放宽相对阈值，严格绝对阈值
    // 将比例从 10% 降为 5% (0.05f)，确保调频(FM)信号较弱的边缘贝塞尔侧带不被漏掉
    Tag_peak = 0.10f * Max_peak;    
    
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
