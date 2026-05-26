#ifndef RACER_IMU_H
#define RACER_IMU_H

#include <stdint.h>

/* Initialize the IMU (ADXL345) */
void racer_imu_init(void);

/* Read and print IMU acceleration readings to serial */
void racer_imu_print_readings(void);

#endif