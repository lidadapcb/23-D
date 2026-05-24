#ifndef __2023Diansai_H
#define __2023Diansai_H

#include "myFFT.h"
#include "Afl_FFT.h"

typedef struct 
{
    u16 index;
    float peak;
    double fs;
}_PEAK;
#define Max_Peak_Num 150
extern _PEAK ADC1_Peak_ARR[Max_Peak_Num];

#define _ERR_  0
#define _CW_   1
#define _AM_   2
#define _FM_   3
#define _PM_   4
#define _2ASK_ 5
#define _2FSK_ 6
#define _2PSK_ 7

typedef struct 
{
    u8 name;
    float peak;
    double fs;
    
    float L_peak;
    float32_t L_fs;
    
    float R_peak;
    float32_t R_fs;
    
    float parameter1;
    float parameter2;
    float parameter3;

}Modem_Type;

extern Modem_Type Modem_ARR[8];

int Afl_fft_getpeak(COMPX input[], _PEAK output[], u16 inlen, u8 x, u8 N, float y);
u8 Modulation_Judge(void);
int fft_osc_filter(_PEAK output[]);
int fft_osc_filter_500hz(_PEAK output[]);
float Afl_THD(_PEAK output[], CHx_DSP *S);  

#endif
