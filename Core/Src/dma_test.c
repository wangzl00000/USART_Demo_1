/**
  ******************************************************************************
  * @file    dma_test.c
  * @brief   USART1 DMA + 环形缓冲区 不定长收发实现
  ******************************************************************************
  * 数据流：
  *   USART1_RX --> DMA1_Channel5(循环模式, 512B 硬件环形缓冲) --> IDLE 空闲中断
  *             --> 软件环形缓冲(1024B) --> 帧长度队列 --> DmaTest_Poll()
  *             --> LED 命令解析 + USART1_TX(DMA1_Channel4) 回发
  *
  * 关键点：
  *   - DMA 为循环模式且永不停止，靠 IDLE 事件划分帧边界；
  *   - 每次事件都用 DMA 剩余计数器(CNDTR)计算新增字节，兼容 IDLE/HT/TC；
  *   - 软件环形缓冲解耦中断与主循环，200 字节一帧不会丢包。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "dma_test.h"
#include "usart.h"

/* Private define ------------------------------------------------------------*/
#define DMATEST_RING_MASK   (DMATEST_RING_SIZE - 1U)

/* Private variables ---------------------------------------------------------*/
/* DMA 硬件环形缓冲区（DMA 直接写入，CPU 只读） */
static uint8_t  s_dma_buf[DMATEST_DMA_BUF_SIZE];
static uint16_t s_dma_old_pos;              /* 上一次已搬运到的位置 */

/* 软件环形缓冲区：中断写 head，主循环写 tail */
static uint8_t  s_ring[DMATEST_RING_SIZE];
static volatile uint16_t s_ring_head;
static volatile uint16_t s_ring_tail;
static volatile uint32_t s_drop_bytes;      /* 累计丢弃字节数 */

/* 已接收完成的帧长度队列 */
static volatile uint16_t s_frame_len[DMATEST_FRAME_QUEUE];
static volatile uint8_t  s_frame_head;
static volatile uint8_t  s_frame_tail;

static volatile uint16_t s_frame_acc;       /* 当前帧已累计字节数 */
static volatile uint8_t  s_frame_broken;    /* 当前帧发生溢出/丢字节 */

/* 发送相关 */
static uint8_t  s_tx_buf[DMATEST_TX_BUF_SIZE];
static volatile uint8_t s_tx_busy;

static uint8_t  s_frame_buf[DMATEST_FRAME_MAX];   /* 主循环取帧缓存 */

/* Private function prototypes -----------------------------------------------*/
static void     DmaTest_StartReceive(void);
static void     DmaTest_ResetBuffers(void);
static uint16_t DmaTest_CommitDmaData(void);
static void     DmaTest_RingPush(const uint8_t *src, uint16_t len);
static void     DmaTest_HandleFrame(const uint8_t *frame, uint16_t len);
static uint8_t  DmaTest_Contains(const uint8_t *buf, uint16_t len, const char *key, uint16_t key_len);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  启动环形 DMA 接收（将 RX DMA 切换为循环模式后重新启动）
  * @retval None
  */
static void DmaTest_StartReceive(void)
{
  /* 停止可能正在进行的接收 */
  (void)HAL_UART_AbortReceive(&huart1);

  /* 关键：改为循环模式并重新初始化 DMA（CCR 的 CIRC 位由 HAL_DMA_Init 写入） */
  huart1.hdmarx->Init.Mode = DMA_CIRCULAR;
  if (HAL_DMA_Init(huart1.hdmarx) != HAL_OK)
  {
    Error_Handler();
  }

  s_dma_old_pos = 0U;

  /* 启动 "接收至空闲" 模式，同时打开串口空闲(IDLE)中断 */
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_dma_buf, DMATEST_DMA_BUF_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  清空软件环形缓冲与帧队列（仅错误路径调用）
  * @retval None
  */
static void DmaTest_ResetBuffers(void)
{
  s_ring_head    = 0U;
  s_ring_tail    = 0U;
  s_frame_head   = 0U;
  s_frame_tail   = 0U;
  s_frame_acc    = 0U;
  s_frame_broken = 0U;
}

/**
  * @brief  把 DMA 环形缓冲中新增的数据搬入软件环形缓冲
  * @retval 本次搬运的字节数
  */
static uint16_t DmaTest_CommitDmaData(void)
{
  uint16_t counter = (uint16_t)__HAL_DMA_GET_COUNTER(huart1.hdmarx);
  uint16_t pos;
  uint16_t old = s_dma_old_pos;
  uint16_t added;

  if (counter > DMATEST_DMA_BUF_SIZE)
  {
    counter = 0U;
  }

  /* DMA 写指针相对缓冲区起点的偏移 */
  pos = (uint16_t)(DMATEST_DMA_BUF_SIZE - counter);

  if (pos == old)
  {
    return 0U;
  }

  if (pos > old)
  {
    added = (uint16_t)(pos - old);
    DmaTest_RingPush(&s_dma_buf[old], added);
  }
  else
  {
    /* 发生回绕：先取缓冲尾部，再取缓冲头部 */
    uint16_t tail_len = (uint16_t)(DMATEST_DMA_BUF_SIZE - old);
    added = (uint16_t)(tail_len + pos);
    if (tail_len > 0U)
    {
      DmaTest_RingPush(&s_dma_buf[old], tail_len);
    }
    if (pos > 0U)
    {
      DmaTest_RingPush(&s_dma_buf[0], pos);
    }
  }

  s_dma_old_pos = pos;
  return added;
}

/**
  * @brief  向软件环形缓冲写入数据（满则丢弃并标记当前帧损坏）
  * @param  src 数据源
  * @param  len 字节数
  * @retval None
  */
static void DmaTest_RingPush(const uint8_t *src, uint16_t len)
{
  uint16_t i;

  for (i = 0U; i < len; i++)
  {
    uint16_t next = (uint16_t)((s_ring_head + 1U) & DMATEST_RING_MASK);

    if (next == s_ring_tail)              /* 缓冲区已满 */
    {
      s_drop_bytes++;
      s_frame_broken = 1U;
      return;
    }

    s_ring[s_ring_head] = src[i];
    s_ring_head = next;
  }
}

/**
  * @brief  判断缓冲区中是否包含指定关键字
  * @param  buf     待查找数据
  * @param  len     数据长度
  * @param  key     关键字
  * @param  key_len 关键字长度
  * @retval 1 包含，0 不包含
  */
static uint8_t DmaTest_Contains(const uint8_t *buf, uint16_t len, const char *key, uint16_t key_len)
{
  uint16_t i;
  uint16_t j;

  if ((buf == NULL) || (key == NULL) || (key_len == 0U) || (len < key_len))
  {
    return 0U;
  }

  for (i = 0U; (uint16_t)(i + key_len) <= len; i++)
  {
    for (j = 0U; j < key_len; j++)
    {
      if (buf[i + j] != (uint8_t)key[j])
      {
        break;
      }
    }
    if (j == key_len)
    {
      return 1U;
    }
  }

  return 0U;
}

/**
  * @brief  处理一帧数据：LED 命令 + 原样回发
  * @param  frame 帧数据
  * @param  len   帧长度
  * @retval None
  */
static void DmaTest_HandleFrame(const uint8_t *frame, uint16_t len)
{
  if (DmaTest_Contains(frame, len, "led on", 6U) != 0U)
  {
    HAL_GPIO_WritePin(DMATEST_LED_PORT, DMATEST_LED_PIN, GPIO_PIN_SET);
  }
  else if (DmaTest_Contains(frame, len, "led off", 7U) != 0U)
  {
    HAL_GPIO_WritePin(DMATEST_LED_PORT, DMATEST_LED_PIN, GPIO_PIN_RESET);
  }
  else
  {
    /* 不含 LED 命令，仅回发数据 */
  }

  (void)DmaTest_Send(frame, len);
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  初始化并启动 DMA 环形接收
  * @retval None
  */
void DmaTest_Init(void)
{
  s_drop_bytes = 0U;
  s_tx_busy    = 0U;
  DmaTest_ResetBuffers();
  DmaTest_StartReceive();
}

/**
  * @brief  主循环轮询：取出完整帧并处理
  * @note   需在主循环中反复调用
  * @retval None
  */
void DmaTest_Poll(void)
{
  uint16_t len;
  uint16_t i;

  if (s_frame_head == s_frame_tail)     /* 无完整帧 */
  {
    return;
  }

  len = s_frame_len[s_frame_tail];
  s_frame_tail = (uint8_t)(((uint16_t)s_frame_tail + 1U) % DMATEST_FRAME_QUEUE);

  if (len > DMATEST_FRAME_MAX)
  {
    len = DMATEST_FRAME_MAX;
  }

  for (i = 0U; i < len; i++)
  {
    s_frame_buf[i] = s_ring[s_ring_tail];
    s_ring_tail = (uint16_t)((s_ring_tail + 1U) & DMATEST_RING_MASK);
  }

  DmaTest_HandleFrame(s_frame_buf, len);
}

/**
  * @brief  通过 USART1 DMA 发送数据（上次发送未完成时短暂等待）
  * @param  data 数据指针
  * @param  len  字节数
  * @retval 1 已启动发送，0 失败
  */
uint8_t DmaTest_Send(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  uint32_t start;

  if ((data == NULL) || (len == 0U) || (len > DMATEST_TX_BUF_SIZE))
  {
    return 0U;
  }

  start = HAL_GetTick();
  while (s_tx_busy != 0U)
  {
    if ((HAL_GetTick() - start) > 200U)   /* 等待超时，放弃本次发送 */
    {
      return 0U;
    }
  }

  for (i = 0U; i < len; i++)
  {
    s_tx_buf[i] = data[i];
  }

  s_tx_busy = 1U;
  if (HAL_UART_Transmit_DMA(&huart1, s_tx_buf, len) != HAL_OK)
  {
    s_tx_busy = 0U;
    return 0U;
  }

  return 1U;
}

/**
  * @brief  获取累计丢弃的字节数（调试用）
  * @retval 字节数
  */
uint32_t DmaTest_GetDropCount(void)
{
  return s_drop_bytes;
}

/* HAL callbacks -------------------------------------------------------------*/

/**
  * @brief  UART 接收事件回调：处理 IDLE / 半传输 / 传输完成事件
  * @param  huart UART 句柄
  * @param  Size  事件发生时已接收的字节数（此处以 DMA 计数器为准）
  * @retval None
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  uint16_t added;
  uint8_t  next;

  if (huart->Instance != USART1)
  {
    return;
  }

  (void)Size;   /* 统一使用 CNDTR 计算增量，兼容三种事件 */

  added = DmaTest_CommitDmaData();
  if (added > 0U)
  {
    s_frame_acc = (uint16_t)(s_frame_acc + added);
  }

  /* 只有空闲中断才代表一帧结束 */
  if (HAL_UARTEx_GetRxEventType(huart) != HAL_UART_RXEVENT_IDLE)
  {
    return;
  }

  if (s_frame_acc == 0U)
  {
    s_frame_broken = 0U;
    return;
  }

  /* 溢出或超过单帧上限：整帧丢弃并复位缓冲区，避免后续帧错位 */
  if ((s_frame_broken != 0U) || (s_frame_acc > DMATEST_FRAME_MAX))
  {
    s_drop_bytes += s_frame_acc;
    DmaTest_ResetBuffers();
    return;
  }

  next = (uint8_t)(((uint16_t)s_frame_head + 1U) % DMATEST_FRAME_QUEUE);
  if (next == s_frame_tail)             /* 队列满：丢弃该帧 */
  {
    s_drop_bytes += s_frame_acc;
    DmaTest_ResetBuffers();
    return;
  }

  s_frame_len[s_frame_head] = s_frame_acc;
  s_frame_head = next;
  s_frame_acc  = 0U;
  s_frame_broken = 0U;
}

/**
  * @brief  UART 发送完成回调
  * @param  huart UART 句柄
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    s_tx_busy = 0U;
  }
}

/**
  * @brief  UART 错误回调：清除错误后重新启动环形接收，防止接收永久停止
  * @param  huart UART 句柄
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    DmaTest_StartReceive();
  }
}
