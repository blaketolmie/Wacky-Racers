/* File:   ledflash2.c
   Author: M. P. Hayes, UCECE
   Date:   15 May 2007
   Descr:  Flash an LED using sysclock interrupt
*/
#include <pio.h>
#include "target.h"
#include "cpu.h"
#include "sysclock.h"

#include <stdint.h>

/* Define LED flash rate in Hz.  */
#define LED_FLASH_RATE 2


/* This is periodically called from an interrupt service routine so
   care is required if use shared variables.  */
static void heartbeat (void)
{
    static uint32_t ticks = 0;

    ticks++;

    if (ticks >= SYSCLOCK_HZ / LED_FLASH_RATE / 2)
    {
        ticks = 0;

        /* Toggle LED.  */
        pio_output_toggle (LED_STATUS_PIO);
    }
}


int
main (void)
{
    uint32_t ticks = 0;

    /* Configure STATUS LED PIO as output and set high.  The LED should
       turn on if wired active-high.  */
    pio_config_set (LED_STATUS_PIO, PIO_OUTPUT_HIGH);

    sysclock_init ();
    /* Register heartbeat function to be periodically called every
       sysclock tick.  */
    sysclock_callback (heartbeat);

    while (1)
    {
        /* This turns off CPU clock while waiting for an interrupt, in
           this case the sysclock interrupt.  This saves power.  */
        cpu_wfi ();
    }
}
