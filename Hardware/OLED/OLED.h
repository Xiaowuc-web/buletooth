#ifndef OLED_H
#define OLED_H

#include "stm32f10x.h"

/* SSD1306 128x64, software I2C: PB3=SCL, PB4=SDA. */
#define OLED_I2C_ADDRESS  0x3CU

/* Status: 0=not started, 1=init acknowledged, 2=bus low,
 * 3=no address ACK, 4=initialization NACK, 5=text transfer failed. */
extern volatile uint8_t OLED_Status;
extern volatile uint8_t OLED_DetectedAddress;
uint8_t OLED_Init(void);
void OLED_Clear(void);
/* 单行 ASCII 显示与串口内容区域更新，均直接写屏，无需额外刷新。 */
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowReceived(const char *str);
void OLED_ShowChinese(uint8_t x, uint8_t page, uint8_t index);
void OLED_ShowName(void);

#endif
