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
#include "menu_vehicle_selector.h"
#include "menu_confirmation.h"
#include "menu_item_text.h"
#include "../../common/favorites.h"

MenuVehicleSelector::MenuVehicleSelector(void)
:Menu(MENU_ID_VEHICLE_SELECTOR, L("Vehicle Actions"), NULL)
{
   m_Width = 0.18;
   m_xPos = menu_get_XStartPos(this, m_Width);
   m_bDisableStacking = true;
   m_bSpectatorMode = false;
   m_yPos = 0.18;
}

MenuVehicleSelector::~MenuVehicleSelector()
{
}

void MenuVehicleSelector::onShow()
{
   m_Height = 0.0;
   log_line("[Menu] Showing vehicle selector for vehicle index: %d", m_IndexSelectedVehicle);
   removeAllItems();

   Model *pModel = NULL;
   if ( m_bSpectatorMode )
      pModel = getSpectatorModel(m_IndexSelectedVehicle);
   else
      pModel = getModelAtIndex(m_IndexSelectedVehicle);

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[Menu] Vehicle Selector: NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      addMenuItem(new MenuItemText(L("Can't get vehicle info!")));
      return;
   }

   char szBuff[256];
   //sprintf(szBuff, "%s Actions", pModel->getLongName());
   strcpy(szBuff, L("Actions"));
   setTitle(szBuff);
   m_IndexSelect = addMenuItem(new MenuItem(L("Select"), L("Make this vehicle the active one.")));
   if ( vehicle_is_favorite(pModel->uVehicleId) )
      m_IndexFavorite = addMenuItem(new MenuItem(L("Remove from favorites"), L("Removes this vehicle from the list of favorite vehicles.")));
   else
      m_IndexFavorite = addMenuItem(new MenuItem(L("Add to favorites"), L("Add this vehicle to the list of favorite vehicles. You can switch quickly between favorite vehicles using a Quick Button action.")));
   m_IndexDelete = addMenuItem(new MenuItem(L("Delete"), L("Deletes this vehicle.")));

   Menu::onShow();
   log_line("[Menu] Showed vehicle selector for vehicle index: %d, VID: %u, spectator mode? %s", m_IndexSelectedVehicle, pModel->uVehicleId, m_bSpectatorMode?"yes":"no");
}

void MenuVehicleSelector::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;
   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}


void MenuVehicleSelector::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   Model *pModel = NULL;
   if ( m_bSpectatorMode )
      pModel = getSpectatorModel(m_IndexSelectedVehicle);
   else
      pModel = getModelAtIndex(m_IndexSelectedVehicle);

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      return;
   }

   // Delete model
   if ( (1 == returnValue) && (1 == iChildMenuId/1000) )
   {
      log_line("[Menu] VehicleSelector: Pressed command to delete model");
      log_line("[Menu] VehicleSelector: Deleting model VID %u, ptr: %X", pModel->uVehicleId, pModel);
      if ( NULL != g_pCurrentModel )
         log_line("[Menu] VehicleSelector: Current local model: VID: %u, ptr: %X", g_pCurrentModel->uVehicleId, g_pCurrentModel);

      if ( (NULL != g_pCurrentModel) && (g_pCurrentModel->uVehicleId == pModel->uVehicleId) )
      {
         log_line("[Menu] VehicleSelector: Pressed command to delete model: current model.");
         render_all(get_current_timestamp_ms(), true, false);
         pairing_stop();

         ruby_set_active_model_id(0);
         g_pCurrentModel = NULL;
      }

      deletePluginModelSettings(pModel->uVehicleId);
      save_PluginsSettings();
      deleteModel(pModel);
      log_line("[Menu] VehicleSelector: Deleted model 1/2.");
      notification_add_model_deleted();
      m_IndexSelectedVehicle = -1;
      menu_refresh_all_menus_except(this);
      menu_stack_pop(0);
      log_line("[Menu] VehicleSelector: Deleted model 2/2.");
      return;
   }
}


void MenuVehicleSelector::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   Model *pModel = NULL;
   if ( m_bSpectatorMode )
      pModel = getSpectatorModel(m_IndexSelectedVehicle);
   else
      pModel = getModelAtIndex(m_IndexSelectedVehicle);

   if ( NULL == pModel )
   {
      log_softerror_and_alarm("[MenuVehicleSelector] NULL model for vehicle index %d.", m_IndexSelectedVehicle);
      return;
   }

   if ( m_IndexDelete == m_SelectedIndex )
   {
      if ( (NULL != g_pCurrentModel) && (pModel->uVehicleId == g_pCurrentModel->uVehicleId) )
      if ( (!m_bSpectatorMode) && checkIsArmed() )
      {
         //addMessage("Can't delete current model as current vehicle is armed.");
         return;
      }
      char szBuff[64];
      sprintf(szBuff, L("Are you sure you want to delete %s?"), pModel->getLongName());
      add_menu_to_stack(new MenuConfirmation("Confirmation",szBuff, 1));
      return;
   }

   if ( m_IndexSelect == m_SelectedIndex )
   {
      log_line("[MenuVehicleSelector] Selecting VID %u", pModel->uVehicleId);
      if ( NULL != g_pCurrentModel )
      {
         log_line("[MenuVehicleSelector] Current VID before switching: %u / %u", g_pCurrentModel->uVehicleId, g_uActiveControllerModelVID);
         if ( (g_uActiveControllerModelVID == pModel->uVehicleId) && (g_pCurrentModel->uVehicleId == pModel->uVehicleId) )
         {
            menu_discard_all_except(this);
            return;
         }
      }
      log_line("[MenuVehicleSelector] Switching to VID: %u (mode: %s)", pModel->uVehicleId, pModel->is_spectator?"spectator mode":"control mode");
      menu_discard_all_except(this);
      warnings_remove_all();
      render_all(get_current_timestamp_ms(), true);
      Popup* p = new Popup(L("Switching vehicle..."), 0.3,0.64, 0.26, 0.2);
      popups_add_topmost(p);
      render_all(get_current_timestamp_ms(), true);

      pairing_stop();
      log_line("[MenuVehicleSelector] Stopped current pairing.");
      hardware_sleep_ms(100);

      if ( ! m_bSpectatorMode )
         pModel->is_spectator = false;
      setCurrentModel(pModel->uVehicleId);
      g_pCurrentModel = getCurrentModel();
      if ( ! m_bSpectatorMode )
         g_pCurrentModel->is_spectator = false;
      log_line("[MenuVehicleSelector] New VID %u mode: %s", pModel->uVehicleId, pModel->is_spectator?"spectator mode":"control mode");
      setControllerCurrentModel(g_pCurrentModel->uVehicleId);
      saveControllerModel(g_pCurrentModel);

      ruby_set_active_model_id(g_pCurrentModel->uVehicleId);

      onMainVehicleChanged(true);
      log_line("[MenuVehicleSelector] Starting new pairing...");
      pairing_start_normal();
      log_line("[MenuVehicleSelector] Started new pairing.");
      return;
   }

   if ( m_IndexFavorite == m_SelectedIndex )
   {
      if ( vehicle_is_favorite(pModel->uVehicleId) )
      {
         log_line("[MenuVehicleSelector] Remove VID %u from favorites.", pModel->uVehicleId);
         remove_favorite(pModel->uVehicleId);
      }
      else
      {
         log_line("[MenuVehicleSelector] Add VID %u to favorites.", pModel->uVehicleId);
         add_favorite(pModel->uVehicleId);
      }
      MenuItem* pItem = NULL;
      if ( vehicle_is_favorite(pModel->uVehicleId) )
         pItem = new MenuItem(L("Remove from favorites"), L("Removes this vehicle from the list of favorite vehicles."));
      else
        pItem = new MenuItem(L("Add to favorites"), L("Add this vehicle to the list of favorite vehicles. You can switch quickly between favorite vehicles using a Quick Button action."));
      m_pMenuItems[m_IndexFavorite] = pItem;

      save_favorites();
      invalidate();
      return;
   }
}
