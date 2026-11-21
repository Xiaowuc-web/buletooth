#include "gpio.h"
#include "blue.h"
#include "OLED.h"

/**
 * @brief  初始化外设，循环接收串口字符串、更新 OLED 并回显。
 * @param  无。
 * @retval 不返回。
 * @note   接收、显示和回显共用 buffer，不重复读取串口数据。
 *         沿用原接收逻辑：单条文本建议不超过 30 字节，以单个 '\n' 结束，
 *         留出结束符接收空间；每次等待回显后再发送下一条。
 *         OLED 初始化失败时跳过显示，仍保留串口接收和回显功能。
 */
int main(void)
{
    char buffer[32];
    uint8_t maxLen = sizeof(buffer);
    uint8_t oledReady;

    GPIO_PC13_Init();
    LED_Init();
    USART1_Init();
    oledReady = OLED_Init();

    if (oledReady)
    {
        OLED_ShowString(6, 0, "UART RECEIVE");
    }

    while (1)
    {
        USART1_ReceiveString(buffer, maxLen);
        if (oledReady)
        {
            OLED_ShowReceived(buffer);
        }
        USART1_SendString(buffer);
    }
}
