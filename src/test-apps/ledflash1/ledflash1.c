/* File:   ledflash1.c
   Author: M. P. Hayes, UCECE
   Date:   15 May 2007
   Descr:  Flash an LED using a paced loop
*/
#include <pio.h>
#include "target.h"
#include "pacer.h"

#include <stdint.h>

/* Define PACER rate in Hz.  The minimum is 6 Hz for a 96 MHz CPU clock.  */
#define PACER_RATE 200

/* Define LED flash rate in Hz.  */
#define LED_FLASH_RATE 2


int
main (void)
{
    uint32_t ticks = 0;

    /* Configure STATUS LED PIO as output and set high.  The LED should
       turn on if wired active-high.  */
    pio_config_set (STAT0_PIO, PIO_OUTPUT_HIGH);
    pio_config_set (STAT1_PIO, PIO_OUTPUT_HIGH);
    pio_config_set (STAT2_PIO, PIO_OUTPUT_HIGH);
    pio_config_set (STAT3_PIO, PIO_OUTPUT_HIGH);

    pacer_init (PACER_RATE);

    while (1)
    {
        /* Wait until next clock tick.  */
        pacer_wait ();

        ticks++;

        if (ticks >= PACER_RATE / LED_FLASH_RATE / 2)
        {
            ticks = 0;

            /* Toggle LED.  */
            pio_output_toggle (STAT0_PIO);
            pio_output_toggle (STAT1_PIO);
            pio_output_toggle (STAT2_PIO);
            pio_output_toggle (STAT3_PIO);
        }
    }
}
