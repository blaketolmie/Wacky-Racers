/** @file   target.h
    @author M. P. Hayes, UCECE
    @date   12 February 2018
    @brief
*/
#ifndef TARGET_H
#define TARGET_H

#include "mat91lib.h"

/* This is for the carhat (chart) board configured as a hat!  */

/* System clocks  */
#define F_XTAL 12.00e6
#define MCU_PLL_MUL 16
#define MCU_PLL_DIV 1

#define MCU_USB_DIV 2
/* 192 MHz  */
#define F_PLL (F_XTAL / MCU_PLL_DIV * MCU_PLL_MUL)
/* 96 MHz  */
#define F_CPU (F_PLL / 2)

/* TWI  */
#define TWI_TIMEOUT_US_DEFAULT 10000
#define MPU_ADDRESS 0x68

/* USB  */
//#define USB_VBUS_PIO PA5_PIO
#define USB_CURRENT_MA 500

/* ADC  */
#define BATTERY_ADC_CHANNEL ADC_CHANNEL_1
#define JOYSTICK_X_ADC_CHANNEL ADC_CHANNEL_6
#define JOYSTICK_Y_ADC_CHANNEL ADC_CHANNEL_5

/* IMU  */
#define IMU_INT1_PIO PA31_PIO
#define IMU_INT2_PIO PA30_PIO
#define ADXL345_ADDRESS 0x1D        //0x1D or 0x53
#define IMU_OFF PB4_PIO

/* LEDs  */
#define LED_ERROR_PIO PA0_PIO //STAT0 DL3
#define LED_STATUS_PIO PA1_PIO //STAT1 DL4
#define LED_RED_PIO PA20_PIO //STAT2 DL1
#define LED_GREEN_PIO PA19_PIO //STAT3 DL2
#define LED_ACTIVE 0 //active low

/* General  */
#define APPENDAGE_PIO PA1_PIO
#define SERVO_PWM_PIO PA2_PIO

/* Button  */
#define BUTTON_SLEEP_PIO PB2_PIO
#define BUTTON_1_PIO PA15_PIO
#define BUTTON_2_PIO PA16_PIO

/* Dip Switch  */
#define DIPSW_1_PIO PA24_PIO
#define DIPSW_2_PIO PA26_PIO
#define DIPSW_3_PIO PA25_PIO
#define DIPSW_4_PIO PA23_PIO

/* Radio  */
#define RADIO_CS_PIO PA9_PIO
#define RADIO_CE_PIO PA8_PIO
#define RADIO_IRQ_PIO PA7_PIO
#define RADIO_OFF_PIO PA6_PIO

/* LED tape  */
#define LEDTAPE_PIO PA11_PIO
#define NUM_LEDS 22

/* Buck Converter */
#define PGOOD_PIO PB3_PIO

/* Throttle & Steering */
#define SCALE_NUM_X 2.6f
#define SCALE_NUM_Y 2.6f

/* Buzzer */
#define BUZZER_PIO PA17_PIO

/* Pacer */
#define PACER_RATE 100


#endif /* TARGET_H  */
