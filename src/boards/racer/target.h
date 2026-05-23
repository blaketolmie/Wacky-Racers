/** @file   target.h
    @author M. P. Hayes, UCECE
    @date   12 February 2018
    @brief
*/
#ifndef TARGET_H
#define TARGET_H

#include "mat91lib.h"

/* This is for the carhat (chart) board configured as a racer!  */

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

/* USB  */
// #define USB_DETECT_PIO PA5_PIO      /* Pin 35  */
//#define USB_VBUS_PIO USB_DETECT_PIO
#define USB_CURRENT_MA 500

/* Status LEDs / status outputs */
#define STAT0_PIO PB1_PIO      /* Pin 4  */
#define STAT1_PIO PA16_PIO     /* Pin 19 - Green */
#define STAT2_PIO PA15_PIO     /* Pin 20 - Red */
#define STAT3_PIO PB2_PIO      /* Pin 5  */

/* ERROR and STATUS LED */
#define LED_STATUS_PIO STAT1_PIO   // Green
#define LED_ERROR_PIO STAT2_PIO    // Red

/* Active level low */
#define LED_ACTIVE 0

/* Buttons */
#define BUTTON_PIO PB0_PIO   /* Pin 3 - Button 1 */
#define BUTTON_PIO2 PB4_PIO   /* Pin 33 - Button 2 */

/* DIP switches */
#define DIP_SW_4_PIO PA17_PIO  /* Pin 9  */
#define DIP_SW_3_PIO PA18_PIO  /* Pin 10 */
#define DIP_SW_2_PIO PA21_PIO  /* Pin 11 */
#define DIP_SW_1_PIO PA22_PIO  /* Pin 14 */

/* H-bridges */
#define HBRIDGE_ENABLE_PIO PA0_PIO  /* Pin 48 */
#define MOTOR_RIGHT_PWM_PIO PA25_PIO     /* Pin 25, DC1 IN  - AIN1 - PWMH2*/
#define MOTOR_RIGHT_DIR_PIO PA20_PIO     /* Pin 16, DC1 OUT - AIN2 - PWML1*/
#define MOTOR_LEFT_PWM_PIO PA19_PIO    /* Pin 13, DC2 IN  - BIN1 - PWML0*/
#define MOTOR_LEFT_DIR_PIO PA24_PIO    /* Pin 23, DC2 OUT - BIN2 - PWMH1 */

/* Power / enable control */
#define SLEEP_PIO PA2_PIO           /* Pin 44 */
#define FPV_ENABLE_PIO PA1_PIO      /* Pin 47 */
#define PGOOD_PIO PA27_PIO          /* Pin 37 */

/* Servos */
#define SERVO1_PIO PA30_PIO         /* Pin 42 */
#define SERVO2_PIO PA29_PIO         /* Pin 41 */

/* Original definitions for servos so then template code can still work */
#define SERVO_PWM_PIO SERVO1_PIO
#define APPENDAGE_PIO SERVO2_PIO

/* Radio */
#define RADIO_OFF_PIO PA26_PIO      /* Pin 26 */
#define RADIO_CS_PIO PA9_PIO        /* Pin 30, RF_CSN */
#define RADIO_CE_PIO PA8_PIO        /* Pin 31, RF_CE */
#define RADIO_IRQ_PIO PA7_PIO       /* Pin 32, RF_IRQ */

/* IMU */
#define IMU_ENABLE_PIO PA10_PIO     /* Pin 29 */
#define IMU_INT1_PIO PA6_PIO        /* Pin 34 */
#define IMU_INT2_PIO PA11_PIO       /* Pin 28 */

/* Other inputs / outputs */
#define BUMPER_PIO PA28_PIO         /* Pin 38 */
#define BATTERY_MONITOR_PIO PB5_PIO /* Pin 49 */

/* LED strip */
#define LEDTAPE_PIO PA31_PIO        /* Pin 52, LED strip levelled */
#define LED_STRIP_NUMBER 18          /* Number of LEDs fitted to the strip */

#endif /* TARGET_H */
