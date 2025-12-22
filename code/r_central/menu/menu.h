#pragma once
#include "menu_objects.h"
#include "../../base/base.h"
#include "../../base/config.h"
#include "../../base/commands.h"
#include "../../base/ctrl_interfaces.h"
#include "../../base/ctrl_preferences.h"
#include "../../base/ctrl_settings.h"
#include "../../common/string_utils.h"
#include "../../renderer/render_engine.h"

#include "../../radio/radioflags.h"

#include "../pairing.h"
#include "../colors.h"
#include "../fonts.h"
#include "../local_stats.h"
#include "../shared_vars.h"
#include "../popup.h"
#include "../handle_commands.h"
#include "../ruby_central.h"
#include "../timers.h"
#include "../events.h"
#include "../notifications.h"
#include "../warnings.h"
#include "../process_router_messages.h"
#include "../ruby_central.h"

#define MAX_MENU_STACK 20

float menu_get_XStartPos(float fWidth);
float menu_get_XStartPos(Menu* pMenu, float fWidth);

float menu_setGlobalAlpha(float fAlpha);
float menu_getGlobalAlpha();

int menu_init();
void menu_loop(bool bNoKeys);
void menu_loop_parse_input_events();
void menu_refresh_all_menus();
void menu_refresh_all_menus_except(Menu* pMenu);
void menu_update_ui_all_menus();
void menu_rearrange_all_menus_no_animation();
void menu_rearrange_all_menus_xpos_no_animation();

void menu_render();
u32 menu_get_loop_counter();

void menu_discard_all();
void menu_discard_all_except(Menu* pMenu);
void add_menu_to_stack(Menu* pMenu);
void remove_menu_from_stack(Menu* pMenu);
void replace_menu_on_stack(Menu* pMenuSrc, Menu* pMenuNew);

void menu_stack_pop(int returnValue);
void menu_stack_pop_no_delete(int returnValue);
bool menu_is_menu_on_top(Menu* pMenu);
bool menu_has_children(Menu* pMenu);
bool menu_has_menu(int menuId);
int menu_had_disable_stacking(int iMenuId);
Menu* menu_get_menu_by_id(int menuId);
Menu* menu_get_top_menu();
bool isMenuOn();

void menu_invalidate_all();

int menu_hasAnyDisableStackingMenu();
void menu_startAnimationOnChildMenuAdd(Menu* pTopMenu);
void menu_startAnimationOnChildMenuClosed(Menu* pTopMenu);

bool menu_check_current_model_ok_for_edit();
bool menu_has_animation_in_progress();
