#pragma once
#include <stdbool.h>
/* Runs before any camera/USB initialization; stays here while head power is off. */
void head_exchange_boot(void);
/* Persists the off state, cuts head power and reboots into the exchange loop. */
void head_exchange_power_off(void);
