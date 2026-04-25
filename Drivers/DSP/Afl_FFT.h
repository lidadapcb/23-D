#ifndef __AFL_FFT_H__
#define __AFL_FFT_H__


#include "math.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#define MAX_FFT_N 8192  
extern const float32_t windowedSignal_16K[16384];
extern const float32_t windowedSignal_1K[1024];
extern const float32_t sintab[4096];
extern const float32_t costab[4096];

///////////////////////////////////*������FFT�ĸ�������*/////////////////////
typedef struct  compx 
{
	float32_t real, imag;//ʵ�� �鲿
}COMPX; 

///////////////////////////////////*���޵�FFT��������*//////////////////////
void InitTableFFT(uint32_t n);
void Init_window(void);
///////////////////////////////////*������FFT*//////////////////////////////
void cfft(struct compx *_ptr, uint32_t FFT_N );

void Afl_change_to_Criterion(COMPX AFLinput[], uint16_t DSPout[],uint16_t leng);

#endif

