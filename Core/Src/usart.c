/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA1_Channel5;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart1_rx);

    /* USART1_TX Init */
    hdma_usart1_tx.Instance = DMA1_Channel4;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart1_tx);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/**
  * @brief  从字符串中解析一个浮点数（支持正负号、整数、小数部分）
  * @param  src   输入：待解析字符串指针；输出：指向数值之后的位置
  * @param  value 解析结果
  * @retval 1 解析成功，0 解析失败
  */
static uint8_t Uart_ParseFloat(const char **src, float *value)
{
  const char *p = *src;
  float result = 0.0f;
  float scale = 0.1f;
  uint8_t negative = 0U;
  uint8_t has_digit = 0U;

  while (*p == ' ')
  {
    p++;
  }

  if ((*p == '+') || (*p == '-'))
  {
    negative = (*p == '-') ? 1U : 0U;
    p++;
  }

  while ((*p >= '0') && (*p <= '9'))
  {
    result = (result * 10.0f) + (float)(*p - '0');
    has_digit = 1U;
    p++;
  }

  if (*p == '.')
  {
    p++;
    while ((*p >= '0') && (*p <= '9'))
    {
      result += (float)(*p - '0') * scale;
      scale *= 0.1f;
      has_digit = 1U;
      p++;
    }
  }

  if (has_digit == 0U)
  {
    return 0U;
  }

  *value = (negative != 0U) ? -result : result;
  *src = p;
  return 1U;
}

/**
  * @brief  浮点数转字符串，固定小数位数（不依赖 printf 浮点支持）
  * @param  out      输出缓冲区（需至少 16 字节）
  * @param  value    待转换数值
  * @param  decimals 小数位数
  * @retval 写入的字符数（不含结束符 '\0'）
  */
static uint16_t Uart_FloatToStr(char *out, float value, uint8_t decimals)
{
  char tmp[12];
  uint16_t pos = 0U;
  uint8_t  n = 0U;
  uint8_t  i;
  uint32_t scale = 1U;
  uint32_t scaled;
  uint32_t int_part;
  uint32_t frac_part;

  if (value < 0.0f)
  {
    out[pos++] = '-';
    value = -value;
  }

  for (i = 0U; i < decimals; i++)
  {
    scale *= 10U;
  }

  if (value > 999999.0f)          /* 防止整数部分溢出 */
  {
    value = 999999.0f;
  }

  scaled = (uint32_t)((value * (float)scale) + 0.5f);   /* 四舍五入 */
  int_part  = scaled / scale;
  frac_part = scaled % scale;

  if (int_part == 0U)
  {
    tmp[n++] = '0';
  }
  while (int_part > 0U)
  {
    tmp[n++] = (char)('0' + (int_part % 10U));
    int_part /= 10U;
  }
  while (n > 0U)
  {
    out[pos++] = tmp[--n];
  }

  if (decimals > 0U)
  {
    out[pos++] = '.';
    for (i = decimals; i > 0U; i--)
    {
      scale /= 10U;
      out[pos++] = (char)('0' + ((frac_part / scale) % 10U));
    }
  }

  out[pos] = '\0';
  return pos;
}

/**
  * @brief  阻塞式接收一帧不定长数据（以字节间隙超时或 '\n' 判定帧结束）
  * @param  buf       接收缓冲区（容量需为 max_len + 1，用于自动补 '\0'）
  * @param  max_len   单帧最多接收的字节数
  * @param  window_ms 整帧最长等待时间，即 0.5s 接收窗口
  * @param  gap_ms    字节之间的最大允许间隔，超时即认为一帧结束
  * @retval 实际接收到的字节数，0 表示未收到数据
  */
uint16_t Uart_ReceiveFrame(uint8_t *buf, uint16_t max_len, uint32_t window_ms, uint32_t gap_ms)
{
  uint32_t start;
  uint32_t elapsed;
  uint32_t remain;
  uint32_t timeout;
  uint16_t len = 0U;
  uint8_t  ch;

  if ((buf == NULL) || (max_len == 0U))
  {
    return 0U;
  }

  start = HAL_GetTick();

  while (1)
  {
    elapsed = HAL_GetTick() - start;
    if (elapsed >= window_ms)
    {
      break;
    }

    remain  = window_ms - elapsed;
    timeout = (remain < gap_ms) ? remain : gap_ms;

    if (HAL_UART_Receive(&huart1, &ch, 1U, timeout) == HAL_OK)
    {
      /* 帧首的空行/回车间隔符直接丢弃 */
      if ((len == 0U) && ((ch == '\r') || (ch == '\n')))
      {
        continue;
      }

      buf[len++] = ch;

      if ((ch == '\n') || (len >= max_len))
      {
        break;
      }
    }
    else
    {
      /* 间隙超时：已收到数据则本帧结束，否则继续等待帧起始 */
      if (len > 0U)
      {
        break;
      }
    }
  }

  buf[len] = '\0';
  return len;
}

/**
  * @brief  解析 "a=22.33 b=34.55 c=44.55" 形式的字符串
  * @param  frame   待解析字符串（以 '\0' 结尾）
  * @param  values  解析结果数组
  * @param  max_num values 数组最大容量
  * @retval 成功解析出的数值个数（按出现顺序写入 values）
  */
uint8_t Uart_ParseFrame(const char *frame, float *values, uint8_t max_num)
{
  const char *p = frame;
  uint8_t cnt = 0U;
  float value;

  if ((frame == NULL) || (values == NULL) || (max_num == 0U))
  {
    return 0U;
  }

  while ((*p != '\0') && (cnt < max_num))
  {
    if (*p == '=')
    {
      p++;                                /* 跳过 '='，解析其后的数值 */
      if (Uart_ParseFloat(&p, &value) != 0U)
      {
        values[cnt++] = value;
      }
      else
      {
        while ((*p != '\0') && (*p != '='))
        {
          p++;                            /* 跳过非法内容 */
        }
      }
    }
    else
    {
      p++;                                /* 跳过变量名与分隔符 */
    }
  }

  return cnt;
}

/**
  * @brief  阻塞式发送解析结果，格式 "a=xx.xx b=xx.xx ..."，保留两位小数
  * @param  values 数值数组
  * @param  num    数值个数（a/b/c/d 依次对应）
  * @retval HAL 状态
  */
HAL_StatusTypeDef Uart_SendValues(const float *values, uint8_t num)
{
  static const char names[UART_DATA_MAX_NUM] = {'a', 'b', 'c', 'd'};
  char tx_buf[UART_TX_BUF_SIZE];
  uint16_t pos = 0U;
  uint8_t  i;

  if ((values == NULL) || (num == 0U))
  {
    return HAL_ERROR;
  }

  if (num > UART_DATA_MAX_NUM)
  {
    num = UART_DATA_MAX_NUM;
  }

  for (i = 0U; i < num; i++)
  {
    if (i > 0U)
    {
      tx_buf[pos++] = ' ';
    }
    tx_buf[pos++] = names[i];
    tx_buf[pos++] = '=';
    pos += Uart_FloatToStr(&tx_buf[pos], values[i], UART_TX_DECIMALS);
  }

  tx_buf[pos++] = '\r';
  tx_buf[pos++] = '\n';

  return HAL_UART_Transmit(&huart1, (uint8_t *)tx_buf, pos, HAL_MAX_DELAY);
}

/* USER CODE END 1 */

