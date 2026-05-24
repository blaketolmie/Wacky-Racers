/*
   Small nRF24L01 wrapper for the racer.

   The low-level nrf24 driver knows how to talk to the chip.  This file gives
   the racer app a simpler interface: initialise the radio, read a payload, or
   send the STOP message back to the hat after a bumper hit.
*/
#include <stdio.h>

#include "radio_link.h"
#include "pio.h"
#include "delay.h"
#include "target.h"

#define RADIO_ADDRESS 0x0123456789LL
#define STOP_MESSAGE "STOP"

nrf24_t *radio_link_init(uint8_t channel)
{
    /* SPI settings for the nRF24L01 module on this board. */
    spi_cfg_t spi_cfg =
    {
        .channel = 0,
        .clock_speed_kHz = 1000,
        .cs = RADIO_CS_PIO,
        .mode = SPI_MODE_0,
        .cs_mode = SPI_CS_MODE_FRAME,
        .bits = 8
    };
    nrf24_cfg_t nrf24_cfg =
    {
        /* Initial channel only.  The main code may hop channels later. */
        .channel = channel,
        .address = RADIO_ADDRESS,
        .payload_size = RADIO_PAYLOAD_SIZE,
        .ce_pio = RADIO_CE_PIO,
        .irq_pio = RADIO_IRQ_PIO,
        .spi = spi_cfg,
    };

#ifdef RADIO_OFF_PIO
    /* Power up the radio module before initialising it over SPI. */
    pio_config_set(RADIO_OFF_PIO, PIO_OUTPUT_HIGH);
    delay_ms(10);
#endif

    return nrf24_init(&nrf24_cfg);
}

uint8_t radio_link_read(nrf24_t *nrf, char *buffer)
{
    uint8_t bytes;

    bytes = nrf24_read(nrf, buffer, RADIO_PAYLOAD_SIZE);
    if (bytes != 0)
        /* Add a terminator for old text-based packets/debug prints. */
        buffer[bytes] = 0;

    return bytes;
}

uint8_t radio_link_stop_send(nrf24_t *nrf)
{
    /* The hat still listens for this short text message after bumper hits. */
    char buffer[RADIO_PAYLOAD_SIZE] = STOP_MESSAGE;
    uint8_t bytes;

    printf("Button pushed: sending STOP to hat...\r\n");
    fflush(stdout);

    bytes = nrf24_write(nrf, buffer, RADIO_PAYLOAD_SIZE);

    if (! bytes)
        printf("TX failed\r\n");
    else
        printf("TX: %s\r\n", buffer);

    fflush(stdout);

    return bytes;
}
