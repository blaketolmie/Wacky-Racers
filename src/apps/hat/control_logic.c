/* File:   adxl345_test1.c
   Author: M. P. Hayes, UCECE
   Date:   3 December 2022
   Descr:  Read from an ADXL345 accelerometer and write its output to the USB serial.
*/
#include "pio.h"
#include "delay.h"
#include "target.h"
#include "pacer.h"
#include "usb_serial.h"
#include "adxl345.h"
#include "panic.h"
#include "mcu.h"
#include <stdlib.h>
#include <math.h>

#include "../wacky_racers_tuning.h"


/*
 * NOTE: you must define ADXL345_ADDRESS in target.h for this to compile.
 */
#ifndef ADXL345_ADDRESS
#error ADXL345_ADDRESS must be defined
#endif

static twi_cfg_t adxl345_twi_cfg =
{
    .channel = TWI_CHANNEL_0,
    .period = TWI_PERIOD_DIVISOR (100000), // 100 kHz
    .slave_addr = 0
};


// Define Variables
twi_t adxl345_twi;
adxl345_t *adxl345;
int count = 0;


void imu_init(void)
{
    mcu_jtag_disable ();
    pio_config_set (IMU_OFF, PIO_OUTPUT_HIGH); // power on the IMU
    delay_ms (100); // wait for power to stabilise

    // Initialise the TWI (I2C) bus for the ADXL345
    adxl345_twi = twi_init (&adxl345_twi_cfg);

    if (! adxl345_twi)
        panic (LED_ERROR_PIO, 1);

    // Initialise the ADXL345
    adxl345 = adxl345_init (adxl345_twi, ADXL345_ADDRESS);

    if (! adxl345)
        panic (LED_ERROR_PIO, 2);
}

int read_imu(int16_t accel[3])
{
    if (!adxl345_is_ready(adxl345)) {
        /* printf("Waiting for accelerometer to be ready... %d\n", ++count); */
        return 0;
    }
    if (adxl345_accel_read(adxl345, accel)) {
        return 1;
    }
    /* printf("ERROR: failed to read acceleration\n"); */
    return 0;
}

void get_pwm(int *left, int *right)
{
    int16_t accel[3];
    if (!read_imu(accel)) {
        *left = 0;
        *right = 0;
        return;
    }

    float x = accel[0] / HAT_CONTROL_X_SCALE;
    float y = accel[1] / HAT_CONTROL_Y_SCALE;

    // Clamp to +/-100
    if (x > HAT_CONTROL_PWM_LIMIT)  x = HAT_CONTROL_PWM_LIMIT;
    if (x < -HAT_CONTROL_PWM_LIMIT) x = -HAT_CONTROL_PWM_LIMIT;
    if (y > HAT_CONTROL_PWM_LIMIT)  y = HAT_CONTROL_PWM_LIMIT;
    if (y < -HAT_CONTROL_PWM_LIMIT) y = -HAT_CONTROL_PWM_LIMIT;

    // Deadzone
    if (fabsf(x) < HAT_CONTROL_DEADZONE) x = 0.0f;
    if (fabsf(y) < HAT_CONTROL_DEADZONE) y = 0.0f;

    *left  = (int)(y + x);
    *right = (int)(y - x);

    if (*left > HAT_CONTROL_PWM_LIMIT)   *left = HAT_CONTROL_PWM_LIMIT;
    if (*left < -HAT_CONTROL_PWM_LIMIT)  *left = -HAT_CONTROL_PWM_LIMIT;
    if (*right > HAT_CONTROL_PWM_LIMIT)  *right = HAT_CONTROL_PWM_LIMIT;
    if (*right < -HAT_CONTROL_PWM_LIMIT) *right = -HAT_CONTROL_PWM_LIMIT;
}
