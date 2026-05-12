#ifndef HAT_RADIO_H
#define HAT_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "nrf24.h"

#define RADIO_PAYLOAD_SIZE 32

nrf24_t *hat_radio_init(void);
uint8_t hat_radio_pwm_send(nrf24_t *nrf, int left_pwm, int right_pwm);
uint8_t hat_radio_read(nrf24_t *nrf, char *buffer);
bool hat_radio_stop_received(const char *buffer);

#endif
