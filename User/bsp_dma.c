#include "bsp_dma.h"

#define DMA_RX_SIZE 512

/* DMA 写入缓冲区，主循环通过读位置逐字节取出。 */
static volatile uint8_t DMA_RxBuffer[DMA_RX_SIZE];
static uint16_t DMA_RxReadPos;

/**
 * @brief 初始化 USART1 的 DMA 接收：使用 DMA1 通道 5。
 * @details 每收到一个字节，DMA 就把 USART1->DR 的数据搬到接收缓冲区。
 *          缓冲区共 512 字节，写到末尾后自动回到开头。初始化时关闭
 *          USART1 的 RXNE 中断，并开启 USART1 的 DMA 接收请求。
 * @note 在 USART1 配置完成后调用。主循环应及时取走数据，
 *       避免 DMA 写满一圈而追上读位置。
 */
void DMA_RX_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    // 1. 开启 DMA1 时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // 2. 暂停接收请求并关闭 RXNE 中断，再复位 DMA 通道。
    USART_DMACmd(USART1, USART_DMAReq_Rx, DISABLE);
    USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
    DMA_DeInit(DMA1_Channel5);
    DMA_RxReadPos = 0;

    // 3. 配置 DMA 参数

    // 外设地址：USART1 数据寄存器
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;

    // 内存地址：接收数组首地址
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)DMA_RxBuffer;

    // 传输方向：外设到内存
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;

    // 传输数量
    DMA_InitStructure.DMA_BufferSize = DMA_RX_SIZE;

    // 外设地址不递增
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;

    // 内存地址递增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;

    // 外设数据宽度：8位
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;

    // 内存数据宽度：8位
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;

    // 循环模式：写到数组末尾后自动回到开头。
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;

    // 优先级：高
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;

    // 禁止内存到内存传输
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

    // 4. 先配置并启用 DMA 通道，再允许 USART1 发出接收请求。
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel5, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
}

/* 只检查 DMA 是否写满过一圈，不读取数据，也不清除 TC5。 */
uint8_t DMA_RX_Read(void)
{
    if (DMA_GetFlagStatus(DMA1_FLAG_TC5) == SET)
    {
        return 1;
    }
    return 0;
}

/* 返回 DMA 下一次写入缓冲区的位置，不读取数据。 */
uint16_t DMA_RX_GetPos(void)
{
    uint16_t remaining;

    remaining = DMA_GetCurrDataCounter(DMA1_Channel5);

    // 剩余量为 0 时，缓冲区刚写完一圈，下一次从下标 0 开始。
    if (remaining == 0)
    {
        return 0;
    }

    // 缓冲区长度减去剩余量，就是下一次写入的下标。
    return DMA_RX_SIZE - remaining;
}

/* 读到一个字节返回 1，没有新数据或 data 为空时返回 0。 */
uint8_t DMA_RX_ReadByte(uint8_t *data)
{
    uint16_t writePos;

    if (data == 0)
    {
        return 0;
    }

    /* 及时读取时，两者相等表示没有待取字节。 */
    writePos = DMA_RX_GetPos();
    if (DMA_RxReadPos == writePos)
    {
        return 0;
    }

    *data = DMA_RxBuffer[DMA_RxReadPos];
    DMA_RxReadPos++;

    /* 读到缓冲区末尾后，回到开头。 */
    if (DMA_RxReadPos >= DMA_RX_SIZE)
    {
        DMA_RxReadPos = 0;
    }
    return 1;
}
