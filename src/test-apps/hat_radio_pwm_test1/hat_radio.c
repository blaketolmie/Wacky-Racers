#include <stdio.h>
#include <string.h>

#include "hat_radio.h"
#include "pio.h"
#include "delay.h"
#include "target.h"

#define RADIO_CHANNEL 5
#define RADIO_ADDRESS 0x0123456789LL
#define STOP_MESSAGE "STOP"

nrf24_t *hat_radio_init(void)
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
        .channel = RADIO_CHANNEL,
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

uint8_t hat_radio_pwm_send(nrf24_t *nrf, int left_pwm, int right_pwm)
{
    char buffer[RADIO_PAYLOAD_SIZE] = {0};

    snprintf(buffer, sizeof(buffer), "%d %d", left_pwm, right_pwm);

    return nrf24_write(nrf, buffer, RADIO_PAYLOAD_SIZE);
}

uint8_t hat_radio_read(nrf24_t *nrf, char *buffer)
{
    uint8_t bytes;

    bytes = nrf24_read(nrf, buffer, RADIO_PAYLOAD_SIZE);
    if (bytes != 0)
        buffer[bytes] = 0;

    return bytes;
}

bool hat_radio_stop_received(const char *buffer)
{
    return strcmp(buffer, STOP_MESSAGE) == 0;
}
