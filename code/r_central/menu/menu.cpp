/*
    Ruby Licence
    Copyright (c) 2020-2025 Petru Soroaga petrusoroaga@yahoo.com
    All rights reserved.

    Redistribution and/or use in source and/or binary forms, with or without
    modification, are permitted provided that the following conditions are met:
        * Redistributions and/or use of the source code (partially or complete) must retain
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Redistributions in binary form (partially or complete) must reproduce
        the above copyright notice, this list of conditions and the following disclaimer
        in the documentation and/or other materials provided with the distribution.
        * Copyright info and developer info must be preserved as is in the user
        interface, additions could be made to that info.
        * Neither the name of the organization nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.
        * Military use is not permitted.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE AUTHOR (PETRU SOROAGA) BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "menu.h"
#include "menu_objects.h"
#include "menu_root.h"
#include "menu_search.h"
#include "../../renderer/render_engine.h"
#include "osd_common.h"
#include "osd_ahi.h"
#include "osd.h"
#include "../../base/base.h"
#include "../../base/hardware.h"
#include "../../base/gpio.h"
#include "../../base/config.h"
#include "../../base/shared_mem_i2c.h"
#include "../../base/ctrl_settings.h"
#include "../../base/ctrl_interfaces.h"
#include "../popup_camera_params.h"
#include "../shared_vars.h"
#include "../timers.h"
#include "../ruby_central.h"
#include "../keyboard.h"
#include "../process_router_messages.h"

static int s_iCountRotaryEncoderCancelCount = 0;
static int s_iCountRotaryEncoder2CancelCount = 0;

Menu* g_pMenuStack[MAX_MENU_STACK];
int g_iMenuIds[MAX_MENU_STACK];
int g_iMenuReturnValue[MAX_MENU_STACK];
int g_iMenuDisableStackingFlag[MAX_MENU_STACK];

int g_iMenuStackTopIndex = 0;
static float s_fMenuGlobalAlpha = 1.0;

static u32 s_uTimeLastMenuRotaryEvent = 0;
static u16 s_uLastMenuRotaryEventIndex = 0;

static u32 s_uMenuLoopCounter = 0;
static bool s_bRotaryRotated = false;

float menu_get_XStartPos(float fWidth)
{
   Preferences* pP = get_Preferences();

   // Left side sticky menus?

   if ( pP->iMenuStyle == 1 )
   {
      return 0.0 + 0.03 * (g_iMenuStackTopIndex-1);
   }

   // Stacked menus ?

   if ( pP->iMenusStacked )
   {
      if ( 0 == g_iMenuStackTopIndex )
         return 0.04;
      return 0.04 + g_iMenuStackTopIndex * 0.02;
   }

   // Non stacked menus (side by side)

   if ( 0 == g_iMenuStackTopIndex )
      return 0.06;

   //float dx = g_pMenuStack[g_iMenuStackTopIndex-1]->m_Width*0.7;
   //if ( dx > 0.18 )
   //   dx = 0.18;
   float dx = 0.12;

   float xPos = g_pMenuStack[g_iMenuStackTopIndex-1]->m_xPos + dx;
   if ( xPos + fWidth > 0.98 )
      xPos = 0.98 - fWidth;
   return xPos;
}

float menu_get_XStartPos(Menu* pMenu, float fWidth)
{
   Preferences* pP = get_Preferences();
   if ( pP->iMenuStyle == 1 )
   {
      for( int i=0; i<MAX_MENU_STACK; i++ )
      {
         if ( g_pMenuStack[i] == pMenu )
            return 0.0 + 0.03 * i;
      }
      return 0.0 + 0.03 * (g_iMenuStackTopIndex-1);
   }

   float xPos = 0.1;
   if ( g_iMenuStackTopIndex > 0 )
      xPos = g_pMenuStack[g_iMenuStackTopIndex-1]->m_xPos + g_pMenuStack[g_iMenuStackTopIndex-1]->getRenderWidth()*0.5;
   return xPos;
}

float menu_setGlobalAlpha(float fAlpha)
{
   float f = s_fMenuGlobalAlpha;
   s_fMenuGlobalAlpha = fAlpha;
   return f;
}

float menu_getGlobalAlpha()
{
   return s_fMenuGlobalAlpha;
}

int menu_init()
{
   for( int i=0; i<MAX_MENU_STACK; i++ )
   {
      g_pMenuStack[i] = NULL;
      g_iMenuIds[i] = -1;
      g_iMenuReturnValue[i] = -1;
      g_iMenuDisableStackingFlag[i] = 0;
   }
   return 1;
}

void menu_discard_all()
{
   menu_discard_all_except(NULL);
}

void menu_discard_all_except(Menu* pMenu)
{
   log_line("[Menu] Discarding all menus...");
   for( int i=MAX_MENU_STACK-1; i>=0; i-- )
   {
      if ( (NULL != g_pMenuStack[i]) && (g_pMenuStack[i] != pMenu) )
      {
         g_pMenuStack[i]->setParent(NULL);
         delete g_pMenuStack[i];
      }
      g_pMenuStack[i] = NULL;
      g_iMenuIds[i] = -1;
      g_iMenuReturnValue[i] = -1;
      g_iMenuDisableStackingFlag[i] = 0;
   }
   g_iMenuStackTopIndex = 0; 
   log_line("[Menu] Discarded all menus.");
}


bool menu_has_menu(int menuId)
{
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( (NULL != g_pMenuStack[i]) && (g_pMenuStack[i]->m_MenuId == menuId) )
         return true;
   return false;
}

int menu_had_disable_stacking(int iMenuId)
{
   for( int i=0; i<=g_iMenuStackTopIndex; i++ )
   {
      if ( g_iMenuIds[i] != iMenuId )
         continue;
      return g_iMenuDisableStackingFlag[i];
   }
   return 0;
}

Menu* menu_get_menu_by_id(int menuId)
{
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( (NULL != g_pMenuStack[i]) && (g_pMenuStack[i]->m_MenuId == menuId) )
         return g_pMenuStack[i];
   return NULL;
}

Menu* menu_get_top_menu()
{
   if ( g_iMenuStackTopIndex > 0 )
      return g_pMenuStack[g_iMenuStackTopIndex-1];
   return NULL;
}

void add_menu_to_stack(Menu* pMenu)
{
   if ( NULL == pMenu )
      return;
   if ( g_iMenuStackTopIndex > 0 )
   {
      pMenu->setParent(g_pMenuStack[g_iMenuStackTopIndex-1]);
      g_pMenuStack[g_iMenuStackTopIndex-1]->onChildMenuAdd(pMenu);
   }
   pMenu->m_MenuDepth = g_iMenuStackTopIndex;
   g_pMenuStack[g_iMenuStackTopIndex] = pMenu;
   g_iMenuIds[g_iMenuStackTopIndex] = pMenu->m_MenuId;
   g_iMenuReturnValue[g_iMenuStackTopIndex] = -1;
   g_iMenuDisableStackingFlag[g_iMenuStackTopIndex] = pMenu->m_bDisableStacking;
   g_iMenuStackTopIndex++;
   if ( g_iMenuStackTopIndex < MAX_MENU_STACK )
   {
      g_iMenuReturnValue[g_iMenuStackTopIndex] = -1;
      g_iMenuIds[g_iMenuStackTopIndex] = -1;
   }
   pMenu->onAddToStack();
   pMenu->onShow();
}

void remove_menu_from_stack(Menu* pMenu)
{
   if ( NULL == pMenu )
      return;
   pMenu->setParent(NULL);
   int k = -1;
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( g_pMenuStack[i] == pMenu )
      {
         k = i;
         break;
      }

   if ( k < 0 )
      return;
   bool bIsTopMenu = false;
   if ( k == g_iMenuStackTopIndex-1 )
      bIsTopMenu = true;

   if ( bIsTopMenu )
   {
      menu_stack_pop_no_delete(0);
      return;
   }

   while ( k < g_iMenuStackTopIndex-1 )
   {
      g_pMenuStack[k] = g_pMenuStack[k+1];
      g_iMenuIds[k] = g_iMenuIds[k+1];
      g_iMenuReturnValue[k] = g_iMenuReturnValue[k+1];
      g_iMenuDisableStackingFlag[k] = g_iMenuDisableStackingFlag[k+1];
      k++;
   }
   g_iMenuStackTopIndex--;
   g_pMenuStack[g_iMenuStackTopIndex] = NULL;
   g_iMenuReturnValue[g_iMenuStackTopIndex] = -1;
   if ( bIsTopMenu )
   {
      g_iMenuIds[g_iMenuStackTopIndex+1] = 0;
      g_iMenuReturnValue[g_iMenuStackTopIndex] = 0;
      g_iMenuDisableStackingFlag[g_iMenuStackTopIndex] = g_pMenuStack[g_iMenuStackTopIndex]->m_bDisableStacking;
   }
   else
      g_iMenuDisableStackingFlag[g_iMenuStackTopIndex] = 0;
   delete pMenu;
}

void replace_menu_on_stack(Menu* pMenuSrc, Menu* pMenuNew)
{
   if ( NULL == pMenuSrc || NULL == pMenuNew )
      return;

   pMenuSrc->setParent(NULL);

   int k = -1;
   for( int i=0; i<g_iMenuStackTopIndex; i++ )
      if ( g_pMenuStack[i] == pMenuSrc )
      {
         k = i;
         break;
      }

   if ( k < 0 )
      return;

   if ( k > 0 )
      pMenuNew->setParent(g_pMenuStack[k-1]);
   pMenuNew->m_MenuDepth = pMenuSrc->m_MenuDepth;
   g_pMenuStack[k] = pMenuNew;
   g_iMenuIds[k] = pMenuNew->m_MenuId;
   g_iMenuReturnValue[k] = -1;
   g_iMenuDisableStackingFlag[k] = 0;
   pMenuNew->onShow();
}

void menu_stack_pop(int returnValue)
{
   if ( g_iMenuStackTopIndex <= 0 )
      return;

   log_line("[Menu] (loop %u): doing stack pop. %d menus in stack. Top menu id before pop: %d-%d, name: [%s]", s_uMenuLoopCounter%100, g_iMenuStackTopIndex, g_pMenuStack[g_iMenuStackTopIndex-1]->m_MenuId%1000, g_pMenuStack[g_iMenuStackTopIndex-1]->m_MenuId/1000, g_pMenuStack[g_iMenuStackTopIndex-1]->m_szTitle);
   g_iMenuStackTopIndex--;

   g_iMenuDisableStackingFlag[g_iMenuStackTopIndex] = g_pMenuStack[g_iMenuStackTopIndex]->m_bDisableStacking;
   g_iMenuReturnValue[g_iMenuStackTopIndex] = returnValue;
   g_pMenuStack[g_iMenuStackTopIndex]->setParent(NULL);
   delete g_pMenuStack[g_iMenuStackTopIndex];
   g_pMenuStack[g_iMenuStackTopIndex] = NULL;
}

void menu_stack_pop_no_delete(int returnValue)
{
   if ( g_iMenuStackTopIndex <= 0 )
      return;

   log_line("[Menu] (loop %u): doing stack pop. %d menus in stack. Top menu id before pop: %d-%d, name: [%s]", s_uMenuLoopCounter%100, g_iMenuStackTopIndex, g_pMenuStack[g_iMenuStackTopIndex-1]->m_MenuId%1000, g_pMenuStack[g_iMenuStackTopIndex-1]->m_MenuId/1000, g_pMenuStack[g_iMenuStackTopIndex-1]->m_szTitle);
   g_iMenuStackTopIndex--;

   g_iMenuDisableStackingFlag[g_iMenuStackTopIndex] = g_pMenuStack[g_iMenuStackTopIndex]->m_bDisableStacking;
   g_iMenuReturnValue[g_iMenuStackTopIndex] = returnValue;
   g_pMenuStack[g_iMenuStackTopIndex]->setParent(NULL);
   g_pMenuStack[g_iMenuStackTopIndex] = NULL;
}


void _menu_check_rotary_encoders_buttons( bool* pbSelect, bool* pbCancel, bool* pbRotatedCW, bool* pbRotatedCCW, bool* pbRotatedFastCW, bool* pbRotatedFastCCW, bool* pbSelect2, bool* pbCancel2, bool* pbRotatedCW2, bool* pbRotatedCCW2, bool* pbRotatedFastCW2, bool* pbRotatedFastCCW2)
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   if ( (NULL == g_pControllerSettings) || (NULL == pCI) )
      return;

   if ( (ruby_get_start_sequence_step() != START_SEQ_COMPLETED) && (ruby_get_start_sequence_step() != START_SEQ_FAILED) )
      return;

   if ( ! hardware_i2c_has_external_extenders_rotary_encoders() )
   if ( ! hardware_i2c_has_external_extenders_buttons() )
   if ( ! hardware_i2c_HasPicoExtender() )
      return;

   if ( NULL == g_pSMRotaryEncoderButtonsEvents )
      g_pSMRotaryEncoderButtonsEvents = shared_mem_i2c_rotary_encoder_buttons_events_open_for_read();

   if ( NULL == g_pSMRotaryEncoderButtonsEvents )
      return;

   if ( s_uLastMenuRotaryEventIndex == g_pSMRotaryEncoderButtonsEvents->uEventIndex )
      return;

   if ( s_uTimeLastMenuRotaryEvent == g_pSMRotaryEncoderButtonsEvents->uTimeStamp )
      return;

   t_shared_mem_i2c_rotary_encoder_buttons_events events;
   memcpy(&events, g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) );
   u32 uCRC = base_compute_crc32((u8*)&events, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));
   if ( uCRC != events.uCRC )
   {
      hardware_sleep_micros(200);
      memcpy(&events, g_pSMRotaryEncoderButtonsEvents, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) );
      u32 uCRC = base_compute_crc32((u8*)&events, sizeof(t_shared_mem_i2c_rotary_encoder_buttons_events) - sizeof(u32));
      if ( uCRC != events.uCRC )
         return;
   }

   s_uTimeLastMenuRotaryEvent = events.uTimeStamp;
   s_uLastMenuRotaryEventIndex = events.uEventIndex;

   if ( ! events.uHasRotaryEncoder )
      s_iCountRotaryEncoderCancelCount = 0;

   if ( events.uHasRotaryEncoder )
   if ( (events.uRotaryEncoderEvents & 0x01) || (events.uRotaryEncoderEvents & (0x01<<2)) || (events.uRotaryEncoderEvents & (0x01<<3)) )
      s_iCountRotaryEncoderCancelCount = 0;

   if ( ! events.uHasSecondaryRotaryEncoder )
      s_iCountRotaryEncoder2CancelCount = 0;

   if ( events.uHasSecondaryRotaryEncoder )
   if ( (events.uRotaryEncoder2Events & 0x01) || (events.uRotaryEncoder2Events & (0x01<<2)) || (events.uRotaryEncoder2Events & (0x01<<3)) )
      s_iCountRotaryEncoder2CancelCount = 0;

   // Cancel rotary encoder (long press on rotary encoder) ?
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & 0x02 )
   {
      s_iCountRotaryEncoderCancelCount++;
      if ( ! events.uHasSecondaryRotaryEncoder )
      if ( s_iCountRotaryEncoderCancelCount > 7 )
      {
         g_pControllerSettings->nRotaryEncoderFunction = 1;
         save_ControllerSettings();
         s_iCountRotaryEncoderCancelCount = 0;
      }
      *pbCancel = true;
      log_line("[Rotary1 Event] Cancel");
   }

   // Cancel rotary encoder 2 (long press on rotary encoder) ?
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & 0x02 )
   {
      s_iCountRotaryEncoder2CancelCount++;
      *pbCancel2 = true;
      log_line("[Rotary2 Event] Cancel");
   }

   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & 0x01 )
   {
      *pbSelect = true;
      log_line("[Rotary1 Event] Select");
   }
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<2) )
   {
      *pbRotatedCCW = true;
      log_line("[Rotary1 Event] RotCCW");
   }
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<3) )
   {
      *pbRotatedCW = true;
      log_line("[Rotary1 Event] RotCW");
   }
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<4) )
   {
      *pbRotatedFastCCW = true;
      log_line("[Rotary1 Event] RotFastCCW");
   }
   if ( events.uHasRotaryEncoder )
   if ( events.uRotaryEncoderEvents & (0x01<<5) )
   {
      *pbRotatedFastCW = true;
      log_line("[Rotary1 Event] RotFastCW");
   }
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & 0x01 )
   {
      *pbSelect2 = true;
      log_line("[Rotary2 Event] Select");
   }
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<2) )
      *pbRotatedCCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<3) )
      *pbRotatedCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<4) )
      *pbRotatedFastCCW2 = true;
   if ( events.uHasSecondaryRotaryEncoder )
   if ( events.uRotaryEncoder2Events & (0x01<<5) )
      *pbRotatedFastCW2 = true;

   if ( 0 != events.uButtonsEvents )
   {
      hardware_override_keys((events.uButtonsEvents & 0x01)?1:0, (events.uButtonsEvents & (0x01<<1))?1:0, (events.uButtonsEvents & (0x01<<2))?1:0, (events.uButtonsEvents & (0x01<<3))?1:0,
             ((events.uButtonsEvents>>16) & 0x01)?1:0, ((events.uButtonsEvents>>16) & (0x01<<1))?1:0, ((events.uButtonsEvents>>16) & (0x01<<2))?1:0, ((events.uButtonsEvents>>16) & (0x01<<3))?1:0);
   }
}


void menu_loop(bool bNoKeys)
{
   s_uMenuLoopCounter++;

   for( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
         g_pMenuStack[i]->periodicLoop();
   }

   if ( g_iMenuStackTopIndex > 0 )
   if ( g_iMenuReturnValue[g_iMenuStackTopIndex] != -1 )
   if ( g_iMenuIds[g_iMenuStackTopIndex] != -1 )
   {
       g_pMenuStack[g_iMenuStackTopIndex-1]->onReturnFromChild(g_iMenuIds[g_iMenuStackTopIndex], g_iMenuReturnValue[g_iMenuStackTopIndex]);
       for( int i=g_iMenuStackTopIndex; i<MAX_MENU_STACK; i++ )
       {
          g_iMenuReturnValue[i] = -1;
          g_iMenuIds[i] = -1;
          g_iMenuDisableStackingFlag[i] = 0;
       }
   }
 
   bool bRotarySelect = false;
   bool bRotaryCancel = false;
   bool bRotaryRotatedCW = false;
   bool bRotaryRotatedCCW = false;
   bool bRotaryRotatedFastCW = false;
   bool bRotaryRotatedFastCCW = false;
   
   bool bRotary2Select = false;
   bool bRotary2Cancel = false;
   bool bRotary2RotatedCW = false;
   bool bRotary2RotatedCCW = false;
   bool bRotary2RotatedFastCW = false;
   bool bRotary2RotatedFastCCW = false;
   
   _menu_check_rotary_encoders_buttons(&bRotarySelect, &bRotaryCancel, &bRotaryRotatedCW, &bRotaryRotatedCCW, &bRotaryRotatedFastCW, &bRotaryRotatedFastCCW, &bRotary2Select, &bRotary2Cancel, &bRotary2RotatedCW, &bRotary2RotatedCCW, &bRotary2RotatedFastCW, &bRotary2RotatedFastCCW);

   s_bRotaryRotated = bRotaryRotatedCW | bRotaryRotatedCCW | bRotaryRotatedFastCW | bRotaryRotatedFastCCW;
   //bool bRotary2Rotated = bRotary2RotatedCW | bRotary2RotatedCCW | bRotary2RotatedFastCW | bRotary2RotatedFastCCW;

   if ( g_pControllerSettings->nRotaryEncoderSpeed == 1 )
   {
      bRotaryRotatedFastCW = false;
      bRotaryRotatedFastCCW = false;
   }

   if ( g_pControllerSettings->nRotaryEncoderSpeed2 == 1 )
   {
      bRotary2RotatedFastCW = false;
      bRotary2RotatedFastCCW = false;
   }

   if ( (NULL != g_pControllerSettings) && (g_pControllerSettings->nRotaryEncoderFunction == 2) && (s_StartSequence == START_SEQ_COMPLETED) )
   {
      if ( (bRotarySelect || bRotaryRotatedCW || bRotaryRotatedCCW) && (!popups_has_popup(g_pPopupCameraParams)) )
      {
         if ( NULL == g_pPopupCameraParams )
            g_pPopupCameraParams = new PopupCameraParams();
         popups_add_topmost(g_pPopupCameraParams);
         g_pPopupCameraParams->invalidate();
         bRotarySelect = false;
      }
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(bRotaryRotatedCW, bRotaryRotatedCCW, bRotaryRotatedFastCW, bRotaryRotatedFastCCW, bRotarySelect, bRotaryCancel);
   }

   if ( (NULL != g_pControllerSettings) && (g_pControllerSettings->nRotaryEncoderFunction2 == 2) && (s_StartSequence == START_SEQ_COMPLETED) )
   {
      if ( (bRotary2Select || bRotary2RotatedCW || bRotary2RotatedCCW) && (!popups_has_popup(g_pPopupCameraParams)) )
      {
         if ( NULL == g_pPopupCameraParams )
            g_pPopupCameraParams = new PopupCameraParams();
         popups_add_topmost(g_pPopupCameraParams);
         g_pPopupCameraParams->invalidate();
         bRotary2Select = false;
      }
      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
         g_pPopupCameraParams->handleRotaryEvents(bRotary2RotatedCW, bRotary2RotatedCCW, bRotary2RotatedFastCW, bRotary2RotatedFastCCW, bRotary2Select, bRotary2Cancel);
   }

   if ( (NULL == g_pControllerSettings) || (g_pControllerSettings->nRotaryEncoderFunction != 1) )
   {
      bRotarySelect = false;
      bRotaryCancel = false;
      bRotaryRotatedCW = false;
      bRotaryRotatedCCW = false;
      bRotaryRotatedFastCW = false;
      bRotaryRotatedFastCCW = false;
      s_bRotaryRotated = false;
   }
   else
   {
      hardware_override_keys(bRotarySelect?1:0, bRotaryCancel?1:0, bRotaryRotatedCCW?1:0, bRotaryRotatedCW?1:0, 0, 0,0,0);
      keyboard_add_triggered_gpio_input_events();
   }

   if ( (NULL == g_pControllerSettings) || (g_pControllerSettings->nRotaryEncoderFunction2 != 1) )
   {
      bRotary2Select = false;
      bRotary2Cancel = false;
      bRotary2RotatedCW = false;
      bRotary2RotatedCCW = false;
      bRotary2RotatedFastCW = false;
      bRotary2RotatedFastCCW = false;
   }
   else
   {
      hardware_override_keys(bRotary2Select?1:0, bRotary2Cancel?1:0, bRotary2RotatedCCW?1:0, bRotary2RotatedCW?1:0, 0, 0,0,0);
      keyboard_add_triggered_gpio_input_events();
   }

   if ( (NULL != g_pControllerSettings) && (g_pControllerSettings->nRotaryEncoderFunction == 1) && (g_pControllerSettings->nRotaryEncoderFunction2 == 1) )
   {
      hardware_override_keys((bRotarySelect || bRotary2Select)?1:0, (bRotaryCancel || bRotary2Cancel)?1:0, (bRotaryRotatedCCW || bRotary2RotatedCCW)?1:0, (bRotaryRotatedCW || bRotary2RotatedCW)?1:0, 0, 0,0,0);
      keyboard_add_triggered_gpio_input_events();
   }

   if ( ! bNoKeys )
      menu_loop_parse_input_events();
}

void menu_loop_parse_input_events()
{
   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MENU )
   if ( ! keyboard_has_long_press_flag() )
   {
      log_line("[Menu] (loop %d) Pressed [Menu] Key", menu_get_loop_counter()%1000);
      if ( osd_is_stats_flight_end_on() )
      {
         osd_remove_stats_flight_end();
         return;
      }
      //system("omxplayer -p -o hdmi res/sound_menu_click2.wav");
      if ( 0 == g_iMenuStackTopIndex )
      {
          load_Preferences();
          add_menu_to_stack(new MenuRoot());
      }
      else 
         g_pMenuStack[g_iMenuStackTopIndex-1]->onSelectItem();
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_BACK )
   {
      log_line("[Menu] (loop %d) Pressed [Back] Key", menu_get_loop_counter()%1000);
      keyboard_clear_triggered_back_event();
      if ( (g_iMenuStackTopIndex < 1) && g_pControllerSettings->iEnableDebugStats )
      {
         //Preferences* pP = get_Preferences();
         //if ( (NULL != pP) && (pP->iDebugStatsQAButton == 0) )
         {
            g_pControllerSettings->iEnableDebugStats = 0;
            save_ControllerSettings();
            send_control_message_to_router(PACKET_TYPE_LOCAL_CONTROL_CONTROLLER_CHANGED, 0xFF);
         }
      }
      if ( osd_is_stats_flight_end_on() )
      {
         osd_remove_stats_flight_end();
         return;
      }

      if ( (g_iMenuStackTopIndex == 0) && g_bHasVideoDecodeStatsSnapshot )
      {
         g_bHasVideoDecodeStatsSnapshot = false;
         return;
      }
      
      if ( (NULL != g_pPopupCameraParams) && popups_has_popup(g_pPopupCameraParams) )
      {
         g_pPopupCameraParams->handleRotaryEvents(false, false, false, false, false, true);
         return;
      }
      //system("omxplayer -p -o hdmi res/sound_menu_click2.wav");
      if ( g_iMenuStackTopIndex > 0 )
      if ( (! g_pMenuStack[g_iMenuStackTopIndex-1]->hasDisabledBackAction()) || g_pMenuStack[g_iMenuStackTopIndex-1]->isEditingItem() )
         g_pMenuStack[g_iMenuStackTopIndex-1]->onBack();
      return;
   }

   if ( isKeyBackPressed() && keyboard_has_long_press_flag() )
   {
      log_line("[Menu] Long pressed [Back] Key");

      if ( NULL != g_pPopupCameraParams && popups_has_popup(g_pPopupCameraParams) )
      {
         g_pPopupCameraParams->handleRotaryEvents(false, false, false, false, false, true);
         return;
      }
      bool bHasModalMenu = false;
      for( int i=g_iMenuStackTopIndex-1; i>=0; i-- )
      {
         if ( NULL == g_pMenuStack[i] )
            continue;
         bHasModalMenu |= g_pMenuStack[i]->isModal();
         if ( g_pMenuStack[i]->hasDisabledBackAction() )
            bHasModalMenu = true;
         else if ( ! bHasModalMenu )
            g_pMenuStack[i]->onBack();
      }
      if ( ! bHasModalMenu )
         menu_discard_all();
      return;
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_MINUS )
   {
      log_line("[Menu] Pressed [Minus] Key");
      if ( g_iMenuStackTopIndex > 0 )
      if ( NULL != g_pMenuStack[g_iMenuStackTopIndex-1] )
      {
         g_pMenuStack[g_iMenuStackTopIndex-1]->onMoveDown(s_bRotaryRotated);
         if ( keyboard_has_long_press_flag() )
            g_pMenuStack[g_iMenuStackTopIndex-1]->onMoveDown(s_bRotaryRotated);
      }
   }

   if ( keyboard_get_triggered_input_events() & INPUT_EVENT_PRESS_PLUS )
   {
      log_line("[Menu] Pressed [Plus] Key");
      if ( g_iMenuStackTopIndex > 0 )
      if ( NULL != g_pMenuStack[g_iMenuStackTopIndex-1] )
      {
         g_pMenuStack[g_iMenuStackTopIndex-1]->onMoveUp(s_bRotaryRotated);
         if ( keyboard_has_long_press_flag() )
            g_pMenuStack[g_iMenuStackTopIndex-1]->onMoveUp(s_bRotaryRotated);
      }
   }
}

void menu_refresh_all_menus()
{
   menu_refresh_all_menus_except(NULL);
}

void menu_refresh_all_menus_except(Menu* pMenu)
{
   log_line("[Menu] Refresh all menus. Will refresh %d menus (except 0x%X):", g_iMenuStackTopIndex, pMenu);
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != g_pMenuStack[i]) && (g_pMenuStack[i] != pMenu) )
         log_line("[Menu]  * Menu id: %d, title: [%s], render xpos: %.2f", g_pMenuStack[i]->getId(), g_pMenuStack[i]->getTitle(), g_pMenuStack[i]->getRenderXPos());
   }

   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != g_pMenuStack[i]) && (g_pMenuStack[i] != pMenu) )
         g_pMenuStack[i]->onShow();
   }
   log_line("[Menu] Refreshed all menus.");
}

void menu_update_ui_all_menus()
{
   log_line("[Menu] Update UI for all menus...");
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
         g_pMenuStack[i]->valuesToUI();
   }
   log_line("[Menu] Updated UI for all menus.");
}

void menu_rearrange_all_menus_no_animation()
{
   log_line("[Menu] Rearrange all menus with no animation.");
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
         g_pMenuStack[i]->resetRenderXPos();
   }
   log_line("[Menu] Updated UI for all menus.");
}

void menu_rearrange_all_menus_xpos_no_animation()
{
   log_line("[Menu] Rearrange all menus xPos with no animation.");
   int iCountMenus = g_iMenuStackTopIndex;
   g_iMenuStackTopIndex = 0;
   for ( int i=0; i<iCountMenus; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
         g_pMenuStack[i]->m_xPos = menu_get_XStartPos(g_pMenuStack[i]->m_Width);
      g_iMenuStackTopIndex++;
   }
   
   menu_rearrange_all_menus_no_animation();
}

u32 menu_get_loop_counter()
{
   return s_uMenuLoopCounter;
}

void menu_render()
{
   if ( s_fMenuGlobalAlpha < 0.01 )
      return;
   float fOrigAlpha = g_pRenderEngine->getGlobalAlfa();
   bool bAlphaEnabled = g_pRenderEngine->isAlphaBlendingEnabled();
   Preferences* pP = get_Preferences();
   
   // If menus are stacked, render only last 3 menus

   int iMenuToRender = g_iMenuStackTopIndex-3;
   if ( iMenuToRender < 0 )
      iMenuToRender = 0;

   if ( ! pP->iMenusStacked )
      iMenuToRender = 0;

   while ( iMenuToRender < g_iMenuStackTopIndex )
   {
      g_pRenderEngine->setGlobalAlfa(s_fMenuGlobalAlpha);

      if ( g_pMenuStack[iMenuToRender]->m_bDisableBackgroundAlpha )
      {
         while ( iMenuToRender < g_iMenuStackTopIndex )
         {
            g_pMenuStack[iMenuToRender]->Render();
            iMenuToRender++;
         }
         break;
      }

      bool bNoBackgroundAdjustment = false;

      if ( bNoBackgroundAdjustment )
      {
         g_pMenuStack[iMenuToRender]->Render();
         iMenuToRender++;
         continue;
      }
      
      float fBgAlphaForMenu = g_pMenuStack[iMenuToRender]->m_fAlfaWhenInBackground - 0.08 * (float)(g_iMenuStackTopIndex-iMenuToRender-1);
      if ( pP->iMenuStyle == 1 )
         fBgAlphaForMenu = g_pMenuStack[iMenuToRender]->m_fAlfaWhenInBackground;
      if ( fBgAlphaForMenu < 0.0 )
         fBgAlphaForMenu = 0.0;

      u32 tMenuShowTime = g_pMenuStack[iMenuToRender]->getOnShowTime();
      u32 tMenuChildAddTime = g_pMenuStack[iMenuToRender]->getOnChildAddTime();
      u32 tMenuChildCloseTime = g_pMenuStack[iMenuToRender]->getOnReturnFromChildTime();

      bool bAnimatingAlpha = false;
      bool bUseBgAlpha = false;
      // On Show

      if ( g_TimeNow < tMenuShowTime + 250 )
      {
         float f = (g_TimeNow-tMenuShowTime)/250.0;
         if ( f < 0.0 ) f = 0.0;
         if ( f > 1.0 ) f = 1.0;
         float fTargetAlpha = s_fMenuGlobalAlpha * f + (1.0 - f) * fBgAlphaForMenu;
         g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
         bAnimatingAlpha = true;
      }

      // On child close

      if ( (0 != tMenuChildCloseTime) && (g_TimeNow < tMenuChildCloseTime+250) )
      {
         float f = (g_TimeNow-tMenuChildCloseTime)/250.0;
         if ( f < 0.0 ) f = 0.0;
         if ( f > 1.0 ) f = 1.0;
         float fTargetAlpha = s_fMenuGlobalAlpha * f + (1.0 - f) * fBgAlphaForMenu;
         g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
         bAnimatingAlpha = true;
      }

      // Child visible and not closing
      if ( (tMenuChildAddTime != 0) && (tMenuChildCloseTime == 0) )
      {
         if ( g_TimeNow >= tMenuChildAddTime + 250 )
            g_pRenderEngine->setGlobalAlfa(fBgAlphaForMenu);
         else
         {
            float f = (g_TimeNow-tMenuChildAddTime)/250.0;
            if ( f < 0.0 ) f = 0.0;
            if ( f > 1.0 ) f = 1.0;
            float fTargetAlpha = s_fMenuGlobalAlpha * (1.0 - f) + f * fBgAlphaForMenu;
            g_pRenderEngine->setGlobalAlfa(fTargetAlpha);
            bAnimatingAlpha = true;
         }
         bUseBgAlpha = true;
      }

      if ( ! bUseBgAlpha )
      if ( ! bAnimatingAlpha )
         g_pRenderEngine->setGlobalAlfa(s_fMenuGlobalAlpha);

      g_pMenuStack[iMenuToRender]->Render();
      iMenuToRender++;
   }
   g_pRenderEngine->setGlobalAlfa(fOrigAlpha);
   g_pRenderEngine->setAlphaBlendingEnabled(bAlphaEnabled);
}

bool menu_is_menu_on_top(Menu* pMenu)
{
   if ( g_iMenuStackTopIndex == 0 )
      return false;
   if ( g_pMenuStack[g_iMenuStackTopIndex-1] != pMenu )
      return false;
   return true;
}

bool menu_has_children(Menu* pMenu)
{
   if ( g_iMenuStackTopIndex == 0 )
      return false;
   if ( g_pMenuStack[g_iMenuStackTopIndex-1] != pMenu )
      return true;
   return false;
}

bool isMenuOn()
{
   if ( g_iMenuStackTopIndex == 0 )
      return false;
   return true;
}

void menu_invalidate_all()
{
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
         g_pMenuStack[i]->invalidate();
   }
}

int menu_hasAnyDisableStackingMenu()
{
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL != g_pMenuStack[i] )
      if ( g_pMenuStack[i]->m_bDisableStacking )
         return 1;
   }
   return 0;
}

// It is called just before the new child menu is added to the stack.
// So it's not yet actually present in the stack.

void menu_startAnimationOnChildMenuAdd(Menu* pTopMenu)
{
   bool bDoAnimate = true;
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != g_pMenuStack[i]) && g_pMenuStack[i]->m_bDisableStacking )
      {
         bDoAnimate = false;
         break;
      }
   }
   if ( ! bDoAnimate )
      return;
   
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL == g_pMenuStack[i] )
         continue;

      g_pMenuStack[i]->startAnimationOnChildMenuAdd();
      if ( g_pMenuStack[i] == pTopMenu )
         break;
   }
}

void menu_startAnimationOnChildMenuClosed(Menu* pTopMenu)
{
   bool bDoAnimate = true;
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( (NULL != g_pMenuStack[i]) && g_pMenuStack[i]->m_bDisableStacking )
      {
         bDoAnimate = false;
         break;
      }
   }
   if ( ! bDoAnimate )
      return;
   
   for ( int i=0; i<g_iMenuStackTopIndex; i++ )
   {
      if ( NULL == g_pMenuStack[i] )
         continue;

      g_pMenuStack[i]->startAnimationOnChildMenuClosed();
      if ( g_pMenuStack[i] == pTopMenu )
         break;
   }
}


bool menu_check_current_model_ok_for_edit()
{
   if ( NULL == g_pCurrentModel )
   {
      Menu* pm = new Menu(0,L("Info"), NULL);
      pm->m_xPos = 0.24; pm->m_yPos = 0.2;
      pm->m_Width = 0.3;
      pm->addTopLine(L("You have no vehicles. Search and connect to a vehicle first."));
      add_menu_to_stack(pm);
      return false;
   }
      
   if ( g_pCurrentModel->is_spectator )
   {
         Menu* pm = new Menu(0,L("Info"), NULL);
         pm->m_xPos = 0.24; pm->m_yPos = 0.2;
         pm->m_Width = 0.3;
         pm->addTopLine(L("You are currently paired with a vehicle in spectator mode. You can not change any vehicle settings while in spectator mode."));
         add_menu_to_stack(pm);
         return false;
   }

   return true;
}

bool menu_has_animation_in_progress()
{
   if ( g_iMenuStackTopIndex < 2 )
      return false;
   return true;
}
