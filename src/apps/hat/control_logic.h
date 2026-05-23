#ifndef CONTROL_LOGIC_H
#define CONTROL_LOGIC_H

#include <stdint.h>

void  imu_init(void);
int  read_imu(int16_t accel[3]);
void get_pwm(int *left, int *right);

#endif