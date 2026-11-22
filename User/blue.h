#ifndef BLUE_H
#define BLUE_H

#include <stdint.h>

void LED_Init(void);
void USART1_Init(void);
void USART1_SendByte(uint8_t data);
void USART1_SendString(const char *str);
uint8_t USART1_ReceiveByte(void);
uint8_t USART1_TryReceiveByte(uint8_t *data);
void USART1_ReceiveString(char *buffer, uint8_t maxLen);

#endif
