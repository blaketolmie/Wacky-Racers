#ifndef RACER_IMU_H
#define RACER_IMU_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize the IMU (ADXL345).  Returns 0 when the IMU starts correctly. */
int racer_imu_init(void);

/* Read the IMU and update the stored upright/upside-down state. */
bool racer_imu_update(void);

/* True when the IMU says the racer is upside down. */
bool racer_imu_is_upside_down(void);

/* Read and print IMU acceleration readings to serial, at a slow debug rate. */
bool racer_imu_print_readings(void);

#endif
