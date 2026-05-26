#include <stdio.h>
#include <stdbool.h>

#include "racer_imu.h"
#include "adxl345.h"
#include "twi.h"
#include "mcu.h"
#include "pio.h"
#include "delay.h"
#include "target.h"
#include "../wacky_racers_tuning.h"

#ifndef ADXL345_ADDRESS
#error ADXL345_ADDRESS must be defined
#endif

#define RACER_IMU_PRINT_TICKS 100
#define RACER_IMU_UPSIDE_DOWN_Z -125
#define RACER_IMU_UPRIGHT_Z 125
#define ADXL345_OTHER_ADDRESS \
    ((ADXL345_ADDRESS == 0x1D) ? 0x53 : 0x1D)

static twi_cfg_t adxl345_twi_cfg =
{
    .channel = TWI_CHANNEL_0,
    .period = TWI_PERIOD_DIVISOR (100000), // 100 kHz
    .slave_addr = 0
};


/* Keep the TWI and ADXL345 handles private to this module. */
static twi_t adxl345_twi;
static adxl345_t *adxl345;
static uint16_t print_ticks;
static int wait_count;
static int16_t last_accel[3];
static bool have_accel_reading;
static bool upside_down;

static adxl345_t *racer_imu_init_address(twi_slave_addr_t address)
{
    return adxl345_init(adxl345_twi, address);
}

int racer_imu_init(void)
{
    /*
       TWI1 shares pins with JTAG on the SAM4S.  The current racer IMU uses
       TWI0, but disabling JTAG here keeps this code safe if the hardware is
       moved to TWI1 later.
    */
    mcu_jtag_disable ();

    pio_config_set (IMU_ENABLE_PIO, PIO_OUTPUT_HIGH); // power on the IMU
    delay_ms (100); // wait for power to stabilise

    // Initialise the TWI (I2C) bus for the ADXL345
    adxl345_twi = twi_init (&adxl345_twi_cfg);

    if (! adxl345_twi)
        return 1;

    /*
       Initialise the ADXL345.  Most boards use 0x1D, but the chip can also
       be strapped to 0x53 with its ALT ADDRESS pin, so try both.
    */
    adxl345 = racer_imu_init_address(ADXL345_ADDRESS);
    if (! adxl345)
    {
        adxl345 = racer_imu_init_address(ADXL345_OTHER_ADDRESS);
        if (adxl345)
        {
            /*
            printf("Racer IMU using alternate address 0x%02x\r\n",
                   ADXL345_OTHER_ADDRESS);
            */
            fflush(stdout);
        }
    }

    if (! adxl345)
        return 2;

    print_ticks = 0;
    wait_count = 0;
    have_accel_reading = false;
    upside_down = false;

    return 0;
}

static bool racer_imu_read_raw(int16_t accel[3])
{
    if (! adxl345)
        return false;

    if (! adxl345_is_ready(adxl345))
        return false;

    if (adxl345_accel_read(adxl345, accel)) {
        return true;
    }

    /* printf("ERROR: failed to read racer IMU acceleration\r\n"); */
    fflush(stdout);
    return false;
}

bool racer_imu_update(void)
{
    int16_t accel[3];

    if (! racer_imu_read_raw(accel))
        return false;

    last_accel[0] = accel[0];
    last_accel[1] = accel[1];
    last_accel[2] = accel[2];
    have_accel_reading = true;

    /*
       ADXL345 readings are about 250 counts per g in this mode.  The
       thresholds below are about +/-0.5 g, which gives enough hysteresis that
       a bump should not rapidly swap the controls back and forth.
       When the racer is upside down, left and right controls are swapped in
       racer.c so steering still feels correct after a rollover.
    */
    if (! upside_down && (accel[2] <= RACER_IMU_UPSIDE_DOWN_Z))
    {
        upside_down = true;
        /* printf("Racer upside down: controls swapped\r\n"); */
        fflush(stdout);
    }
    else if (upside_down && (accel[2] >= RACER_IMU_UPRIGHT_Z))
    {
        upside_down = false;
        /* printf("Racer upright: controls normal\r\n"); */
        fflush(stdout);
    }

    return true;
}

bool racer_imu_is_upside_down(void)
{
    return upside_down;
}

bool racer_imu_print_readings(void)
{
    /*
       main() calls this every 10 ms.  Only print once every 100 calls so the
       serial output does not slow down radio receiving and motor control.
    */
    print_ticks++;
    if (print_ticks < RACER_IMU_PRINT_TICKS)
        return false;
    print_ticks = 0;

    if (have_accel_reading) {
        /*
        printf("Racer IMU: X=%d, Y=%d, Z=%d, %s\r\n",
               last_accel[0], last_accel[1], last_accel[2],
               upside_down ? "upside down" : "upright");
        */
        fflush(stdout);
        return true;
    }

    /* printf("Waiting for racer IMU to be ready... %d\r\n", ++wait_count); */
    fflush(stdout);

    return false;
}
