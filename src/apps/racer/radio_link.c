#include <stdio.h>

#include "radio_link.h"
#include "pio.h"
#include "delay.h"
#include "target.h"

#define RADIO_ADDRESS 0x0123456789LL
#define STOP_MESSAGE "STOP"

nrf24_t *radio_link_init(uint8_t channel)
{
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
        .channel = channel,
        .address = RADIO_ADDRESS,
        .payload_size = RADIO_PAYLOAD_SIZE,
        .ce_pio = RADIO_CE_PIO,
        .irq_pio = RADIO_IRQ_PIO,
        .spi = spi_cfg,
    };

#ifdef RADIO_OFF_PIO
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
        buffer[bytes] = 0;

    return bytes;
}

uint8_t radio_link_stop_send(nrf24_t *nrf)
{
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
