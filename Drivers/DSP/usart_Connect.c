#include "usart_Connect.h"


//接收变量
volatile uint8_t rx_end_flag=0;					//接收标志位
uint8_t FFT_Accomplish_TAG = 0; 					//接收使用标志位

uint32_t temp_rx_buffer_leng=0;  				  //临时接收数据缓存长度
uint32_t temp_tx_buffer_leng=0;  				  //临时发送数据缓存长度


uint8_t temp_rx_buffer[RxBuffMaxLEN+1]; //临时接收缓存	
char temp_tx_buffer[TxBuffMaxLEN+1];  	// 临时发送缓冲区，根据需要调整大小 



// 封装用于指令发送函数
void connect_Printf(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    uint16_t len = vsnprintf(temp_tx_buffer, sizeof(char)*(TxBuffMaxLEN+1), format, args);  // 格式化指令
    va_end(args);
    // 发送格式化后的字符串
    HAL_UART_Transmit(&huart2, (uint8_t *)temp_tx_buffer, len, 10);
}

// 封装用于指令发送函数
void connect_DMA_Printf(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    uint16_t len = vsnprintf(temp_tx_buffer, sizeof(uint8_t)*(TxBuffMaxLEN+1), format, args);  // 格式化指令
    va_end(args);
    // 发送格式化后的字符串
	if(__HAL_DMA_GET_COUNTER(huart1.hdmatx) == 0 )                                // 检查上次数据是否发送完成   
  {

    HAL_UART_DMAStop(&huart2);                                                  // 关闭DMA
    HAL_UART_Transmit_DMA(&huart2,(uint8_t *)temp_tx_buffer,len);                 // 启动DMA发送 
    
  }
  else
	{
			 // 停止串口
      // HAL_UART_DeInit(&huart2);
			//MX_USART2_UART_Init();
		   HAL_UART_DMAStop(&huart2);                                                  // 关闭DMA
       HAL_UART_Transmit_DMA(&huart2,(uint8_t *)temp_tx_buffer,len);                 // 启动DMA发
		   
  }
	
	
}



#include "usart.h"
extern DMA_HandleTypeDef hdma_usart2_rx;
extern DMA_HandleTypeDef hdma_usart2_tx;
uint8_t temp_rx_data;//单个数据帧

void connectUART_init()
{
	
	
	/*正常程序*/
	rx_end_flag=0;
	FFT_Accomplish_TAG = 0;
	temp_rx_buffer_leng=0;
	temp_tx_buffer_leng=0;
	memset(temp_tx_buffer,0,sizeof(char)*TxBuffMaxLEN);
	memset(temp_rx_buffer,0,sizeof(uint8_t)*RxBuffMaxLEN);
	
	HAL_UART_Receive_IT(&huart2,&temp_rx_data,1); // 使能接收中断

  __HAL_UART_ENABLE_IT(&huart2,UART_IT_IDLE); // 使能空闲中断
}

// 发送完成中断，打开接收
void USART2_EndTxd_IRQHandler(void)
{
  temp_tx_buffer_leng=0;    // 发送完成
}


void connect_Task(void)
{

	
    if(USART2 == huart2.Instance)
		{
			  if(__HAL_UART_GET_FLAG(&huart2,UART_FLAG_RXNE) != RESET)
				{
						HAL_UART_IRQHandler(&huart2); // 该函数会清空中断标志，取消中断使能，并间接调用回调函数,清除接收标志

						// 收到数据，将其放入缓冲区
						if(temp_rx_buffer_leng < RxBuffMaxLEN)
						{
							
							temp_rx_buffer[temp_rx_buffer_leng] = temp_rx_data;							
							#if	Ring_queue	
							ringBuff.Lenght =  RINGBUFF_LEN - temp; 
							#else
							temp_rx_buffer_leng++;
							#endif

						}
								
				}

				if(__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE) != RESET)
				{
						__HAL_UART_CLEAR_IDLEFLAG(&huart2);     //清楚空闲中断标志（否则会一直不断进入中断）

						rx_end_flag = 1;  // 标记接收完成
				}

				HAL_UART_Receive_IT(&huart2,&temp_rx_data,1);   // 重新使能接收中断
			

    }
}


const uint8_t Tag_FPGA[1]={0x55};
void Usart2_Task(void)
{
								/*标志位归零*/
	if(rx_end_flag == 1)
	{
		HAL_UART_Transmit_DMA(&huart1,temp_rx_buffer,sizeof(uint8_t)*temp_rx_buffer_leng);	//DMA发送
			#if Ring_queue
        ringBuff.Lenght = 0;//清除计数
			#else
				temp_rx_buffer_leng = 0;//数据长度归零
			#endif
				rx_end_flag = 0;//清除接收结束标志位
		
		if(FFT_Accomplish_TAG==1)//如果使用完成就清空，避免出问题
		{
//		HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);		
    memset(temp_rx_buffer,0,sizeof(uint8_t)*(RxBuffMaxLEN+1));		
		}

			//与FPGA通信需要
//		HAL_UART_Transmit(&huart2, (uint8_t *)Tag_FPGA, sizeof(Tag_FPGA), 2);//发送给FPGA接收完成标志		
	}
		
}







