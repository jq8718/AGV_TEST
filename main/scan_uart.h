#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "xcr_decoder.h"

esp_err_t scan_uart_init(void);
bool scan_uart_submit(const XcrBarcodeResult *result, uint64_t deadline_us);

/* Latest UART latency sample, for optional USB diagnostics. */
typedef struct { uint32_t sequence; uint64_t frame_timestamp_us; uint32_t latency_us; } scan_uart_latency_t;
bool scan_uart_take_latency(scan_uart_latency_t *sample);
