#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

void MPU6050_I2C_Init(void);

/* 返回 1：成功；返回 0：通信错误或等待超时。 */
uint8_t MPU6050_ReadReg(uint8_t reg, uint8_t *value);
/* data 依次存放 AX、AY、AZ、温度、GX、GY、GZ 的原始值。 */
uint8_t MPU6050_ReadData(int16_t data[7]);
uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t data);
uint8_t MPU6050_WakeUp(void);

#define MPU6050_ADDR 0x68

#define MPU6050_GYRO_CONFIG_REG 0x1B
#define MPU6050_ACCEL_CONFIG_REG 0x1C
#define MPU6050_ACCEL_XOUT_H_REG 0x3B
#define MPU6050_WHO_AM_I_REG 0x75
#define MPU6050_WHO_AM_I_ID 0x68
#define MPU6050_PWR_MGMT_1_REG 0x6B

#endif
