#pragma once
#include "../base/base.h"


#define INPUT_EVENT_PRESS_MENU ((u32)1)
#define INPUT_EVENT_PRESS_BACK ((u32)2)
#define INPUT_EVENT_PRESS_MINUS ((u32)4)
#define INPUT_EVENT_PRESS_PLUS ((u32)8)

#define INPUT_EVENT_PRESS_QA1 ((u32)16)
#define INPUT_EVENT_PRESS_QA2 ((u32)32)
#define INPUT_EVENT_PRESS_QA3 ((u32)64)
#define INPUT_EVENT_PRESS_QAPLUS ((u32)128)
#define INPUT_EVENT_PRESS_QAMINUS ((u32)256)


int keyboard_init();
int keyboard_uninit();

u32 keyboard_get_next_input_event();

int keyboard_consume_input_events();
int keyboard_has_long_press_flag();

u32 keyboard_get_triggered_input_events();
void keyboard_clear_triggered_back_event();
u32 keyboard_add_triggered_gpio_input_events();
