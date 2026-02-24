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
#include "../osd/osd_common.h"
#include "menu_vehicle_general.h"
#include "menu_item_select.h"
#include "menu_confirmation.h"
#include "menu_confirmation_vehicle_board.h"
#include "menu_item_text.h"

#include "../link_watch.h"
#include "../launchers_controller.h"
#include "../../base/encr.h"

MenuVehicleGeneral::MenuVehicleGeneral(void)
:Menu(MENU_ID_VEHICLE_GENERAL, L("General Vehicle Settings"), NULL)
{
   m_Width = 0.26;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.21;
   m_IndexBoardType = -1;
}

MenuVehicleGeneral::~MenuVehicleGeneral()
{
}

void MenuVehicleGeneral::addTopDescription()
{
   Preferences* pP = get_Preferences();
   removeAllTopLines();
   char szBuff[256];
   char szType[32];

   strcpy(szType, "runs");
   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      strcpy(szType, "flights");

   sprintf(szBuff, "Total %s: %d", szType, g_pCurrentModel->m_Stats.uTotalFlights);
   addTopLine(szBuff);

   int sec = (g_pCurrentModel->m_Stats.uTotalOnTime)%60;
   int min = (g_pCurrentModel->m_Stats.uTotalOnTime/60)%60;
   int hours = (g_pCurrentModel->m_Stats.uTotalOnTime/3600);

   sprintf(szBuff, "Total ON time: %dh:%02dm:%02ds", hours, min, sec);
   addTopLine(szBuff);

   sec = (g_pCurrentModel->m_Stats.uTotalFlightTime)%60;
   min = (g_pCurrentModel->m_Stats.uTotalFlightTime/60)%60;
   hours = (g_pCurrentModel->m_Stats.uTotalFlightTime/3600);

   strcpy(szType, "run time");
   if ( (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_DRONE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_AIRPLANE ||
        (g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK) == MODEL_TYPE_HELI )
      strcpy(szType, "flight time");
   sprintf(szBuff, "Total %s: %dh:%02dm:%02ds", szType, hours, min, sec);
   addTopLine(szBuff);

   if ( pP->iUnits == prefUnitsImperial )
      sprintf(szBuff, "Odometer: %.1f Mi", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   else
      sprintf(szBuff, "Odometer: %.1f Km", _osd_convertKm(g_pCurrentModel->m_Stats.uTotalFlightDistance/100.0/1000.0));
   addTopLine(szBuff);

   char szBuff2[64];
   getSystemVersionString(szBuff2, g_pCurrentModel->sw_version);

   sprintf(szBuff, "SW Version: %s", szBuff2);
   addTopLine(szBuff);

   addTopLine("");
}

void MenuVehicleGeneral::onShow()
{
   int iTmp = getSelectedMenuItemIndex();
   populate();
   Menu::onShow();
   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleGeneral::populate()
{
   int iTmp = getSelectedMenuItemIndex();

   removeAllItems();
   addTopDescription();

   m_pItemEditName = new MenuItemEdit(L("Name"), g_pCurrentModel->vehicle_name);
   m_pItemEditName->setMaxLength(MAX_VEHICLE_NAME_LENGTH-1);
   addMenuItem(m_pItemEditName);

   m_pItemsSelect[0] = new MenuItemSelect(L("Vehicle Type"), L("Changes the vehicle type Ruby is using. Has impact on things like OSD elements, telemetry parsing."));
   m_pItemsSelect[0]->addSelection(L("Generic"));
   m_pItemsSelect[0]->addSelection(L("Drone"));
   m_pItemsSelect[0]->addSelection(L("Airplane"));
   m_pItemsSelect[0]->addSelection(L("Helicopter"));
   m_pItemsSelect[0]->addSelection(L("Car"));
   m_pItemsSelect[0]->addSelection(L("Boat"));
   m_pItemsSelect[0]->addSelection(L("Robot"));
   m_pItemsSelect[0]->setIsEditable();
   m_IndexVehicleType = addMenuItem(m_pItemsSelect[0]);

   m_IndexBoardType = -1;
   if ( hardware_board_is_sigmastar(g_pCurrentModel->hwCapabilities.uBoardType) )
   {
      MenuItem* pMenuItem = new MenuItem(L("Board Type"), L("Sets the board type variant."));
      pMenuItem->showArrow();
      m_IndexBoardType = addMenuItem(pMenuItem);
   }
   if ( iTmp >= 0 )
   {
      m_SelectedIndex = iTmp;
      onFocusedItemChanged();
   }
}

void MenuVehicleGeneral::valuesToUI()
{
   populate();

   m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK);
}

void MenuVehicleGeneral::Render()
{
   RenderPrepare();

   float yTop = RenderFrameAndTitle();
   float y = yTop;

   float height_text = g_pRenderEngine->textHeight(g_idFontMenu);
   float iconHeight = 4.0*height_text;
   float iconWidth = iconHeight/g_pRenderEngine->getAspectRatio();

   u32 idIcon = osd_getVehicleIcon( g_pCurrentModel->vehicle_type );

   g_pRenderEngine->setColors(get_Color_MenuText(), 0.7);
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);
   g_pRenderEngine->drawIcon(m_RenderXPos+m_RenderWidth - iconWidth - m_sfMenuPaddingX , m_RenderYPos + m_RenderHeaderHeight+m_sfMenuPaddingY + iconHeight*0.05, iconWidth, iconHeight, idIcon);
   g_pRenderEngine->setColors(get_Color_MenuText());
   g_pRenderEngine->setStrokeSize(MENU_OUTLINEWIDTH);

   for( int i=0; i<m_ItemsCount; i++ )
   {
      float hItem = RenderItem(i,y);
      if ( i == m_IndexBoardType )
      {
         float dy = 0.5*(g_pRenderEngine->textHeight(g_idFontMenu) - g_pRenderEngine->textHeight(g_idFontMenu));
         float fWidth = g_pRenderEngine->textWidth(g_idFontMenu, str_get_hardware_board_name_short(g_pCurrentModel->hwCapabilities.uBoardType));
         g_pRenderEngine->drawText(m_RenderXPos + m_RenderWidth - m_sfMenuPaddingX - fWidth, y+dy, g_idFontMenu, str_get_hardware_board_name_short(g_pCurrentModel->hwCapabilities.uBoardType) );
      }
      y += hItem;
   }
   RenderEnd(yTop);
}


int MenuVehicleGeneral::onBack()
{
   if ( 0 == m_SelectedIndex )
   if ( m_pMenuItems[0]->isEditing() )
   {
      log_line("MenuVehicleGeneral: onBack: end editing name, current edit value: (%s)", m_pItemEditName->getCurrentValue());
      m_pMenuItems[0]->endEdit(false);
      char szBuff[MAX_VEHICLE_NAME_LENGTH+1];
      strncpy(szBuff, m_pItemEditName->getCurrentValue(), MAX_VEHICLE_NAME_LENGTH);
      log_line("MenuVehicleGeneral: onBack: sanitizing vehicle name: (%s)", szBuff);
      str_sanitize_modelname(szBuff);
      log_line("MenuVehicleGeneral: onBack: end sanitizing vehicle name.");
      if ( g_pCurrentModel->is_spectator )
      {
         strcpy(g_pCurrentModel->vehicle_name, (const char*)szBuff );
         g_pCurrentModel->constructLongName();
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
      }
      else if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_NAME, 0, (u8*)szBuff, MAX_VEHICLE_NAME_LENGTH) )
         m_pItemEditName->setCurrentValue(g_pCurrentModel->vehicle_name);

      log_line("MenuVehicleGeneral: onBack: done end editing name.");
      return 1;
   }

   return Menu::onBack();
}

void MenuVehicleGeneral::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexVehicleType == m_SelectedIndex )
   {
      u8 uVehicleType = (u8)(m_pItemsSelect[0]->getSelectedIndex());
      if ( g_pCurrentModel->is_spectator )
      {
         g_pCurrentModel->vehicle_type &= MODEL_FIRMWARE_MASK;
         g_pCurrentModel->vehicle_type |= (uVehicleType & MODEL_TYPE_MASK);
         saveControllerModel(g_pCurrentModel);
         send_model_changed_message_to_router(MODEL_CHANGED_GENERIC, 0);
      }
      else if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_VEHICLE_TYPE, uVehicleType, NULL, 0) )
         m_pItemsSelect[0]->setSelection(g_pCurrentModel->vehicle_type & MODEL_TYPE_MASK);
   }

   if ( (-1 != m_IndexBoardType) && (m_IndexBoardType == m_SelectedIndex) )
   {
       add_menu_to_stack( new MenuConfirmationVehicleBoard() );
       return;
   }
}