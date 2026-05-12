#ifndef RADIO_LINK_H
#define RADIO_LINK_H

#include <stdint.h>

#include "nrf24.h"

#define RADIO_PAYLOAD_SIZE 32

nrf24_t *radio_link_init(void);
uint8_t radio_link_read(nrf24_t *nrf, char *buffer);
uint8_t radio_link_stop_send(nrf24_t *nrf);

#endif
