#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

void MPU6050_I2C_Init(void);
void MPU6050_ReadReg(uint8_t reg, uint8_t *value);

/* 返回 1：写入成功；返回 0：通信错误或等待超时。 */
uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t data);
uint8_t MPU6050_WakeUp(void);

#define MPU6050_ADDR 0x68

#define MPU6050_WHO_AM_I_REG 0x75
#define MPU6050_PWR_MGMT_1_REG 0x6B

#endif
