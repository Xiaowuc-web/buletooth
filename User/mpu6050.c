#include "mpu6050.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"

/* I2C 等待的轮询次数上限，单位不是毫秒。 */
#define MPU6050_I2C_POLL_LIMIT 100000
#define MPU6050_I2C_ERROR_FLAGS \
    (I2C_FLAG_AF | I2C_FLAG_BERR | I2C_FLAG_ARLO)

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

/* 等待指定的 I2C 标志，遇到通信错误或超时就返回失败。 */
static uint8_t MPU6050_WaitFlag(uint32_t flag)
{
    uint32_t timeout = MPU6050_I2C_POLL_LIMIT;

    while (timeout != 0)
    {
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET ||
            I2C_GetFlagStatus(I2C1, I2C_FLAG_BERR) == SET ||
            I2C_GetFlagStatus(I2C1, I2C_FLAG_ARLO) == SET)
        {
            return 0;
        }
        if (I2C_GetFlagStatus(I2C1, flag) == SET)
        {
            return 1;
        }
        --timeout;
    }
    return 0;
}

static uint8_t MPU6050_I2C_Start(void)
{
    /* 产生 START 或重复 START，等 EV5 的 SB 标志。 */
    I2C_GenerateSTART(I2C1, ENABLE);
    return MPU6050_WaitFlag(I2C_FLAG_SB);
}

/* 失败时尝试发 STOP；STOP 完成后才恢复 ACK。 */
static uint8_t MPU6050_EndRead(uint8_t success)
{
    uint32_t timeout = MPU6050_I2C_POLL_LIMIT;

    if (!success && I2C_GetFlagStatus(I2C1, I2C_FLAG_MSL) == SET &&
        I2C_GetFlagStatus(I2C1, I2C_FLAG_ARLO) == RESET &&
        (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0)
    {
        I2C_GenerateSTOP(I2C1, ENABLE);
    }
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0 && timeout != 0)
    {
        --timeout;
    }
    if ((I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) == 0)
    {
        I2C_AcknowledgeConfig(I2C1, ENABLE);
    }
    if (timeout == 0 || I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET ||
        I2C_GetFlagStatus(I2C1, I2C_FLAG_BERR) == SET ||
        I2C_GetFlagStatus(I2C1, I2C_FLAG_ARLO) == SET)
    {
        success = 0;
    }
    I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);
    return success;
}

/* 共用读时序：调用者只读取 1 字节或一组 14 字节。 */
static uint8_t MPU6050_ReadBytes(uint8_t reg, uint8_t *value, uint8_t count)
{
    uint32_t timeout = MPU6050_I2C_POLL_LIMIT;
    uint32_t irqState;
    uint8_t index = 0;

    /* 首次 START 前等待总线空闲。 */
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET ||
           (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) != 0)
    {
        --timeout;
        if (timeout == 0)
        {
            return 0;
        }
    }
    I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
    if (!MPU6050_I2C_Start())
    {
        return MPU6050_EndRead(0);
    }

    /* 先写寄存器地址，再用重复 START 切换为读。 */
    I2C_Send7bitAddress(I2C1, MPU6050_ADDR << 1, I2C_Direction_Transmitter);
    if (!MPU6050_WaitFlag(I2C_FLAG_ADDR))
    {
        return MPU6050_EndRead(0);
    }
    (void)I2C_GetLastEvent(I2C1); /* 读 SR1、SR2，清除 ADDR。 */
    I2C_SendData(I2C1, reg);
    if (!MPU6050_WaitFlag(I2C_FLAG_BTF) || !MPU6050_I2C_Start())
    {
        return MPU6050_EndRead(0);
    }
    I2C_Send7bitAddress(I2C1, MPU6050_ADDR << 1, I2C_Direction_Receiver);
    if (!MPU6050_WaitFlag(I2C_FLAG_ADDR))
    {
        return MPU6050_EndRead(0);
    }

    if (count == 1)
    {
        /* 单字节：先关 ACK，再清 ADDR，紧接着请求 STOP。 */
        irqState = __get_PRIMASK();
        __disable_irq();
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        (void)I2C_GetLastEvent(I2C1);
        I2C_GenerateSTOP(I2C1, ENABLE);
        __set_PRIMASK(irqState);
    }
    else
    {
        (void)I2C_GetLastEvent(I2C1);
        while (count - index > 3)
        {
            if (!MPU6050_WaitFlag(I2C_FLAG_BTF))
            {
                return MPU6050_EndRead(0);
            }
            value[index] = I2C_ReceiveData(I2C1);
            ++index;
        }

        /* 剩 3 字节时等 BTF，再关 ACK；最后一个字节由主机发 NACK。 */
        if (!MPU6050_WaitFlag(I2C_FLAG_BTF))
        {
            return MPU6050_EndRead(0);
        }
        /* 读倒数第三字节到发 STOP 必须连着执行，不让串口中断打断。 */
        irqState = __get_PRIMASK();
        __disable_irq();
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        value[index] = I2C_ReceiveData(I2C1);
        I2C_GenerateSTOP(I2C1, ENABLE);
        value[index + 1] = I2C_ReceiveData(I2C1);
        __set_PRIMASK(irqState);
    }

    if (!MPU6050_WaitFlag(I2C_FLAG_RXNE))
    {
        return MPU6050_EndRead(0);
    }
    value[count - 1] = I2C_ReceiveData(I2C1);
    return MPU6050_EndRead(1);
}

uint8_t MPU6050_ReadReg(uint8_t reg, uint8_t *value)
{
    uint8_t data;

    if (value == 0 || !MPU6050_ReadBytes(reg, &data, 1))
    {
        return 0;
    }
    *value = data;
    return 1;
}

uint8_t MPU6050_ReadData(int16_t data[7])
{
    uint8_t bytes[14];
    uint8_t i;
    int32_t value;

    if (data == 0 || !MPU6050_ReadBytes(MPU6050_ACCEL_XOUT_H_REG, bytes, 14))
    {
        return 0;
    }
    for (i = 0; i < 7; i++)
    {
        /* 高字节占高 8 位；最高位为 1 时按 16 位补码转成负数。 */
        value = (bytes[i * 2] << 8) | bytes[i * 2 + 1];
        if (value >= 32768)
        {
            value -= 65536;
        }
        data[i] = value;
    }
    return 1;
}

uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t data)
{
    uint32_t timeout = MPU6050_I2C_POLL_LIMIT;

    /* 等待总线空闲，并确认没有尚未完成的 START 或 STOP。 */
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) == SET ||
           (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) != 0)
    {
        --timeout;
        if (timeout == 0)
        {
            return 0;
        }
    }
    I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);

    /* 产生 START，等待 EV5。 */
    I2C_GenerateSTART(I2C1, ENABLE);
    while (timeout != 0 &&
           I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) != SUCCESS)
    {
        --timeout;
    }

    /* START 成功后，以写方向发送设备地址，等待 EV6。 */
    if (timeout != 0)
    {
        I2C_Send7bitAddress(I2C1, MPU6050_ADDR << 1, I2C_Direction_Transmitter);
        while (timeout != 0 && I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET &&
               I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) != SUCCESS)
        {
            --timeout;
        }
    }

    /* 前一步未超时、未收到 NACK，才发送寄存器地址。 */
    if (timeout != 0 && I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET)
    {
        I2C_SendData(I2C1, reg);
        while (timeout != 0 && I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET &&
               I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
        {
            --timeout;
        }
    }

    /* 发送数据，再等一次 EV8_2，确保最后一个字节发送完毕。 */
    if (timeout != 0 && I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET)
    {
        I2C_SendData(I2C1, data);
        while (timeout != 0 && I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == RESET &&
               I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED) != SUCCESS)
        {
            --timeout;
        }
    }

    /* 成功或失败都尝试收尾，但只有仍为主机时才能发 STOP。 */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_MSL) == RESET ||
        I2C_GetFlagStatus(I2C1, I2C_FLAG_ARLO) == SET ||
        (I2C1->CR1 & (I2C_CR1_START | I2C_CR1_STOP)) != 0)
    {
        I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);
        return 0;
    }
    I2C_GenerateSTOP(I2C1, ENABLE);

    if (timeout == 0 || I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) == SET)
    {
        I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);
        return 0;
    }

    /* 只有传输正常并且 STOP 已完成，才返回成功。 */
    timeout = MPU6050_I2C_POLL_LIMIT;
    while ((I2C1->CR1 & I2C_CR1_STOP) != 0)
    {
        --timeout;
        if (timeout == 0)
        {
            return 0;
        }
    }
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BERR) == SET ||
        I2C_GetFlagStatus(I2C1, I2C_FLAG_ARLO) == SET)
    {
        I2C_ClearFlag(I2C1, MPU6050_I2C_ERROR_FLAGS);
        return 0;
    }
    return 1;
}

uint8_t MPU6050_WakeUp(void)
{
    /* PWR_MGMT_1 写 0x00：清除 Bit 6（SLEEP）。 */
    return MPU6050_WriteReg(MPU6050_PWR_MGMT_1_REG, 0x00U);
}
