// /* File:   adxl345_test1.c
//    Author: M. P. Hayes, UCECE
//    Date:   3 December 2022
//    Descr:  Read from an ADXL345 accelerometer and write its output to the USB serial.
// */
// #include "pio.h"
// #include "delay.h"
// #include "target.h"
// #include "pacer.h"
// #include "usb_serial.h"
// #include "adxl345.h"
// #include "panic.h"
// #include "mcu.h"
// #include <stdlib.h>
// #include <math.h>


// /*
//  * NOTE: you must define ADXL345_ADDRESS in target.h for this to compile.
//  */
// #ifndef ADXL345_ADDRESS
// #error ADXL345_ADDRESS must be defined
// #endif

// #define PACER_RATE 20
// #define ACCEL_POLL_RATE 10

// static twi_cfg_t adxl345_twi_cfg =
// {
//     .channel = TWI_CHANNEL_0,
//     .period = TWI_PERIOD_DIVISOR (100000), // 100 kHz
//     .slave_addr = 0
// };


// int
// main (void)
// {
//     mcu_jtag_disable ();

//     // Define Variables
//     twi_t adxl345_twi;
//     adxl345_t *adxl345;
//     int ticks = 0;
//     int count = 0;
//     float x = 0;
//     float y = 0;
//     float steering = 0; // x
//     float throttle = 0; // y
//     float left_motor = 0;
//     float right_motor = 0;

//     // Redirect stdio to USB serial
//     usb_serial_stdio_init ();

//     pio_config_set (LED_ERROR_PIO, PIO_OUTPUT_LOW);
//     pio_output_set (LED_ERROR_PIO, ! LED_ACTIVE);
//     pio_config_set (LED_STATUS_PIO, PIO_OUTPUT_LOW);
//     pio_output_set (LED_STATUS_PIO, ! LED_ACTIVE);
//     pio_config_set (LED_RED_PIO, PIO_OUTPUT_LOW);
//     pio_output_set (LED_RED_PIO, ! LED_ACTIVE);
//     pio_config_set (LED_GREEN_PIO, PIO_OUTPUT_LOW);
//     pio_output_set (LED_GREEN_PIO, ! LED_ACTIVE);

//     pio_config_set (IMU_OFF, PIO_OUTPUT_HIGH);  // power on the IMU
//     delay_ms (100);                             // wait for power to stabilise


//     // Initialise the TWI (I2C) bus for the ADXL345
//     adxl345_twi = twi_init (&adxl345_twi_cfg);

//     if (! adxl345_twi)
//         panic (LED_RED_PIO, 1);

//     // Initialise the ADXL345
//     adxl345 = adxl345_init (adxl345_twi, ADXL345_ADDRESS);

//     if (! adxl345)
//         panic (LED_GREEN_PIO, 2);
//     pacer_init (PACER_RATE);

//     while (1)
//     {
//         /* Wait until next clock tick.  */
//         pacer_wait ();

//         ticks++;
//         if (ticks < PACER_RATE / ACCEL_POLL_RATE)
//             continue;
//         ticks = 0;

//         pio_output_toggle (LED_STATUS_PIO);

//         /* Read in the accelerometer data.  */
//         if (! adxl345_is_ready (adxl345))
//         {
//             count++;
//             printf ("Waiting for accelerometer to be ready... %d\n", count);
//         }
//         else
//         {
//             int16_t accel[3];
//             if (adxl345_accel_read (adxl345, accel))
//             {
//                 // printf ("x: %5d  y: %5d  z: %5d\n", accel[0], accel[1], accel[2]);
//                 x = accel[0];
//                 y = accel[1];
//             }
//             else
//             {
//                 printf ("ERROR: failed to read acceleration\n");
//             }
//         }

//         // Find throttle (-100 to 100)
//         if (y > 10 || y < -10)
//         {
//             throttle = y / SCALE_NUM_Y;
//         }
//         else
//         {
//             throttle = 0;
//         }
//         if (y > 260)
//         {
//             throttle = 100;
//         }
//         if (y < -260)
//         {
//             throttle = -100;
//         }

//         // Find Steering (-100 to 100)
//         if (x > 10 || x < -10)
//         {
//             steering = x / SCALE_NUM_X;
//         }
//         else
//         {
//             steering = 0;
//         }
//         if (x > 260)
//         {
//             steering = 100;
//         }
//         if (x < -260)
//         {
//             steering = -100;
//         }

//         /* steer, throttle logic */
//         // if (steering == 0 && throttle == 0)
//         // {
//         //     left_motor = 0;
//         //     right_motor = 0;
//         // }
//         // else
//         // {
//         //     left_motor = ( (steering + throttle) / fabsf(steering + throttle) )* 100;
//         //     right_motor = ( (steering + throttle) / fabsf(steering + throttle) )* 100;
//         // }
//         left_motor  = throttle + steering;
//         right_motor = throttle - steering;

//         // Clamp motors to ±100
//         if (left_motor > 100.0f)   left_motor = 100.0f;
//         if (left_motor < -100.0f)  left_motor = -100.0f;
//         if (right_motor > 100.0f)  right_motor = 100.0f;
//         if (right_motor < -100.0f) right_motor = -100.0f;
//         // printf("Left = %5f    Right = %5f    Steering = %5f    Throttle = %5f \n", left_motor, right_motor, steering, throttle );
//         //printf ("x: %5d  y: %5d  z: %5d\n", accel[0], accel[1], accel[2]);
//         printf("Left = %5f    Right = %5f\n",left_motor, right_motor);
//     }
// }
