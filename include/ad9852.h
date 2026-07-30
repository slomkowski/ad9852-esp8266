#pragma once

#include <stdint.h>

/*
 * AD9852 DDS driver for ESP8266 (ESP-07), software SPI.
 *
 * Pin wiring (AD9852 signal -> ESP-07 GPIO):
 *   MASTER_RESET    -> GPIO12  (active HIGH)
 *   SCB  / RDB      -> GPIO15  (active LOW chip select; GPIO15 pull-down holds it LOW at boot,
 *                               chip is "selected" until ad9852_init() runs — harmless, no SCLK)
 *   IO_RESET / A2   -> GPIO13  (active HIGH pulse, resets serial address counter)
 *   SCLK / WRB      -> GPIO2   (GPIO2 must be HIGH at boot; pull-up holds it HIGH until pinMode()
 *                               is called; UART1 TX glitch on reset is cleared by MASTER_RESET)
 *   I/O UD          -> GPIO4   (active HIGH pulse, latches written values)
 *   SDIO / A0       -> GPIO5   (write-only; MISO not connected)
 */

#define AD9852_PIN_MRESET   12
#define AD9852_PIN_SCB      15
#define AD9852_PIN_IORESET  13
#define AD9852_PIN_SCLK     2
#define AD9852_PIN_IOUD     4
#define AD9852_PIN_SDIO     5

#ifdef __cplusplus
extern "C" {
#endif

void ad9852_init(void);

void ad9852_set_freq(double freq_hz);

uint32_t ad9852_read_control_reg(void);

#ifdef __cplusplus
}
#endif
