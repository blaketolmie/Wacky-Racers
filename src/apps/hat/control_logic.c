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
        printf("Waiting for accelerometer to be ready... %d\n", ++count);
        return 0;
    }
    if (adxl345_accel_read(adxl345, accel)) {
        return 1;
    }
    printf("ERROR: failed to read acceleration\n");
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

    float x = accel[0] / 2.6f;
    float y = accel[1] / 2.6f;

    // Clamp to ±100
    if (x > 100.0f)  x = 100.0f;
    if (x < -100.0f) x = -100.0f;
    if (y > 100.0f)  y = 100.0f;
    if (y < -100.0f) y = -100.0f;

    // Deadzone
    if (fabsf(x) < 3.85f) x = 0.0f;
    if (fabsf(y) < 3.85f) y = 0.0f;

    *left  = (int)(y + x);
    *right = (int)(y - x);

    if (*left > 100)   *left = 100;
    if (*left < -100)  *left = -100;
    if (*right > 100)  *right = 100;
    if (*right < -100) *right = -100;
}