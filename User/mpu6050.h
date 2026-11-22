#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

void MPU6050_I2C_Init(void);
void MPU6050_ReadReg(uint8_t reg, uint8_t *value);

#define MPU6050_ADDR 0x68

#define MPU6050_WHO_AM_I_REG 0x75

#endif
