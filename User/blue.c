#include "blue.h"
#include "stm32f10x.h"
#include "bsp_dma.h"

/**
 * @brief  初始化连接至 PB12 的 LED 控制引脚。
 *         开启 GPIOB 时钟，将 PB12 配置为 50 MHz 通用推挽输出。
 *         在切换为输出模式前预置低电平，避免初始化时出现高电平脉冲。
 * @param  无。
 * @retval 无。
 * @note   初始化完成后 PB12 输出低电平；仅在 LED 为高电平点亮时表示关闭。
 *         本函数只配置 PB12，不修改 GPIOB 其他引脚的工作模式。
 */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

/**
 * @brief  初始化 USART1，使用默认引脚 PA9 发送、PA10 接收。
 *         开启 GPIOA 和 USART1 时钟，将 PA9 配置为 50 MHz 复用推挽输出，
 *         将 PA10 配置为浮空输入；串口采用 9600 波特率、8 位数据位、
 *         1 位停止位、无奇偶校验、无硬件流控，并使能发送和接收功能。
 * @param  无。
 * @retval 无。
 * @note   应在系统时钟配置完成后、首次收发数据前调用。
 *         要求 USART1 未重映射，且系统时钟相关变量与实际时钟一致。
 *         使用 DMA 循环接收，发送使用轮询方式。
 *         不应在正在收发数据时调用，以免中断当前传输。
 */
void USART1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
        ENABLE);

    USART_Cmd(USART1, DISABLE);

    GPIO_StructInit(&GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_StructInit(&USART_InitStructure);
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode =
        USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);
    DMA_RX_Init();
    USART_Cmd(USART1, ENABLE);
}

/**
 * @brief  通过 USART1 以阻塞轮询方式发送一个字节。
 *         等待发送数据寄存器空标志 TXE 置位后，将待发送字节写入数据寄存器，
 *         由串口硬件按照初始化时配置的波特率和帧格式完成发送。
 * @param  data：待发送的一个字节，取值范围为 0～255（0x00～0xFF）。
 *         参数为原始字节，不会自动转换为十进制字符串或添加换行符。
 * @retval 无。
 * @note   调用前必须完成 USART1_Init() 初始化并使能发送功能。
 *         本函数没有超时机制，等待 TXE 期间会持续占用处理器。
 *         函数返回仅表示数据已写入发送数据寄存器，不代表线路发送完成；
 *         如需确认最后一帧已发送完毕，应继续等待 TC 标志置位。
 *         不应与中断、DMA 或其他执行上下文同时操作 USART1 发送寄存器。
 */
void USART1_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
    {
    }

    USART_SendData(USART1, data);
}

/**
 * @brief  通过 USART1 以阻塞轮询方式发送字符串。
 *         从下标 0 开始逐字节发送，遇到字符串结束符 '\0' 时停止，
 *         不发送结束符，也不会自动添加回车或换行。
 * @param  str：待发送字符串的指针，必须指向以 '\0' 结尾的有效字符串。
 *         传入空指针或空字符串时，不发送任何数据。
 * @retval 无。
 * @note   调用前必须完成 USART1_Init() 初始化。
 *         本函数通过 USART1_SendByte() 发送数据，没有超时机制。
 *         函数返回时，最后一个字节可能仍在传输；如需确认线路发送完成，
 *         应继续等待 USART1 的 TC 标志置位。
 *         不适用于包含中间零字节的二进制数据，也不应被多个执行上下文同时调用。
 */
void USART1_SendString(const char *str)
{
    uint32_t i = 0;

    if (str == 0)
    {
        return;
    }

    while (str[i] != '\0')
    {
        USART1_SendByte(str[i]);
        i++;
    }
}

/**
 * @brief  等待并读取 USART1 的一个 DMA 接收字节。
 * @param  无。
 * @retval 接收到的原始字节，取值范围为 0～255（0x00～0xFF）。
 *         返回值 0 也是有效数据，不表示接收失败。
 * @note   调用前必须完成 USART1_Init() 初始化。
 *         DMA 缓冲区为空时，会等待直到有字节到达。
 */
uint8_t USART1_ReceiveByte(void)
{
    uint8_t data;

    while (!USART1_TryReceiveByte(&data))
    {
    }
    return data;
}

/**
 * @brief  通过 USART1 以阻塞轮询方式接收字符串。
 *         每次接收一个字节，遇到回车 '\r'、换行 '\n' 或缓冲区已满时停止。
 *         回车和换行不存入缓冲区，接收结果始终以 '\0' 结尾。
 * @param  buffer：接收缓冲区指针，必须指向至少 maxLen 字节的可写空间。
 *         传入空指针时直接返回，不接收数据。
 * @param  maxLen：缓冲区总容量，包含字符串结束符的位置，取值范围为 0～255。
 *         最多保存 maxLen - 1 个数据字节；为 0 时直接返回，不访问缓冲区；
 *         为 1 时仅写入 '\0'，不接收数据。
 * @retval 无。
 * @note   调用前必须完成 USART1_Init() 初始化。本函数无超时机制，
 *         缓冲区未满且未收到结束符时，会一直等待后续数据。
 *         缓冲区满时不丢弃剩余数据，后续调用将继续读取尚未接收的数据。
 *         仅消费遇到的一个结束符；若发送端使用 '\r\n'，剩余的 '\n'
 *         可能使下一次调用返回空字符串。建议发送端仅使用一种结束符。
 *         不适用于包含零字节的二进制数据。
 */
void USART1_ReceiveString(char *buffer, uint8_t maxLen)
{
    uint8_t index = 0;
    uint8_t data;

    if (buffer == 0 || maxLen == 0)
    {
        return;
    }

    buffer[0] = '\0';

    while (index < maxLen - 1)
    {
        data = USART1_ReceiveByte();
        if (data == '\r' || data == '\n')
        {
            break;
        }

        buffer[index] = (char)data;
        index++;
    }

    buffer[index] = '\0';
}

/* 缓冲区空时立即返回，不等蓝牙发送。 */
uint8_t USART1_TryReceiveByte(uint8_t *data)
{
    return DMA_RX_ReadByte(data);
}
