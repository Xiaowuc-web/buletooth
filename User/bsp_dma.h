#ifndef __BSP_DMA_H
#define __BSP_DMA_H

#include "stm32f10x.h"

void DMA_RX_Init(void);
/* TC5 置位返回 1，否则返回 0；不清标志。 */
uint8_t DMA_RX_Read(void);
/* 返回 DMA 下一次写入的下标。 */
uint16_t DMA_RX_GetPos(void);
/* 非阻塞：读到一字节返回 1，否则返回 0。 */
uint8_t DMA_RX_ReadByte(uint8_t *data);

#endif
