/**
  ******************************************************************************
  * @file    dma_test.h
  * @brief   USART1 DMA + 环形缓冲区 不定长收发，以及 LED 命令解析
  ******************************************************************************
  * 使用方式：
  *   1. main() 中初始化外设后调用 DmaTest_Init()；
  *   2. 主循环中不断调用 DmaTest_Poll()；
  *   3. 中断处理由 HAL_UARTEx_RxEventCallback / HAL_UART_ErrorCallback /
  *      HAL_UART_TxCpltCallback 回调完成，无需修改 stm32f1xx_it.c。
  *
  * 接收规则：
  *   - USART1_RX 由 DMA1_Channel5 循环模式直接搬运到 512 字节硬件环形缓冲，
  *     再经 IDLE 空闲中断搬运到 1024 字节软件环形缓冲，任意时刻不丢失数据；
  *   - 一帧以"总线空闲"划分，最大 256 字节（200 字节实测无丢包）；
  *   - 帧内出现 "led on"  -> PB9 输出高电平；
  *   - 帧内出现 "led off" -> PB9 输出低电平；
  *   - 无论是否匹配命令，均将该帧原样通过 USART1 DMA 发回上位机。
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DMA_TEST_H__
#define __DMA_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private defines -----------------------------------------------------------*/
#define DMATEST_DMA_BUF_SIZE    512U    /* DMA 硬件环形缓冲区大小，必须为 2 的幂 */
#define DMATEST_RING_SIZE      1024U    /* 软件环形缓冲区大小，必须为 2 的幂     */
#define DMATEST_FRAME_MAX       256U    /* 单帧最大字节数                        */
#define DMATEST_FRAME_QUEUE       8U    /* 待处理帧队列深度                      */
#define DMATEST_TX_BUF_SIZE     256U    /* DMA 发送缓冲区大小                    */

#define DMATEST_LED_PORT       GPIOB    /* 板载 LED 端口                         */
#define DMATEST_LED_PIN        GPIO_PIN_9

/* Exported functions --------------------------------------------------------*/
void     DmaTest_Init(void);                                  /* 启动环形 DMA 接收 */
void     DmaTest_Poll(void);                                  /* 主循环轮询处理    */
uint8_t  DmaTest_Send(const uint8_t *data, uint16_t len);     /* DMA 阻塞等待发送  */
uint32_t DmaTest_GetDropCount(void);                          /* 累计丢字节数      */

#ifdef __cplusplus
}
#endif

#endif /* __DMA_TEST_H__ */
