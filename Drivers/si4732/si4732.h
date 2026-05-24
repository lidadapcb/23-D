#ifndef __SI4732_H
#define __SI4732_H

#include "stm32f4xx_hal.h" // 根据你的具体型号包含头文件

/* 1. 设备地址 (参考 AN332 Section 6.1) */
/* Arduino用的是0x11，HAL库需要左移一位 */
#define SI4732_I2C_ADDR_W   (0x11 << 1) 
#define SI4732_I2C_ADDR_R   ((0x11 << 1) | 0x01)

/* 2. 核心命令 (参考 AN332 Section 4 & 5) */
#define CMD_POWER_UP        0x01
#define CMD_GET_REV         0x10
#define CMD_POWER_DOWN      0x11
#define CMD_SET_PROPERTY    0x12
#define CMD_GET_INT_STATUS  0x14
#define CMD_FM_TUNE_FREQ    0x20
#define CMD_FM_RSQ_STATUS   0x23

/* 3. 常用属性 (参考 AN332 Table 5) */
#define PROP_FM_VOLUME      0x4000

// 模式定义
#define MODE_FM             0x00
#define MODE_AM             0x01

// AM 专用命令
#define CMD_AM_TUNE_FREQ    0x40
#define CMD_AM_SEEK_START   0x41
#define CMD_AM_TUNE_STATUS  0x42
#define CMD_AM_RSQ_STATUS   0x49

// AM 常用属性
#define PROP_AM_SEEK_BAND_BOTTOM 0x3400
#define PROP_AM_SEEK_BAND_TOP    0x3401
#define PROP_AM_SEEK_FREQ_SPACING 0x3402
#define PROP_AM_SOFT_MUTE_MAX_ATTENUATION 0x3302
// AM 搜台命令
#define CMD_AM_SEEK_START   0x41 

// AM 信号质量查询命令
#define CMD_AM_RSQ_STATUS   0x49
/* 4. 设备句柄结构体 */
typedef struct {
    I2C_HandleTypeDef *hi2c; // 指向 STM32 I2C 句柄
    GPIO_TypeDef *RST_Port;  // 复位引脚端口
    uint16_t RST_Pin;        // 复位引脚号
} Si4732_Handle_t;

/* 函数声明 */
void Si4732_Init(Si4732_Handle_t *dev, I2C_HandleTypeDef *hi2c, GPIO_TypeDef *port, uint16_t pin);
void Si4732_SetFrequency(Si4732_Handle_t *dev, uint16_t freq_10khz);
void Si4732_SetVolume(Si4732_Handle_t *dev, uint8_t volume);
void Si4732_ConfigSeek(Si4732_Handle_t *dev) ;
uint8_t Si4732_Seek(Si4732_Handle_t *dev, uint8_t direction);
uint16_t Si4732_GetCurrFrequency(Si4732_Handle_t *dev) ;
void Si4732_GetSignalQuality(Si4732_Handle_t *dev, uint8_t *pRSSI, uint8_t *pSNR) ;
void Si4732_SetProperty(Si4732_Handle_t *dev, uint16_t propID, uint16_t propValue);
void Si4732_PowerUp(Si4732_Handle_t *dev, uint8_t mode); // 增加 mode 参数
void Si4732_SetAMFrequency(Si4732_Handle_t *dev, uint16_t freq_khz);
uint16_t Si4732_GetAMFrequency(Si4732_Handle_t *dev);
void Si4732_ConfigAM_Seek(Si4732_Handle_t *dev);
uint8_t Si4732_AM_Seek(Si4732_Handle_t *dev, uint8_t direction);
void Si4732_GetAMSignalQuality(Si4732_Handle_t *dev, uint8_t *pRSSI, uint8_t *pSNR);
#endif