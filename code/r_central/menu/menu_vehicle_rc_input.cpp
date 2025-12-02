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

#include "../../base/hardware_i2c.h"
#include "menu.h"
#include "menu_vehicle_rc_input.h"
#include "menu_confirmation.h"

MenuVehicleRCInput::MenuVehicleRCInput(void)
:Menu(MENU_ID_VEHICLE_RC_INPUT+10*1000, "RC Input Source", NULL)
{
   m_Width = 0.32;
   m_Height = 0.0;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.26;
   m_bDisableStacking = true;
   char szLegend[256];
   
   hardware_i2c_log_devices();

   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   m_pItemsRadio[0] = new MenuItemRadio("", "");
   m_pItemsRadio[0]->addSelection("None");

   strcpy(szLegend, "Use this option when you connect a USB joystick, a USB gamepad or a USB RC transmitter to your controller");
   if ( 0 == pCI->inputInterfacesCount )
      strcat(szLegend, ".\nNo USB HID devices detected on the controller");
   m_pItemsRadio[0]->addSelection("USB HID device", szLegend);

   bool bHasRCIn = true;
   int i2cAddress = hardware_i2c_has_external_extenders_rcin();
   if ( (! hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN)) &&
        (! hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER)) &&
        (! hardware_i2c_has_device_id(i2cAddress)) )
      bHasRCIn = false;
   if ( 0 == i2cAddress )
      bHasRCIn = false;
   strcpy(szLegend, "Use this option when you connect a SBUS/IBUS input adapter to the I2C bus on the controller");
   if ( bHasRCIn )
   {
      log_line("No RC SBUS input device detected.");
      strcat(szLegend, ".\nNo RC In device detected on the I2C busses");
   }
   m_pItemsRadio[0]->addSelection("RC In (SBUS/IBUS)", szLegend);

   m_pItemsRadio[0]->setEnabled(true);
   m_IndexInputType = addMenuItem(m_pItemsRadio[0]);
}

MenuVehicleRCInput::~MenuVehicleRCInput()
{
}

void MenuVehicleRCInput::onShow()
{      
   Menu::onShow();
}

void MenuVehicleRCInput::valuesToUI()
{
   ControllerInterfacesSettings* pCI = get_ControllerInterfacesSettings();

   m_pItemsRadio[0]->setSelectedIndex(g_pCurrentModel->rc_params.inputType);
   m_pItemsRadio[0]->setFocusedIndex(g_pCurrentModel->rc_params.inputType);
   m_pItemsRadio[0]->enableSelectionIndex(0, true);

   if ( 0 == pCI->inputInterfacesCount )
      m_pItemsRadio[0]->enableSelectionIndex(1, false);
   else
      m_pItemsRadio[0]->enableSelectionIndex(1, true);

   bool bHasRCIn = true;
   int i2cAddress = hardware_i2c_has_external_extenders_rcin();
   if ( (! hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_RC_IN)) &&
        (! hardware_i2c_has_device_id(I2C_DEVICE_ADDRESS_PICO_EXTENDER)) &&
        (! hardware_i2c_has_device_id(i2cAddress)) )
      bHasRCIn = false;
   if ( 0 == i2cAddress )
      bHasRCIn = false;

   if ( bHasRCIn )
      m_pItemsRadio[0]->enableSelectionIndex(2, true);
   else
      m_pItemsRadio[0]->enableSelectionIndex(2, false);
}


void MenuVehicleRCInput::Render()
{
   RenderPrepare();   
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

void MenuVehicleRCInput::onSelectItem()
{
   Menu::onSelectItem();
   if ( (-1 == m_SelectedIndex) || (m_pMenuItems[m_SelectedIndex]->isEditing()) )
      return;

   if ( m_IndexInputType == m_SelectedIndex )
   {
      log_line("Selected option %d", m_pItemsRadio[0]->getFocusedIndex());
      m_pItemsRadio[0]->setSelectedIndex(m_pItemsRadio[0]->getFocusedIndex());

      rc_parameters_t params;
      memcpy(&params, &g_pCurrentModel->rc_params, sizeof(rc_parameters_t));
      int index = m_pItemsRadio[0]->getSelectedIndex();
      if ( 0 == index )
         params.inputType = RC_INPUT_TYPE_NONE;
      if ( 1 == index )
         params.inputType = RC_INPUT_TYPE_USB;
      if ( 2 == index )
         params.inputType = RC_INPUT_TYPE_RC_IN_SBUS_IBUS;

      if ( ! handle_commands_send_to_vehicle(COMMAND_ID_SET_RC_PARAMS, 0, (u8*)&params, sizeof(rc_parameters_t)) )
         valuesToUI();

      menu_stack_pop(1);
   }
}
