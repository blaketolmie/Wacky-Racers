#include <stdio.h>
#include "usb_serial.h"
#include "pio.h"
#include "delay.h"
#include "target.h"

#define HELLO_DELAY_MS 1000

int main (void)
{
    mcu_jtag_disable ();

    int i = 0;



    // Redirect stdio to USB serial
    usb_serial_stdio_init ();

    while (1)
    {
        delay_ms (HELLO_DELAY_MS);
        printf ("IMU_OFF HIGH  %d\n", i++);
        pio_config_set (IMU_OFF, PIO_OUTPUT_HIGH);  // power on the IMU
        delay_ms (HELLO_DELAY_MS);
        printf ("IMU_OFF LOW  %d\n", i++);
        pio_config_set (IMU_OFF, PIO_OUTPUT_LOW);  // power on the IMU

    }
}
