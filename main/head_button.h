#pragma once
#include <stdbool.h>
#include <stdint.h>

/* A press is accepted only after a stable release and 2 s continuously low.
 * Any high sample cancels the press timer; a held button triggers only once. */
#define HEAD_BUTTON_RELEASE_US 30000LL
#define HEAD_BUTTON_HOLD_US 2000000LL

typedef struct {
    bool armed;
    int64_t low_since;
    int64_t high_since;
} head_button_state_t;

static bool button_long_press(head_button_state_t *state, bool pressed, int64_t now)
{
    if (!pressed) {
        state->low_since = -1;
        if (state->high_since < 0) state->high_since = now;
        if (now - state->high_since >= HEAD_BUTTON_RELEASE_US) state->armed = true;
        return false;
    }
    state->high_since = -1;
    if (!state->armed) return false;
    if (state->low_since < 0) state->low_since = now;
    if (now - state->low_since < HEAD_BUTTON_HOLD_US) return false;
    state->armed = false;
    state->low_since = -1;
    return true;
}
