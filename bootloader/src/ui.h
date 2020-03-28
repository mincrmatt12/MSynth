#ifndef UI_H
#define UI_H

// Stripped DOWN button/led driver.

#include <stdint.h>

void ui_init();
uint32_t ui_get_button();

#define UI_BUTTON_N1 (1 << 0)
#define UI_BUTTON_N2 (1 << 5)
#define UI_BUTTON_N3 (1 << 10)
#define UI_BUTTON_BKSP (1 << 15)
#define UI_BUTTON_N4 (1 << 20)
#define UI_BUTTON_N5 (1 << 1)
#define UI_BUTTON_N6 (1 << 6)
#define UI_BUTTON_N7 (1 << 11)
#define UI_BUTTON_N8 (1 << 16)
#define UI_BUTTON_N9 (1 << 21)
#define UI_BUTTON_N0 (1 << 2)
#define UI_BUTTON_ENTER (1 << 7)
#define UI_BUTTON_PATCH (1 << 12)
#define UI_BUTTON_REC (1 << 17)
#define UI_BUTTON_PLAY (1 << 22)
#define UI_BUTTON_UP (1 << 3)
#define UI_BUTTON_DOWN (1 << 8)
#define UI_BUTTON_LEFT (1 << 13)
#define UI_BUTTON_RIGHT (1 << 18)
#define UI_BUTTON_F4 (1 << 23)
#define UI_BUTTON_F3 (1 << 4)
#define UI_BUTTON_F2 (1 << 9)
#define UI_BUTTON_F1 (1 << 14)

#endif
