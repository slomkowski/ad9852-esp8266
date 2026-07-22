#pragma once

#include <stdint.h>

/*
 * AD9852 DDS driver for ESP8266 (ESP-07), software SPI.
 *
 * Pin wiring (AD9852 signal -> ESP-07 GPIO):
 *   MASTER_RESET -> GPIO15  (active HIGH; GPIO15 is pulled LOW at boot = chip not in reset)
 *   SCB          -> GPIO4   (active LOW chip select)
 *   IO_RESET     -> GPIO5   (active HIGH pulse, resets serial address counter)
 *   SCLK         -> GPIO14
 *   SDIO         -> GPIO13  (write-only; MISO not connected)
 *   I/O UD       -> GPIO12  (active HIGH pulse, latches written values)
 */

#define AD9852_PIN_MRESET   15
#define AD9852_PIN_SCB      4
#define AD9852_PIN_IORESET  5
#define AD9852_PIN_SCLK     14
#define AD9852_PIN_SDIO     13
#define AD9852_PIN_IOUD     12

/* System clock fed to the AD9852 core.
 * Set PLL_BYPASS in the control register so SYSCLK == REFCLK == 66.667 MHz.
 * Adjust AD9852_SYSCLK_HZ and the CTRL bytes in ad9852_init() if you use the
 * on-chip PLL instead. */
#define AD9852_SYSCLK_HZ    66667000UL

#ifdef __cplusplus
extern "C" {
#endif

void ad9852_init(void);

void ad9852_set_freq(uint32_t freq_hz);

#ifdef __cplusplus
}
#endif
