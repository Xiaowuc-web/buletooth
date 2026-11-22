#include "gpio.h"
#include "blue.h"
#include "OLED.h"
#include "mpu6050.h"
#include <stdio.h>

/* value 是实际值的 100 倍；末尾空格擦除上一行多出的字符。 */
static void OLED_ShowValue(uint8_t page, const char *name, int value, const char *unit)
{
    char text[21];
    char sign = '+';

    if (value < 0)
    {
        sign = '-';
        value = -value;
    }
    snprintf(text, sizeof(text), "%s %c%d.%02d %s   ", name, sign,
             value / 100, value % 100, unit);
    OLED_ShowString(0, page, text);
}

int main(void)
{
    char buffer[32];
    uint8_t length = 0;
    uint8_t byte;
    uint8_t oledReady;
    uint8_t whoAmI;
    uint8_t sensorReady = 0;
    uint8_t readError = 0;
    uint8_t page;
    int16_t data[7];
    const char *status;

    GPIO_PC13_Init();
    LED_Init();
    USART1_Init();
    MPU6050_I2C_Init();
    oledReady = OLED_Init();

    if (!MPU6050_ReadReg(MPU6050_WHO_AM_I_REG, &whoAmI))
    {
        status = "MPU6050 I2C ERR  ";
    }
    else if (whoAmI != MPU6050_WHO_AM_I_ID)
    {
        status = "MPU6050 BAD ID   ";
    }
    else if (!MPU6050_WakeUp() ||
             !MPU6050_WriteReg(MPU6050_ACCEL_CONFIG_REG, 0x00) ||
             !MPU6050_WriteReg(MPU6050_GYRO_CONFIG_REG, 0x00))
    {
        status = "MPU6050 INIT ERR ";
    }
    else
    {
        status = "MPU6050 ID:0x68  ";
        sensorReady = 1;
    }
    if (oledReady)
    {
        OLED_ShowString(0, 0, status);
    }

    /* SysTick 用 HCLK/8，约 300 ms 到期一次，不开启定时中断。 */
    SysTick->LOAD = SystemCoreClock / 80 * 3 - 1;
    SysTick->VAL = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk;

    while (1)
    {
        /* 有字符才处理，保留按行回显，不覆盖传感器画面。 */
        if (USART1_TryReceiveByte(&byte))
        {
            if (byte != '\r' && byte != '\n')
            {
                buffer[length] = byte;
                ++length;
            }
            if (byte == '\r' || byte == '\n' || length == sizeof(buffer) - 1)
            {
                buffer[length] = '\0';
                USART1_SendString(buffer);
                length = 0;
            }
        }

        if (sensorReady && oledReady && OLED_Status == 1 &&
            (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) != 0)
        {
            if (MPU6050_ReadData(data))
            {
                if (readError)
                {
                    OLED_ShowString(0, 0, status);
                    readError = 0;
                }
                OLED_ShowValue(1, "AX", data[0] * 100 / 16384, "g");
                OLED_ShowValue(2, "AY", data[1] * 100 / 16384, "g");
                OLED_ShowValue(3, "AZ", data[2] * 100 / 16384, "g");
                OLED_ShowValue(4, "GX", data[4] * 100 / 131, "d/s");
                OLED_ShowValue(5, "GY", data[5] * 100 / 131, "d/s");
                OLED_ShowValue(6, "GZ", data[6] * 100 / 131, "d/s");
                OLED_ShowValue(7, "TEMP", data[3] * 100 / 340 + 3653, "C");
            }
            else if (!readError)
            {
                /* 通信失败时只清除一次旧数据，下一次到期再尝试读取。 */
                OLED_ShowString(0, 0, "MPU6050 READ ERR ");
                for (page = 1; page < 8 && OLED_Status == 1; page++)
                {
                    OLED_ShowString(0, page, "                     ");
                }
                readError = 1;
            }
        }
    }
}
