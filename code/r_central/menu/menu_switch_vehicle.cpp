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
#include "menu_switch_vehicle.h"


MenuSwitchVehicle::MenuSwitchVehicle(u32 uVehicleId)
:Menu(MENU_ID_SWITCH_VEHICLE, "Switch Vehicle", NULL)
{
   m_Width = 0.4;
   m_xPos = 0.53-m_Width/2; m_yPos = 0.44;

   m_uVehicleId = uVehicleId;

   char szBuff[256];
   char szName[128];

   szBuff[0] = 0;
   szName[0] = 0;
   strcpy(szName, "Unknown");
   Model* pModelTmp = findModelWithId(m_uVehicleId, 16);
   if ( NULL != pModelTmp )
   strcpy(szName, pModelTmp->getLongName());

   if ( NULL != g_pCurrentModel )
   {
      sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency as your current vehicle (%s)!", szName, g_pCurrentModel->getLongName());
      if ( 1 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
         sprintf(szBuff, "Warning: There is a different vehicle (%s) on the same frequency (%s) as your current vehicle (%s)!", szName, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]), g_pCurrentModel->getLongName());
      else if ( 2 == g_pCurrentModel->radioInterfacesParams.interfaces_count )
      {
         char szFreq1[64];
         char szFreq2[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, g_pCurrentModel->getLongName());
      }
      else
      {
         char szFreq1[64];
         char szFreq2[64];
         char szFreq3[64];
         strcpy(szFreq1, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[0]));
         strcpy(szFreq2, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[1]));
         strcpy(szFreq3, str_format_frequency(g_pCurrentModel->radioInterfacesParams.interface_current_frequency_khz[2]));
         snprintf(szBuff, sizeof(szBuff)/sizeof(szBuff[0]), "Warning: There is a different vehicle (%s) on the same frequencies (%s/%s/%s) as your current vehicle (%s)!", szName, szFreq1, szFreq2, szFreq3, g_pCurrentModel->getLongName());
      }
   }
   if ( 0 != szBuff[0] )
   {
      addTopLine(szBuff);
      addTopLine("Switch to this vehicle or power it off as it will impact your current vehicle radio link quality.");
      addTopLine("Do you want to switch to this vehicle?");
   }
   addMenuItem(new MenuItem("Yes"));
   addMenuItem(new MenuItem("No"));
}


void MenuSwitchVehicle::onShow()
{
   Menu::onShow();
}

void MenuSwitchVehicle::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuSwitchVehicle::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( 1 == m_SelectedIndex )
   {
      menu_discard_all();
      return;
   }
   if ( 0 == m_SelectedIndex )
   {
      Model *pModel = findModelWithId(m_uVehicleId, 17);

      if ( NULL == pModel )
      {
         log_softerror_and_alarm("NULL model for vehicle to switch to.");
         menu_discard_all();
         return;
      }
      if ( NULL != g_pCurrentModel && (!g_pCurrentModel->is_spectator) && (g_pCurrentModel->uVehicleId == pModel->uVehicleId) )
      {
         menu_discard_all();
         return;
      }

      menu_discard_all();
      render_all(get_current_timestamp_ms(), true);
      Popup* p = new Popup("Switching vehicles...",0.3,0.64, 0.26, 0.2);
      popups_add_topmost(p);
      render_all(get_current_timestamp_ms(), true);

      pairing_stop();
      setCurrentModel(pModel->uVehicleId);
      g_pCurrentModel = getCurrentModel();
      setControllerCurrentModel(g_pCurrentModel->uVehicleId);
      saveControllerModel(g_pCurrentModel);

      ruby_set_active_model_id(g_pCurrentModel->uVehicleId);

      g_bIsFirstConnectionToCurrentVehicle = true;
      onMainVehicleChanged(true);
      pairing_start_normal();
   }
}

