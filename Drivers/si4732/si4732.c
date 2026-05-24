#include "si4732.h"
#include <stdio.h>
/* * 内部函数：等待 CTS (Clear to Send) 标志位 
 * 逻辑来源：AN332 Section 10 "Timing" 
 * 只有当状态字节的 Bit 7 (0x80) 为 1 时，才能发送下一个命令。
 */
static void Si4732_WaitCTS(Si4732_Handle_t *dev) {
    uint8_t status = 0;
    int timeout = 0;
    
    do {
        // 读取 1 个字节的状态字
        HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, &status, 1, 100);
        HAL_Delay(1); // 还原：确保 I2C 通讯稳定性
        timeout++;
        if(timeout > 2000) break; 
    } while (!(status & 0x80)); // 检查 Bit 7 是否为 1
}

/* * 内部函数：发送通用命令 
 * 相当于 Arduino 的 Wire.write 序列
 */
static void Si4732_WriteCommand(Si4732_Handle_t *dev, uint8_t *cmd, uint16_t len) {
    Si4732_WaitCTS(dev); // 发送前必须确保 CTS 就绪
    HAL_I2C_Master_Transmit(dev->hi2c, SI4732_I2C_ADDR_W, cmd, len, 100);
}

/* * 1. 硬件复位与初始化
 * 对应 C++ 中的 reset()
 */
void Si4732_Init(Si4732_Handle_t *dev, I2C_HandleTypeDef *hi2c, GPIO_TypeDef *port, uint16_t pin) {
    dev->hi2c = hi2c;
    dev->RST_Port = port;
    dev->RST_Pin = pin;

    // 硬件复位时序 (RST 引脚拉低再拉高)
    HAL_GPIO_WritePin(dev->RST_Port, dev->RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(30);
    HAL_GPIO_WritePin(dev->RST_Port, dev->RST_Pin, GPIO_PIN_SET);
    HAL_Delay(30);
}

/* * 2. 上电 (Power Up)
 * 对应 AN332 Section 7.1 和 Command 0x01
 */
//void Si4732_PowerUp(Si4732_Handle_t *dev) {
//    /* * 命令结构 (3字节):
//     * Byte 0: CMD = 0x01 (POWER_UP)
//     * Byte 1: ARG1 = 0x10 (晶振模式 Crystal Oscillator, CTS中断禁用)
//     * Byte 2: ARG2 = 0x05 (模拟输出 Analog Output)
//     */
//    uint8_t cmd[] = {CMD_POWER_UP, 0x10, 0x05};
//    
//    // 注意：这是上电后的第一条指令，不需要 WaitCTS，直接发
//    HAL_I2C_Master_Transmit(dev->hi2c, SI4732_I2C_ADDR_W, cmd, sizeof(cmd), 100);
//    
//    // 上电需要较长时间稳定，参考 C++ 代码中的 delay(maxDelayAfterPowerUp)
//    HAL_Delay(500); 
//}
/* * 改进后的 PowerUp，支持 FM 和 AM 切换
 * mode: MODE_FM (0x00) 或 MODE_AM (0x01)
 */
void Si4732_PowerUp(Si4732_Handle_t *dev, uint8_t mode) {
    // 1. 先断电复位，确保干净的状态
    HAL_GPIO_WritePin(dev->RST_Port, dev->RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(30);
    HAL_GPIO_WritePin(dev->RST_Port, dev->RST_Pin, GPIO_PIN_SET);
    HAL_Delay(30);

    /* 命令结构:
     * Byte 0: 0x01 (POWER_UP)
     * Byte 1: 0x11 (Arg1) -> Bit 4=1(Crystal), Bit 0-3=Function(0=FM, 1=AM)
     * Byte 2: 0x05 (Arg2) -> Analog Output
     */
    uint8_t func = (mode == MODE_AM) ? 0x01 : 0x00;
    uint8_t arg1 = 0x10 | func; // 0x10(晶振) | 模式
    
    uint8_t cmd[] = {0x01, arg1, 0x05};
    
    HAL_I2C_Master_Transmit(dev->hi2c, SI4732_I2C_ADDR_W, cmd, sizeof(cmd), 100);
    HAL_Delay(500); // AM 启动可能需要更久，给足时间
}
/* * 3. 设置属性 (Set Property)
 * 对应 AN332 Command 0x12
 * 通用函数，用于设置音量、步进等任何参数
 */
 void Si4732_SetProperty(Si4732_Handle_t *dev, uint16_t propID, uint16_t propValue) {
    uint8_t cmd[6];
    
    cmd[0] = CMD_SET_PROPERTY;
    cmd[1] = 0x00;              // ARG1 固定为 0
    cmd[2] = (propID >> 8);     // Prop ID High
    cmd[3] = (propID & 0xFF);   // Prop ID Low
    cmd[4] = (propValue >> 8);  // Value High
    cmd[5] = (propValue & 0xFF);// Value Low
    
    Si4732_WriteCommand(dev, cmd, 6);
}

/*
 * 4. 设置音量
 */
void Si4732_SetVolume(Si4732_Handle_t *dev, uint8_t volume) {
    if (volume > 63) volume = 63;
    Si4732_SetProperty(dev, PROP_FM_VOLUME, volume);
}

/* * 5. 调频 (Tune Frequency)
 * 对应 AN332 Command 0x20
 * 参数 freq_10khz: 例如 103.9MHz 传入 10390
 */
void Si4732_SetFrequency(Si4732_Handle_t *dev, uint16_t freq_10khz) {
    uint8_t cmd[5];
    
    cmd[0] = CMD_FM_TUNE_FREQ;
    cmd[1] = 0x00; // 还原：禁用 FAST 模式，确保信号锁定稳定性
    cmd[2] = (freq_10khz >> 8);   // Freq High Byte
    cmd[3] = (freq_10khz & 0xFF); // Freq Low Byte
    cmd[4] = 0x00; // ANT_CAP High (0 = 自动调谐电容)
    
    Si4732_WriteCommand(dev, cmd, 5);
    
    // 还原：使用 100ms 稳定延时
    HAL_Delay(100); 
}
/* 配置搜台范围和步进 */
void Si4732_ConfigSeek(Si4732_Handle_t *dev) {
    // 设置搜索下限 88.0 MHz (8800)
    Si4732_SetProperty(dev, 0x1400, 8800); 
    
    // 设置搜索上限 108.0 MHz (10800)
    Si4732_SetProperty(dev, 0x1401, 10800);
    
    // 设置步进 100 kHz (10)
    Si4732_SetProperty(dev, 0x1402, 10);
    
    // (可选) 设置信噪比阈值，值越小搜到的台越多但也越多噪点
    // 0x1403 FM_SEEK_TUNE_SNR_THRESHOLD, 默认 3dB
    // Si4732_SetProperty(dev, 0x1403, 3); // 之前的 SNR 门限
    Si4732_SetProperty(dev, 0x1403, 0);    // 调低 SNR 门限（设为 0），提高搜台灵敏度

    // 2. 设置 RSSI 阈值 (Property 0x1404)
    // 设为 0x05 (5 dBuV)，默认是 20。这能让微弱信号也被锁住。
    // Si4732_SetProperty(dev, 0x1404, 0x15); // 之前的 RSSI 门限 (21 dBuV)
    Si4732_SetProperty(dev, 0x1404, 5);       // 恢复为更灵敏的 5 dBuV，以支持解调 -84dBm 的弱信号
}
/* * 自动搜台函数
 * direction: 1 = 向上频率搜索, 0 = 向下频率搜索
 * 返回值: 1 = 搜台成功, 0 = 失败/超时
 */
uint8_t Si4732_Seek(Si4732_Handle_t *dev, uint8_t direction) {
    uint8_t cmd[2];
    uint8_t status = 0;
    int timeout = 0;

    /* 命令生成: 0x21 FM_SEEK_START */
    cmd[0] = 0x21; 
    
    /* Arg1 配置:
     * Bit 3 (SEEKUP): 1=Up, 0=Down
     * Bit 2 (WRAP): 1=循环搜索(搜到头回到尾), 0=不循环
     */
    cmd[1] = 0x00; // 还原：禁用 FAST 位，防止在搜弱台时错过信号
    if(direction) cmd[1] |= (1 << 3); // Set SEEKUP bit
    cmd[1] |= (1 << 2); // Set WRAP bit (推荐开启循环)

    // 发送命令
    Si4732_WriteCommand(dev, cmd, 2);

    // *** 关键点：等待 STC (搜台完成) ***
    // 还原：增加轮询延时，确保芯片有足够时间评估弱信号
    do {
        HAL_Delay(10); // 还原回 10ms
        HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, &status, 1, 100);
        timeout++;
        
        // 超时保护
        if(timeout > 300) return 0; 
        
    } while (!(status & 0x01)); // 等待 Status Byte 的 Bit 0 (STCINT)变 1
    uint8_t clear_cmd[] = {0x22, 0x01}; 
    Si4732_WriteCommand(dev, clear_cmd, 2);
    return 1; // 搜台成功
}
/* * 获取当前频率和信号质量
 * 返回值: 频率 (单位 10kHz，例如 10390 代表 103.9MHz)
 */
uint16_t Si4732_GetCurrFrequency(Si4732_Handle_t *dev) {
    uint8_t cmd[2];
    uint8_t resp[8]; // 响应数据很长，我们需要读取多个字节
    uint16_t freq = 0;

    /* 命令: 0x22 FM_TUNE_STATUS */
    cmd[0] = 0x22;
    /* Arg1: 0x01 (INTACK) -> 非常重要！
     * 这会清除之前的 STC 完成标志位，为下一次搜台做准备 
     */
    cmd[1] = 0x01; 

    // 发送查询命令
    Si4732_WriteCommand(dev, cmd, 2);

    // 读取响应 (根据 AN332 Table 2，响应包含 Status + Resp1~7)
    // 我们至少需要读到 Resp3 才能拼出频率
    HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, resp, 4, 100);

    /* 解析频率:
     * Resp2 = High Byte (READFREQH)
     * Resp3 = Low Byte (READFREQL)
     * 注意：I2C读取数组中，resp[0]是Status字节，所以 Resp2 是 resp[2]
     */
    freq = ((uint16_t)resp[2] << 8) | resp[3];
    
    return freq;
}
/* si4732.c */

/* 获取当前信号的强度(RSSI)和信噪比(SNR) */
void Si4732_GetSignalQuality(Si4732_Handle_t *dev, uint8_t *pRSSI, uint8_t *pSNR) {
    uint8_t cmd[2];
    uint8_t resp[8]; 

    /* 命令 0x23: FM_RSQ_STATUS */
    cmd[0] = 0x23;
    cmd[1] = 0x00; // Arg1: 0x00 (只读取，不清除任何标志位)

    Si4732_WriteCommand(dev, cmd, 2);
    
    // 读取返回数据 (Status + Resp1...Resp7)
    HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, resp, 8, 100);

    /* 解析数据 (参考 AN332) */
    // Resp4 是 RSSI (在数组中是下标4)
    // Resp5 是 SNR  (在数组中是下标5)
    if (pRSSI != NULL) *pRSSI = resp[4];
    if (pSNR != NULL)  *pSNR  = resp[5];
}

//AM
/* * AM 调频函数
 * freq_khz: 直接传入 kHz，例如 603 代表 603kHz
 */
void Si4732_SetAMFrequency(Si4732_Handle_t *dev, uint16_t freq_khz) {
    uint8_t cmd[6];
    
    cmd[0] = CMD_AM_TUNE_FREQ; // 0x40
    cmd[1] = 0x00;             // Arg1: Fast=0
    cmd[2] = (freq_khz >> 8);  // Freq High
    cmd[3] = (freq_khz & 0xFF);// Freq Low
    cmd[4] = 0x00;             // ANT_CAP High (0 = 自动调谐)
    cmd[5] = 0x01;             // ANT_CAP Low
    
    Si4732_WriteCommand(dev, cmd, 6);
    
    // 等待 STC (调谐完成)
    uint8_t status = 0;
    int timeout = 0;
    do {
        HAL_Delay(10); 
        HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, &status, 1, 100);
        timeout++;
        if(timeout > 200) break;
    } while (!(status & 0x01));
    
    // 清除 STC 标志 (用 AM_TUNE_STATUS 0x42)
    uint8_t clear_cmd[] = {CMD_AM_TUNE_STATUS, 0x01};
    Si4732_WriteCommand(dev, clear_cmd, 2);
}

/* * 读取当前 AM 频率
 */
uint16_t Si4732_GetAMFrequency(Si4732_Handle_t *dev) {
    uint8_t cmd[2];
    uint8_t resp[8]; 
    uint16_t freq = 0;

    cmd[0] = CMD_AM_TUNE_STATUS; // 0x42 (注意跟 FM 不一样)
    cmd[1] = 0x01; // INTACK

    Si4732_WriteCommand(dev, cmd, 2);
    HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, resp, 4, 100);

    freq = ((uint16_t)resp[2] << 8) | resp[3];
    return freq;
}

/*
 * 配置 AM 搜台参数 (MW 中波)
 * 中国/欧洲标准：522 - 1620 kHz，步进 9kHz
 */
void Si4732_ConfigAM_Seek(Si4732_Handle_t *dev) {
    // 范围 520 - 1710 kHz
    Si4732_SetProperty(dev, PROP_AM_SEEK_BAND_BOTTOM, 520);
    Si4732_SetProperty(dev, PROP_AM_SEEK_BAND_TOP, 1710);
    
    // 步进 9kHz (中国/欧洲), 美国是 10kHz
    Si4732_SetProperty(dev, PROP_AM_SEEK_FREQ_SPACING,10);
    
    // AM 的阈值通常要设低一点，因为环境噪声大
    // AM_SEEK_TUNE_SNR_THRESHOLD (0x3403)
    Si4732_SetProperty(dev, 0x3403, 1); 
    
    // AM_SEEK_TUNE_RSSI_THRESHOLD (0x3404)
    Si4732_SetProperty(dev, 0x3404, 1); 
}

//AM搜台功能
/* * AM 自动搜台
 * direction: 1 = 向上 (频率增加), 0 = 向下 (频率减小)
 * 返回值: 1 = 成功, 0 = 失败/超时
 */
uint8_t Si4732_AM_Seek(Si4732_Handle_t *dev, uint8_t direction) {
    uint8_t cmd[2];
    uint8_t status = 0;
    int timeout = 0;

    /* 命令: 0x41 AM_SEEK_START (注意不是 0x21) */
    cmd[0] = CMD_AM_SEEK_START; 
    
    /* Arg1 配置:
     * Bit 3 (SEEKUP): 1=Up, 0=Down
     * Bit 2 (WRAP): 1=循环搜索
     */
    cmd[1] = 0x00; 
    if(direction) cmd[1] |= (1 << 3); 
    cmd[1] |= (1 << 2); 

    // 发送命令
    Si4732_WriteCommand(dev, cmd, 2);

    // 等待 STC (搜台完成)
    do {
        HAL_Delay(10); 
        HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, &status, 1, 100);
        timeout++;
        if(timeout > 500) return 0; // AM 搜台通常比 FM 慢，给 5秒超时
    } while (!(status & 0x01)); // 等待 Bit 0 (STCINT)

    /* 清除 STC 标志 */
    // 注意：AM 必须用 0x42 (AM_TUNE_STATUS) 来清除，不能用 FM 的 0x22
    uint8_t clear_cmd[] = {CMD_AM_TUNE_STATUS, 0x01}; 
    Si4732_WriteCommand(dev, clear_cmd, 2);

    return 1; 
}
/* 获取 AM 信号质量 (RSSI / SNR) */
void Si4732_GetAMSignalQuality(Si4732_Handle_t *dev, uint8_t *pRSSI, uint8_t *pSNR) {
    uint8_t cmd[2];
    uint8_t resp[8]; 

    /* 命令: 0x49 AM_RSQ_STATUS */
    cmd[0] = CMD_AM_RSQ_STATUS;
    cmd[1] = 0x00; // Arg1: 0x00 (只读)

    Si4732_WriteCommand(dev, cmd, 2);
    Si4732_WaitCTS(dev);
    // 读取返回数据
   if( HAL_I2C_Master_Receive(dev->hi2c, SI4732_I2C_ADDR_R, resp, 8, 100)==HAL_OK)
	 {
    /* 解析数据 (参考 AN332 Table 20) */
    // Resp4 = RSSI
    // Resp5 = SNR
    if (pRSSI != NULL) *pRSSI = resp[4];
    if (pSNR != NULL)  *pSNR  = resp[5];
	 }
	 else
	 {
	 printf("I2C Read Error!\r\n");
	 }
}