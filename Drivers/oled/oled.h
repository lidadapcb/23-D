#ifndef __OLED_H
#define __OLED_H

#include "main.h"

// OLED 物理 I2C 地址 (SSD1306 常见地址为 0x78)
#define OLED_HW_ADDRESS      0x78

/* ==================================================================== */
/* ------------------------ [2] 屏幕物理参数宏 ------------------------ */
/* ==================================================================== */
#define OLED_WIDTH           128
#define OLED_HEIGHT          64
#define OLED_PAGES           (OLED_HEIGHT / 8)
#define OLED_BUFFER_SIZE     (OLED_WIDTH * OLED_PAGES) // 显存大小 1024 字节

/* ==================================================================== */
/* ------------------------ [3] 外部函数与变量接口 -------------------- */
/* ==================================================================== */
extern uint8_t OLED_Buffer[OLED_PAGES][OLED_WIDTH];

// 基础控制
void Soft_I2C_Init(void);
void OLED_Init(void);
void OLED_Init_DMA(void); // 为了兼容原代码保留名字，内部改用软I2C
void OLED_Clear(void);
void OLED_Clear_Buffer(void);

// 显存刷新
void Refresh(void);                  
void OLED_Refresh_DMA(void); // 内部使用软I2C的高速阻塞刷新

// 基础显示 (直接操作屏幕GRAM)
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowChinese(uint8_t Line, uint8_t Column, uint8_t CHN);
void OLED_ShowPicture(uint8_t Pic);

// 快速显示 (操作本地SRAM缓冲区，需配合刷新函数使用)
void OLED_FastShowPicture(uint8_t Pic);
void OLED_FastShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_FastShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_FastShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
uint32_t OLED_Pow(uint32_t X, uint32_t Y);

#endif