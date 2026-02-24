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
#include "menu_vehicle_camera_gains.h"
#include "menu_item_select.h"
#include "menu_item_slider.h"
#include "menu_item_legend.h"


MenuVehicleCameraGains::MenuVehicleCameraGains(void)
:Menu(MENU_ID_VEHICLE_CAMERA_GAINS, "Camera Analog Gains", NULL)
{
   m_Width = 0.20;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.40;

   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);

   if ( NULL != g_pCurrentModel && pCamProfile->whitebalance != 0 )
      addMenuItem(new MenuItemLegend("Warning", "Analog gains are ignored if AWB is not turned off.", 0) );


   m_pItemsRange[0] = new MenuItemRange("Analog Gain:", "Analog gain of the camera sensor when AWB is turned off.", 1.0, 8.0, pCamProfile->analogGain, 0.1 );
   m_pItemsRange[0]->setSufix("");
   m_IndexGain = addMenuItem(m_pItemsRange[0]);

   m_pItemsRange[1] = new MenuItemRange("Blue Gain:", "Blue gain when AWB is turned off.", 0.0, 5.0, pCamProfile->awbGainB, 0.1 );
   m_pItemsRange[1]->setSufix("");
   m_IndexGainB = addMenuItem(m_pItemsRange[1]);

   m_pItemsRange[2] = new MenuItemRange("Red Gain:", "Red gain when AWB is turned off.", 0.0, 5.0, pCamProfile->awbGainR, 0.1 );
   m_pItemsRange[2]->setSufix("");
   m_IndexGainR = addMenuItem(m_pItemsRange[2]);
}

MenuVehicleCameraGains::~MenuVehicleCameraGains()
{
}

void MenuVehicleCameraGains::updateUIValues()
{
   int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;
   camera_profile_parameters_t* pCamProfile = &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].profiles[iProfile]);
   m_pItemsRange[0]->setCurrentValue(pCamProfile->analogGain);
   m_pItemsRange[1]->setCurrentValue(pCamProfile->awbGainB);
   m_pItemsRange[2]->setCurrentValue(pCamProfile->awbGainR);
}

void MenuVehicleCameraGains::valuesToUI()
{
   updateUIValues();
}


void MenuVehicleCameraGains::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleCameraGains::onItemValueChanged(int itemIndex)
{
   Menu::onItemValueChanged(itemIndex);
}

void MenuVehicleCameraGains::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( handle_commands_is_command_in_progress() )
   {
      handle_commands_show_popup_progress();
      return;
   }

   if ( m_IndexGain == m_SelectedIndex && ! m_pMenuItems[m_IndexGain]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      cparams.profiles[iProfile].analogGain = m_pItemsRange[0]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }

   if ( m_IndexGainB == m_SelectedIndex && ! m_pMenuItems[m_IndexGainB]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      cparams.profiles[iProfile].awbGainB = m_pItemsRange[1]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }

   if ( m_IndexGainR == m_SelectedIndex && ! m_pMenuItems[m_IndexGainR]->isEditing() )
   {
      type_camera_parameters cparams;
      memcpy(&cparams, &(g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera]), sizeof(type_camera_parameters));
      int iProfile = g_pCurrentModel->camera_params[g_pCurrentModel->iCurrentCamera].iCurrentProfile;

      cparams.profiles[iProfile].awbGainR = m_pItemsRange[2]->getCurrentValue();
      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_CAMERA_PARAMETERS, g_pCurrentModel->iCurrentCamera, (u8*)(&cparams), sizeof(type_camera_parameters)) )
         valuesToUI();
   }
}