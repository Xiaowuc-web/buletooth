#include "mpu6050.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"

/**
 * @brief 初始化 MPU6050 使用的 STM32 硬件 I2C1，总线速率为 100 kHz。
 * @details 开启 APB2 的 GPIOB 时钟和 APB1 的 I2C1 时钟，并复位 I2C1。
 *          使用默认映射 PB6（SCL）、PB7（SDA），两脚均为复用开漏输出。
 *          采用标准模式、7 位寻址，默认使能 ACK；库函数 I2C_Init 会使能外设。
 * @note SCL 和 SDA 都需要上拉到 3.3 V；若模块已经带上拉，则不必重复加装。
 *       本函数仅初始化总线，不检测或唤醒 MPU6050。
 */
void MPU6050_I2C_Init(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef i2c;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    I2C_DeInit(I2C1);

    gpio.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &gpio);

    i2c.I2C_ClockSpeed = 100000U;
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0U;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &i2c);
}