import re

def rewrite_main():
    with open('../Core/Src/main.c', 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # 1. Add #include "si4732.h" and "i2c.h"
    if '#include "si4732.h"' not in content:
        content = content.replace('/* USER CODE BEGIN Includes */', '/* USER CODE BEGIN Includes */\n#include "si4732.h"\n#include "i2c.h"')
    
    # 2. Add Si4732_Handle_t hRadio
    if 'Si4732_Handle_t hRadio;' not in content and 'Si4732_t hRadio;' not in content:
        content = content.replace('/* USER CODE BEGIN PV */', '/* USER CODE BEGIN PV */\nSi4732_Handle_t hRadio;')
    elif 'Si4732_t hRadio;' in content:
        content = content.replace('Si4732_t hRadio;', 'Si4732_Handle_t hRadio;')

    # 3. Add MX_I2C_Init
    if 'MX_I2C1_Init();' not in content:
        content = content.replace('MX_USART2_UART_Init();', 'MX_USART2_UART_Init();\n  MX_I2C1_Init();\n  MX_I2C2_Init();')

    # 4. Rebuild USER CODE BEGIN 2
    # We will find USER CODE BEGIN 2 and USER CODE END 2 and replace the block
    pattern = re.compile(r'/\* USER CODE BEGIN 2 \*/.*?/\* USER CODE END 2 \*/', re.DOTALL)
    
    new_block = '''/* USER CODE BEGIN 2 */
  myFFT_init();
  HAL_TIM_Base_Start(&htim3);

  // AD9959 Initialization and CH0 setup (86MHz, ~500mVpp)
  ad9959_init();
  ad9959_write_frequency(AD9959_CHANNEL_0, 86000000);
  ad9959_write_amplitude(AD9959_CHANNEL_0, 500);

  // Si4732 Initialization
  Si4732_Init(&hRadio, &hi2c1, RADIO_RST_GPIO_Port, RADIO_RST_Pin);
  Si4732_PowerUp(&hRadio, MODE_FM);
  Si4732_SetVolume(&hRadio, 63);
  Si4732_SetFrequency(&hRadio, 8800);
  /* USER CODE END 2 */'''

    content = re.sub(pattern, new_block, content)

    with open('../Core/Src/main.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == '__main__':
    rewrite_main()
