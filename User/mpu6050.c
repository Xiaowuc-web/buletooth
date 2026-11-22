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

static void MPU6050_I2C_Start(void)
{
    /* 产生 START 或重复 START，等待 EV5。 */
    I2C_GenerateSTART(I2C1, ENABLE);
    while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
    }
}

void MPU6050_ReadReg(uint8_t reg, uint8_t *value)
{
    if (value == 0)
    {
        return;
    }

    /* 首次 START 前等待总线空闲；重复 START 前不等待 BUSY 清零。 */
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET)
    {
    }
    MPU6050_I2C_Start();

    /* 先用写方向发送从机地址，再发送调用者指定的寄存器地址。 */
    I2C_Send7bitAddress(I2C1, MPU6050_ADDR << 1, I2C_Direction_Transmitter);
    while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
    {
    }
    I2C_SendData(I2C1, reg);
    while (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
    {
    }

    /* 用重复 START 切换到读方向，不释放总线。 */
    MPU6050_I2C_Start();
    I2C_Send7bitAddress(I2C1, MPU6050_ADDR << 1, I2C_Direction_Receiver);
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_ADDR) == RESET)
    {
    }

   
}
