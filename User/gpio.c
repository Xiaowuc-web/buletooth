#include "gpio.h"
#include "stm32f10x.h"

/**
 * @brief  初始化 PC13，并将其输出设置为低电平。
 *         开启 GPIOC 外设时钟，将 PC13 配置为通用推挽输出模式，
 *         GPIO 输出速度配置为 50 MHz，随后设置 PC13 输出低电平。
 * @param  无。
 * @retval 无。
 * @note   本函数封装原 main() 中的 GPIO 配置，保持原有配置及执行顺序。
 *         仅配置 PC13，不改变 GPIOC 其他引脚的工作模式。
 *         低电平是否点亮 LED 取决于实际硬件连接。
 *         GPIO 输出速度是输出驱动配置，不表示引脚以该频率翻转；
 *         PC13 的实际负载和输出使用限制应以具体芯片数据手册为准。
 */
void GPIO_PC13_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOC, GPIO_Pin_13);
}
