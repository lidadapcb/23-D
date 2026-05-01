#include "ad9959.h"

// AD9959寄存器默认值定义
// FR2: 功能寄存器2，默认值 0x0000
uint8_t FR2_DATA[2] = {0x20, 0x00};
// CFR: 通道功能寄存器，默认值 0x000302
uint8_t CFR_DATA[3] = {0x00, 0x03, 0x02};
// CPOW0: 通道相位偏移字0，默认值 0x0000，相位计算公式: Phase = (POW / 2^14) * 360 degrees
uint8_t CPOW0_DATA[2] = {0x00, 0x00};
// LSRR: 线性扫频斜率寄存器，默认未使用
uint8_t LSRR_DATA[2] = {0x00, 0x00};
// RDW:  ramp步长字，默认未使用
uint8_t RDW_DATA[4] = {0x00, 0x00, 0x00, 0x00};
// FDW: 频率步进字，默认未使用
uint8_t FDW_DATA[4] = {0x00, 0x00, 0x00, 0x00};

/**
* @brief AD9959软件延时函数
* @param length 延时长度，内部会乘以12以延长延时时间
* @note 这是一个简单的阻塞式延时，具体延时时间取决于系统主频
* */
static void ad9959_delay(uint32_t length) {
    length = length * 30;
    while (length--);
}

/**
* @brief AD9959初始化函数
* @note 配置PLL倍频系数、电荷泵电流，并初始化所有通道的频率、相位和幅度
* */
void ad9959_init(void) {
    // FR1: 功能寄存器1
    // 0xD0: 
    //   Bit7-4 (1101): PLL multiplier = 20 (参考时钟25MHz * 20 = 500MHz系统时钟)
    //   Bit3-0 (0000): Charge pump current control (需结合具体硬件调整，此处默认)
    // 0x00, 0x00: VCO gain control = 0 (当系统时钟低于160MHz时设为0，此处500MHz应检查数据手册是否需调整，通常高频率需特定设置)
    uint8_t FR1_DATA[3] = {0xD0, 0x00, 0x00};

    // 初始化IO引脚状态
    ad9959_io_init();
    // 执行硬件复位序列
    ad9959_reset();

    // 写入功能寄存器1 (FR1)，配置PLL和系统时钟
    ad9959_write_data(AD9959_REG_FR1, 3, FR1_DATA, 1);
    // 写入功能寄存器2 (FR2)
    ad9959_write_data(AD9959_REG_FR2, 2, FR2_DATA, 1);

    // 初始化所有4个通道的输出频率为 1000 Hz
    ad9959_write_frequency(AD9959_CHANNEL_0, 1000);
    ad9959_write_frequency(AD9959_CHANNEL_1, 1000);
    ad9959_write_frequency(AD9959_CHANNEL_2, 1000);
    ad9959_write_frequency(AD9959_CHANNEL_3, 1000);

    // 初始化所有4个通道的输出相位为 0
    ad9959_write_phase(AD9959_CHANNEL_0, 0);
    ad9959_write_phase(AD9959_CHANNEL_1, 0);
    ad9959_write_phase(AD9959_CHANNEL_2, 0);
    ad9959_write_phase(AD9959_CHANNEL_3, 0);

    // 初始化所有4个通道的输出幅度为最大值 (0xFF 对应约满量程)
    ad9959_write_amplitude(AD9959_CHANNEL_0, 0xFF);
    ad9959_write_amplitude(AD9959_CHANNEL_1, 0xFF);
    ad9959_write_amplitude(AD9959_CHANNEL_2, 0xFF);
    ad9959_write_amplitude(AD9959_CHANNEL_3, 0xFF);
}

/**
* @brief AD9959硬件复位序列
* @note 按照数据手册要求，拉低RESET -> 延时 -> 拉高RESET -> 延时 -> 拉低RESET
* */
void ad9959_reset(void) {
    AD9959_RESET_0;       // 拉低复位引脚
    ad9959_delay(1);      // 短延时
    AD9959_RESET_1;       // 拉高复位引脚，保持至少几个系统时钟周期
    ad9959_delay(30);     // 等待内部电路稳定
    AD9959_RESET_0;       // 拉低复位引脚，完成复位序列
}

/**
* @brief AD9959控制IO引脚初始化
* @note 设置CS, SCLK, UPDATE, PS0-3, SDIO等引脚的初始电平状态
* */
void ad9959_io_init(void) {
    AD9959_PDC_0;         // Power Down Control 拉低 (正常工作模式)
    AD9959_CS_1;          // Chip Select 拉高 (禁用SPI)
    AD9959_SCLK_0;        // Serial Clock 拉低 (空闲状态)
    AD9959_UPDATE_0;      // IO Update 拉低
    AD9959_PS0_0;         // Profile Select 0 拉低
    AD9959_PS1_0;         // Profile Select 1 拉低
    AD9959_PS2_0;         // Profile Select 2 拉低
    AD9959_PS3_0;         // Profile Select 3 拉低
    AD9959_SDIO0_0;       // Serial Data IO 0 拉低
    AD9959_SDIO1_0;       // Serial Data IO 1 拉低
    AD9959_SDIO2_0;       // Serial Data IO 2 拉低
    AD9959_SDIO3_0;       // Serial Data IO 3 拉低
}

/**
 * @brief 触发AD9959 IO更新操作
 * @note 在UPDATE引脚上产生一个正脉冲，使之前写入寄存器的数据生效
 * */
void ad9959_io_update(void) {
    AD9959_UPDATE_0;      // 拉低UPDATE引脚
    ad9959_delay(2);      // 保持低电平
    AD9959_UPDATE_1;      // 拉高UPDATE引脚
    ad9959_delay(4);      // 保持高电平，确保被AD9959检测到
    AD9959_UPDATE_0;      // 拉低UPDATE引脚，完成脉冲
}

/**
 * @brief 通过模拟SPI向AD9959指定寄存器写入数据
 * @param register_address 目标寄存器地址 (AD9959_REG_ADDR枚举)
 * @param number_of_registers 要写入的数据字节数
 * @param register_data 指向待发送数据缓冲区的指针
 * @param update 是否在此次写入后自动触发IO Update (true: 触发, false: 不触发)
 * @note 使用SDIO0作为数据线，MSB先行
 * */
void ad9959_write_data(AD9959_REG_ADDR register_address, uint8_t number_of_registers, const uint8_t *register_data,
                       bool update) {
    uint8_t ControlValue = 0;
    uint8_t ValueToWrite = 0;
    uint8_t RegisterIndex = 0;
    uint8_t i = 0;

    // 参数有效性检查
    assert_param(IS_AD9959_REG_ADDR(register_address));

    ControlValue = register_address;
    
    // --- SPI写时序开始 ---
    AD9959_SCLK_0;        // 确保SCLK初始为低
    AD9959_CS_0;          // 拉低CS，选中AD9959

    // 1. 发送8位寄存器地址 (MSB First)
    for (i = 0; i < 8; i++) {
        AD9959_SCLK_0;    // SCLK拉低，准备数据
        // 根据最高位设置SDIO0电平
        if (0x80 == (ControlValue & 0x80))
            AD9959_SDIO0_1;
        else
            AD9959_SDIO0_0;
        AD9959_SCLK_1;    // SCLK拉高，AD9959在上升沿采样数据
        ControlValue <<= 1; // 左移一位，准备下一位
    }
    AD9959_SCLK_0;        // 最后一个字节发送完后，SCLK拉低

    // 2. 发送数据字节
    for (RegisterIndex = 0; RegisterIndex < number_of_registers; RegisterIndex++) {
        ValueToWrite = register_data[RegisterIndex];
        for (i = 0; i < 8; i++) {
            AD9959_SCLK_0;    // SCLK拉低，准备数据
            // 根据最高位设置SDIO0电平
            if (0x80 == (ValueToWrite & 0x80))
                AD9959_SDIO0_1;
            else
                AD9959_SDIO0_0;
            AD9959_SCLK_1;    // SCLK拉高，AD9959在上升沿采样数据
            ValueToWrite <<= 1; // 左移一位，准备下一位
        }
        AD9959_SCLK_0;      // 当前字节发送完，SCLK拉低
    }

    // 如果标记为需要更新，则触发IO Update脉冲
    if (update) ad9959_io_update();
    
    AD9959_CS_1;          // 拉高CS，结束SPI通信
}

/**
 * @brief 设置指定通道的输出相位
 * @param channel 目标通道 (AD9959_CHANNEL_0 ~ 3)
 * @param phase 相位值，14位有效 (0~16383)
 *              对应角度计算公式: Angle = (phase / 16384) * 360 度
 * @note 写入前先通过CSR寄存器选择通道
 * */
void ad9959_write_phase(AD9959_CHANNEL channel, uint16_t phase) {
    uint8_t cs_data = channel; // CSR寄存器数据，用于选择通道
    assert_param(IS_AD9959_CHANNEL(channel));
    
    // 将14位相位值拆分为2个字节 (MSB First)
    CPOW0_DATA[1] = (uint8_t) phase;       // 低8位
    CPOW0_DATA[0] = (uint8_t) (phase >> 8); // 高6位 (高2位补0)
    
    // 1. 写CSR寄存器，选择当前操作的通道
    ad9959_write_data(AD9959_REG_CSR, 1, &cs_data, 0); // 不立即Update
    // 2. 写CPOW0寄存器，设置相位
    ad9959_write_data(AD9959_REG_CPOW0, 2, CPOW0_DATA, 1); // 立即Update生效
}

/**
 * @brief 设置指定通道的输出频率
 * @param channel 目标通道 (AD9959_CHANNEL_0 ~ 3)
 * @param Freq 期望输出频率 (单位: Hz, 范围建议 1 ~ 200,000,000 Hz)
 * @note 频率调谐字(FTW)计算公式: FTW = (Freq * 2^32) / SystemClock
 *       假设系统时钟为500MHz (25MHz ref * 20 PLL), 则系数为 2^32 / 500,000,000 ≈ 8.589934592
 * */
void ad9959_write_frequency(AD9959_CHANNEL channel, uint32_t Freq) {
    uint8_t CFTW0_DATA[4] = {0x00, 0x00, 0x00, 0x00}; // 32位频率调谐字缓冲区
    uint32_t frequency; // 计算后的32位整数值
    uint8_t cs_data = channel;

    assert_param(IS_AD9959_CHANNEL(channel));

    // 计算频率调谐字 (FTW)
    // 注意: 此处硬编码了系统时钟为500MHz的情况。如果PLL倍频系数改变，需修改此系数
    frequency = (uint32_t)(Freq * 8.589934592); 
    
    // 将32位FTW拆分为4个字节 (MSB First)
    CFTW0_DATA[3] = (uint8_t) frequency;       // Byte 0 (LSB) - 注意AD9959数据顺序通常是MSB先传，但数组索引0存高位还是低位需看write_data实现
                                               // 在write_data中，register_data[0]先被发送。
                                               // 通常DDS寄存器MSB在前。
                                               // 这里代码逻辑: 
                                               // CFTW0_DATA[0] = MSB (frequency >> 24)
                                               // CFTW0_DATA[3] = LSB (frequency)
                                               // write_data循环从0到number_of_registers-1发送，即先送MSB，符合SPI MSB First习惯
    CFTW0_DATA[2] = (uint8_t) (frequency >> 8);
    CFTW0_DATA[1] = (uint8_t) (frequency >> 16);
    CFTW0_DATA[0] = (uint8_t) (frequency >> 24);

    // 1. 写CSR寄存器，选择当前操作的通道
    ad9959_write_data(AD9959_REG_CSR, 1, &cs_data, 0);
    // 2. 写CFTW0寄存器，设置频率
    ad9959_write_data(AD9959_REG_CFTW0, 4, CFTW0_DATA, 1);
}

/**
 * @brief 设置指定通道的输出幅度
 * @param channel 目标通道 (AD9959_CHANNEL_0 ~ 3)
 * @param amplitude 幅度值，10位有效 (0~1023)
 *                  对应输出电流/电压，最大值1023对应满量程输出 (约530mV into 50ohm, 具体视负载而定)
 * @note ACR寄存器格式: [Reserved(1bit)][PowerDown(1bit)][Amplitude(10bits)][Reserved(4bits)]? 
 *       实际代码中 amplitude | 0x1000 可能是为了设置某个控制位或保留位，需查阅ACR寄存器定义。
 *       通常ACR高两位可能涉及功率关闭或其他控制。
 * */
void ad9959_write_amplitude(AD9959_CHANNEL channel, uint16_t amplitude) {
    // ACR: Amplitude Control Register
    // 默认值结构可能需要根据具体寄存器定义调整，此处沿用原代码结构
    uint8_t ACR_DATA[3] = {0x00, 0x00, 0x00}; 
    uint8_t cs_data = channel;

    assert_param(IS_AD9959_CHANNEL(channel));

    // 原始代码逻辑: amplitude | 0x1000
    // 0x1000 即第12位为1。AD9959 ACR寄存器通常为16位或更多，但通过SPI写入时可能只关心部分位。
    // 请确认数据手册中ACR寄存器的位定义。如果第12位是Reserved或特定控制位，此操作是必要的。
    amplitude = amplitude | 0x1000;
    
    // 拆分幅度值到ACR数据缓冲区
    // 假设ACR_DATA[0]是高位字节，[2]是低位字节（依据write_data发送顺序）
    ACR_DATA[2] = (uint8_t) amplitude;       // 低8位
    ACR_DATA[1] = (uint8_t) (amplitude >> 8); // 高4位 (含设置的0x1000位)
    // ACR_DATA[0] 保持 0x00，或者根据寄存器定义填充其他控制位

    // 1. 写CSR寄存器，选择当前操作的通道
    ad9959_write_data(AD9959_REG_CSR, 1, &cs_data, 0);
    // 2. 写ACR寄存器，设置幅度
    ad9959_write_data(AD9959_REG_ACR, 3, ACR_DATA, 1);
}

/* 以下注释掉的代码为之前的测试或调试用逻辑，包含频率扫描和GPIO中断处理
 * 如需启用，请取消注释并确保相关变量 (SWEEP, freq) 和函数 (LCD_ShowFloatNum1, delay_x) 已定义
 */
/*  
    if(SWEEP == 1)
    {
      for(uint32_t freq_sw = 1000000; freq_sw < 40000000; freq_sw += 1000000)
    {
        ad9959_write_frequency(AD9959_CHANNEL_3,freq_sw);
        LCD_ShowFloatNum1(20,20,(float)freq_sw/1000000,4,BLACK,WHITE,24);
        HAL_Delay(200);
    }
    SWEEP = 0;
    }
    else
    {
      ;
       //LCD_ShowFloatNum1(10,10,(float)freq/1000000,4,BLACK,WHITE,24);
    }
*/
/* 
    void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == GPIO_PIN_4)
  {
    delay_x();
    if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4) == RESET)
    {
      freq = freq + 1000000;
       ad9959_write_frequency(AD9959_CHANNEL_3,(uint32_t)freq);
      LCD_ShowFloatNum1(20,20,(float)freq/1000000,4,BLACK,WHITE,24);
    }
  }
  if(GPIO_Pin == GPIO_PIN_5)
  {
     delay_x();
    if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_5) == RESET)
    {
      freq = freq - 1000000;
      ad9959_write_frequency(AD9959_CHANNEL_3,(uint32_t)freq);
      LCD_ShowFloatNum1(20,20,(float)freq/1000000,4,BLACK,WHITE,24);
    }
  }
  if(GPIO_Pin == GPIO_PIN_6)
  { delay_x();
    if(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_6) == RESET)
    {
      SWEEP = 1-SWEEP;
    }
    delay_x();
  }
  
}
void delay_x(void)
{
  for(int i = 0;i<3000000;i++)
  {
    ;
  }
} 
*/