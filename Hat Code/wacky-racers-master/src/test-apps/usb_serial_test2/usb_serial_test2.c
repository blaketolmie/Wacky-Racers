#include <stdio.h>
#include "usb_serial.h"
#include "pio.h"
#include "sys.h"
#include "pacer.h"
#include "delay.h"


#define PACER_RATE 1000
#define LED_FLASH_RATE 2


static void
prompt_command (void)
{
    printf ("> ");
    fflush (stdout);
}


static void
process_command (void)
{
    char buffer[80];
    char *str;

    str = fgets (buffer, sizeof (buffer), stdin);
    if (! str)
        return;

    // printf ("<<<%s>>>\n", str);

    switch (str[0])
    {
    case '0':
        pio_output_set (LED_ERROR_PIO, ! LED_ACTIVE);
        break;

    case '1':
        pio_output_set (LED_ERROR_PIO, LED_ACTIVE);
        break;

    case 'h':
        printf ("Hello world!\n");
        fflush (stdout);
        break;

    default:
       break;
    }

    prompt_command ();
}


int main (void)
{
    int flash_ticks = 0;
    int i;

    pio_config_set (LED_STATUS_PIO, PIO_OUTPUT_LOW);
    pio_output_set (LED_STATUS_PIO, LED_ACTIVE);

    pio_config_set (LED_ERROR_PIO, PIO_OUTPUT_LOW);
    pio_output_set (LED_ERROR_PIO, ! LED_ACTIVE);

    // Redirect stdio to USB serial
    usb_serial_stdio_init ();

    for (i = 0; i < 100; i++)
    {
        printf ("Hello world %d\n", i);
        fflush (stdout);
        delay_ms (100);
    }

    prompt_command ();

    pacer_init (PACER_RATE);

    while (1)
    {
        pacer_wait ();

	flash_ticks++;
	if (flash_ticks >= PACER_RATE / (LED_FLASH_RATE * 2))
	{
	    flash_ticks = 0;

	    pio_output_toggle (LED_STATUS_PIO);

            process_command ();
	}
    }
}
