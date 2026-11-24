#pragma once

/* Test: output SPLL / 20 on GPIO21 (EN_MCLK). Set 0 for normal static enable use. */
#ifndef BOARD_MCLK_TEST_ENABLE
#define BOARD_MCLK_TEST_ENABLE 1
#endif

/* 0: preserve boot state; 1800/3300: configure LDO_VO4 in app_main.
 * Requires the board's IO supply domain to be connected to LDO_VO4.
 * 3300 uses the LDO input rail directly; verify connected device ratings.
 */
#ifndef BOARD_VDDO4_MV
#define BOARD_VDDO4_MV 0
#endif

#if BOARD_VDDO4_MV != 0 && BOARD_VDDO4_MV != 1800 && BOARD_VDDO4_MV != 3300
#error "BOARD_VDDO4_MV must be 0, 1800, or 3300"
#endif
