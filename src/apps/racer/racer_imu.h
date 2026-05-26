#ifndef RACER_IMU_H
#define RACER_IMU_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize the IMU (ADXL345).  Returns 0 when the IMU starts correctly. */
int racer_imu_init(void);

/* Read and print IMU acceleration readings to serial, at a slow debug rate. */
bool racer_imu_print_readings(void);

#endif
