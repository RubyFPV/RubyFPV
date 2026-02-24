#pragma once
#include "../base/base.h"
#include "../base/config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined (HW_PLATFORM_RADXA)
int wiringPiI2CSetup(const int devId);
int wiringPiI2CRead(int fd);
int wiringPiI2CReadReg8(int fd, int reg);
int wiringPiI2CReadReg16(int fd, int reg);
int wiringPiI2CWrite(int fd, int data);
int wiringPiI2CWriteReg8(int fd, int reg, int data);
int wiringPiI2CWriteReg16(int fd, int reg, int data);
int wiringPiI2CWriteBlockData(int fd, uint8_t reg, uint8_t length, uint8_t *values);
int wiringPiI2CWriteBlockDataIoctl(int fd,int addr, uint8_t reg, uint8_t length, uint8_t *values);
#endif

#ifdef __cplusplus
}
#endif