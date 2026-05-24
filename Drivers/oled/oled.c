#include "oled.h"
#include "oled_font.h" 
#include "stdio.h"

uint8_t OLED_Buffer[OLED_PAGES][OLED_WIDTH] = {0};

/* ===================== 软件 I2C 底层 ===================== */
#define OLED_SCL_PIN GPIO_PIN_6
#define OLED_SCL_PORT GPIOB
#define OLED_SDA_PIN GPIO_PIN_7
#define OLED_SDA_PORT GPIOB

#define OLED_SCL_Clr() HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define OLED_SCL_Set() HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define OLED_SDA_Clr() HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)
#define OLED_SDA_Set() HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)

static void IIC_Delay(void)
{
    // H723 主频 420MHz，一个简单的 for 循环大约 3~4 个 CPU 周期
    // 设为 300 约等于 1000 周期 = ~2.5us 延时，折算 I2C 频率为 <200kHz，非常安全
    for(volatile uint32_t i=0; i<300; i++);
}

void Soft_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // 初始化 PB6 和 PB7 为开漏输出，带上拉
    GPIO_InitStruct.Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    OLED_SCL_Set();
    OLED_SDA_Set();
}

static void IIC_Start(void)
{
    OLED_SCL_Set();
    OLED_SDA_Set();
    IIC_Delay();
    OLED_SDA_Clr();
    IIC_Delay();
    OLED_SCL_Clr();
}

static void IIC_Stop(void)
{
    OLED_SCL_Clr();
    OLED_SDA_Clr();
    IIC_Delay();
    OLED_SCL_Set();
    IIC_Delay();
    OLED_SDA_Set();
    IIC_Delay();
}

static void IIC_SendByte(uint8_t txd)
{
    uint8_t i;
    OLED_SCL_Clr();
    for (i = 0; i < 8; i++)
    {
        if ((txd & 0x80) >> 7)
            OLED_SDA_Set();
        else
            OLED_SDA_Clr();
        txd <<= 1;
        IIC_Delay();
        OLED_SCL_Set();
        IIC_Delay();
        OLED_SCL_Clr();
        IIC_Delay();
    }
    
    // 等待 ACK (假装等待，OLED不理ACK我们也能发)
    OLED_SDA_Set();
    IIC_Delay();
    OLED_SCL_Set();
    IIC_Delay();
    OLED_SCL_Clr();
}

void OLED_WriteCommand(uint8_t Command)
{
    IIC_Start();
    IIC_SendByte(OLED_HW_ADDRESS);
    IIC_SendByte(0x00);
    IIC_SendByte(Command);
    IIC_Stop();
}

void OLED_WriteData(uint8_t Data)
{
    IIC_Start();
    IIC_SendByte(OLED_HW_ADDRESS);
    IIC_SendByte(0x40);
    IIC_SendByte(Data);
    IIC_Stop();
}

static void OLED_WriteCmd(uint8_t cmd) // 初始化专用
{
    OLED_WriteCommand(cmd);
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
    OLED_WriteCommand(0x00 | (X & 0x0F));
}

/* ===================== 初始化与控制 ===================== */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < OLED_PAGES; j++)
    {
        OLED_SetCursor(j, 0);
        for(i = 0; i < OLED_WIDTH; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_Clear_Buffer(void)
{
    for (uint8_t i = 0; i < OLED_PAGES; i++)
    {
        for (uint8_t j = 0; j < OLED_WIDTH; j++)
        {
            OLED_Buffer[i][j] = 0x00;
        }
    }
}

void OLED_Init_DMA(void) // 保留函数名以防修改 main.c 时出错
{
    HAL_Delay(200); // 确保电源稳定
    
    OLED_WriteCmd(0xAE); // 关闭显示
    OLED_WriteCmd(0xD5); // 设置显示时钟
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); // 复用率
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3); // 显示偏移
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40); // 开始行

    OLED_WriteCmd(0x20); // 水平寻址模式
    OLED_WriteCmd(0x00); 
    OLED_WriteCmd(0x21); // 列边界
    OLED_WriteCmd(0x00); 
    OLED_WriteCmd(0x7F); 
    OLED_WriteCmd(0x22); // 页边界
    OLED_WriteCmd(0x00); 
    OLED_WriteCmd(0x07); 

    OLED_WriteCmd(0xA1); // 左右
    OLED_WriteCmd(0xC8); // 上下
    OLED_WriteCmd(0xDA); // COM引脚配置
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); // 对比度
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); // 预充电
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); // VCOMH
    OLED_WriteCmd(0x30);
    OLED_WriteCmd(0xA4); // 全局显示开启
    OLED_WriteCmd(0xA6); // 正常显示
    OLED_WriteCmd(0x8D); // 电荷泵
    OLED_WriteCmd(0x14); 

    OLED_Clear_Buffer();
    OLED_WriteCmd(0xAF); // 开启显示 (最重要的一步)
    HAL_Delay(50);
    OLED_Refresh_DMA();  
}

/* ===================== 刷新控制 ===================== */

void OLED_Refresh_DMA(void)
{
    // 改为软件 I2C 连续写入，模拟 DMA 的块传输效果
    uint8_t i, j;
    IIC_Start();
    IIC_SendByte(OLED_HW_ADDRESS);
    IIC_SendByte(0x40); // 连续发数据
    for(j = 0; j < OLED_PAGES; j++)
    {
        for (i = 0; i < OLED_WIDTH; i++)
        {
            IIC_SendByte(OLED_Buffer[j][i]);
        }
    }
    IIC_Stop();
}

/* ===================== 显示功能 (基于 SRAM) ===================== */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--) Result *= X;
    return Result;
}

void OLED_FastShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    uint8_t page_top = (Line - 1) * 2;
    uint8_t col_start = (Column - 1) * 8;
    for (i = 0; i < 8; i++)
    {
        OLED_Buffer[page_top][col_start + i] = OLED_F8x16[Char - ' '][i];
        OLED_Buffer[page_top + 1][col_start + i] = OLED_F8x16[Char - ' '][i + 8];
    }
}

void OLED_FastShowString(uint8_t Line, uint8_t Column, char *String)
{
    for (uint8_t i = 0; String[i] != '\0'; i++) OLED_FastShowChar(Line, Column + i, String[i]);
}

void OLED_FastShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    for (uint8_t i = 0; i < Length; i++)
        OLED_FastShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
}

void OLED_FastShowPicture(uint8_t Pic)
{
    uint8_t i, j;
    uint8_t pic_offset = Pic * 8;
    for(j = 0; j < 8; j++)
    {
        for (i = 0; i < 128; i++)
        {
            OLED_Buffer[j][i] = Picture[j + pic_offset][i]; 
        }
    }
}

/* ===================== 基础显示 (直接写GRAM，效率低) ===================== */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		
    for (i = 0; i < 8; i++) OLED_WriteData(OLED_F8x16[Char - ' '][i]);			
    
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	
    for (i = 0; i < 8; i++) OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++) OLED_ShowChar(Line, Column + i, String[i]);
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
}

void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0) { OLED_ShowChar(Line, Column, '+'); Number1 = Number; }
    else { OLED_ShowChar(Line, Column, '-'); Number1 = -Number; }
    
    for (i = 0; i < Length; i++)
        OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10) OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        else OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
    }
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
}

void OLED_ShowChinese(uint8_t Line, uint8_t Column, uint8_t CHN)
{
    uint8_t i;
    OLED_SetCursor(Line * 2, Column * 16);		
    for (i = 0; i < 16; i++) OLED_WriteData(GBK[CHN][i]);			
    
    OLED_SetCursor((Line * 2) + 1, Column * 16);	
    for (i = 0; i < 16; i++) OLED_WriteData(GBK[CHN][i + 16]);		
}

void OLED_ShowPicture(uint8_t Pic)
{
    uint8_t i, j;
    Pic = Pic * 8;
    for(j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++) OLED_WriteData(Picture[j + Pic][i]);			
    }
}