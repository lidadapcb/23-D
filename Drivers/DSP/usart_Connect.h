#ifndef __USART_CONNECT_H__
#define __USART_CONNECT_H__

#include "usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RxBuffMaxLEN 256
#define TxBuffMaxLEN 256

extern uint8_t FFT_Accomplish_TAG;

// 修复警告：提供明确的参数类型 (void)
void connectUART_init(void);
void connect_Printf(const char *format, ...);
void connect_DMA_Printf(const char *format, ...);
void connect_Task(void);
void Usart2_Task(void);

#endif
