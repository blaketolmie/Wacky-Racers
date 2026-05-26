#include <stdio.h>
#include "usb_serial.h"
#include "adxl345.h"
#include "twi.h"
#include "mcu.h"
#include "pio.h"
#include "delay.h"
#include "panic.h"



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


void racer_imu_init(void)
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

void racer_imu_print_readings(void)
{
    int16_t accel[3];
    
    if (read_imu(accel)) {
        printf("IMU readings: X=%d, Y=%d, Z=%d\r\n", accel[0], accel[1], accel[2]);
        fflush(stdout);
    }
}