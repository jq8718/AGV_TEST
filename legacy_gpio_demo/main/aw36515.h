#pragma once

#include <stdint.h>

#include "esp_err.h"

/* AW36515 datasheet (Oct. 2019 V1.1): fixed 7-bit address 0x63. */
#define AW36515_I2C_ADDR             0x63
#define AW36515_REG_CHIP_ID          0x00
#define AW36515_REG_ENABLE           0x01
#define AW36515_REG_IVFM             0x02
#define AW36515_REG_LED1_FLASH       0x03
#define AW36515_REG_LED2_FLASH       0x04
#define AW36515_REG_LED1_TORCH       0x05
#define AW36515_REG_LED2_TORCH       0x06
#define AW36515_REG_BOOST_CONFIG     0x07
#define AW36515_REG_TIMING_CONFIG    0x08
#define AW36515_REG_FLAGS1           0x0A
#define AW36515_REG_FLAGS2           0x0B

esp_err_t aw36515_init(void);
/* Read a register; intended for diagnostics and I2C waveform capture. */
esp_err_t aw36515_read_register(uint8_t reg, uint8_t *value);
