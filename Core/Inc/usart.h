/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

/* USER CODE BEGIN Private defines */
/* 收发周期与窗口时间（单位：ms） */
#define UART_CYCLE_MS        1000U  /* 总周期 1s                      */
#define UART_RX_WINDOW_MS     500U  /* 前 0.5s：阻塞式接收窗口        */
#define UART_TX_WINDOW_MS     500U  /* 后 0.5s：阻塞式发送窗口        */
#define UART_RX_GAP_MS         30U  /* 帧内字节间隙超时（判定帧结束） */

#define UART_RX_BUF_SIZE      128U  /* 单帧最大数据字节数             */
#define UART_DATA_MAX_NUM       4U  /* 最多解析 a/b/c/d 四个数值      */
#define UART_TX_BUF_SIZE       80U  /* 发送缓冲区大小                 */
#define UART_TX_DECIMALS        2U  /* 发送时保留的小数位             */
/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);

/* USER CODE BEGIN Prototypes */
/* 阻塞式接收一帧：window_ms 为整帧最长等待时间，gap_ms 为字节间隙超时。
 * 返回值：实际接收到的字节数（0 表示本窗口内未收到有效数据）。 */
uint16_t Uart_ReceiveFrame(uint8_t *buf, uint16_t max_len, uint32_t window_ms, uint32_t gap_ms);

/* 解析形如 "a=22.33 b=34.55 c=44.55" 的字符串，按出现顺序写入 values[]。
 * 返回值：成功解析出的数值个数。 */
uint8_t Uart_ParseFrame(const char *frame, float *values, uint8_t max_num);

/* 阻塞式发送：按 "a=xx.xx b=xx.xx ..." 格式发送，保留 2 位小数。 */
HAL_StatusTypeDef Uart_SendValues(const float *values, uint8_t num);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

