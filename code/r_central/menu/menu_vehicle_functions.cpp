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
#include "menu_vehicle_functions.h"
#include "menu_channels_select.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_range.h"

MenuVehicleFunctions::MenuVehicleFunctions(void)
:Menu(MENU_ID_VEHICLE_FUNCTIONS, "Special Functions and Triggers", NULL)
{
   m_Width = 0.31;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.20;

   addTopLine("No functions or triggers to configure.");
}

MenuVehicleFunctions::~MenuVehicleFunctions()
{
}

void MenuVehicleFunctions::valuesToUI()
{
}

void MenuVehicleFunctions::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleFunctions::sendParams()
{
   type_functions_parameters params;
   memcpy(&params, &(g_pCurrentModel->functions_params), sizeof(type_functions_parameters));

   params.bEnableRCTriggerFreqSwitchLink1 = false;
   params.bEnableRCTriggerFreqSwitchLink2 = false;
   params.bEnableRCTriggerFreqSwitchLink3 = false;

   if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_FUNCTIONS_TRIGGERS_PARAMS, 0, (u8*)&params, sizeof(type_functions_parameters)) )
      valuesToUI();
}

void MenuVehicleFunctions::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);
}

void MenuVehicleFunctions::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   bool sendToVehicle = false;

   if ( sendToVehicle )
   {
      sendParams();
      return;
   }
}
