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

#include "../../base/encr.h"
#include "menu.h"
#include "menu_objects.h"
#include "menu_controller_encryption.h"
#include "menu_text.h"
#include "menu_item_text.h"
#include "menu_item_section.h"
#include "menu_confirmation.h"

#include <time.h>
#include <sys/resource.h>


MenuControllerEncryption::MenuControllerEncryption(void)
:Menu(MENU_ID_CONTROLLER_ENCRYPTION, "Controller Encryption Settings", NULL)
{
   m_Width = 0.29;
   m_xPos = menu_get_XStartPos(m_Width); m_yPos = 0.28;
   m_bHasPassPhrase = false;
   addItems();
}

void MenuControllerEncryption::addItems()
{
   addMenuItem( new MenuItemText("Sets the encryption pass phrase to be used to generate the key and encrypt the radio link.") );
   addMenuItem( new MenuItemText("Pass phrase should be at least 3 words of at least 3 letters each.") );

   char szBuff[128];

   int len = 64;
   int res = lpp(szBuff, len);
   if ( res )
   {
      m_bHasPassPhrase = true;
      strcpy(szBuff, "******");
   }
   else
   {
      m_bHasPassPhrase = false;
      strcpy(szBuff, "Not Set");
   }
   m_pItemPass = new MenuItemEdit("Pass phrase", szBuff);
   m_pItemPass->setOnlyLetters();
   m_pItemPass->setMaxLength(MAX_PASS_LENGTH-1);
   m_pItemPass->setIsEditable();
   m_IndexChangePass = addMenuItem(m_pItemPass);

   int countEncr = 0;
   for( int i=0; i<getControllerModelsCount(); i++ )
   {
      Model* pModel = getModelAtIndex(i);
      if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
      if ( m_bHasPassPhrase )
      {
         if ( 0 == countEncr )
            addMenuItem( new MenuItemText("These vehicles currently have encryption enabled:") );
         sprintf(szBuff, "%s", pModel->getLongName());
         addMenuItem( new MenuItemText(szBuff) );
         countEncr++;
      }
   }
}

void MenuControllerEncryption::valuesToUI()
{
   //ControllerSettings* pCS = get_ControllerSettings();
   //Preferences* pp = get_Preferences();
}

void MenuControllerEncryption::Render()
{
   RenderPrepare();
   float yTop = RenderFrameAndTitle();
   float y = yTop;

   for( int i=0; i<m_ItemsCount; i++ )
      y += RenderItem(i,y);
   RenderEnd(yTop);
}

int MenuControllerEncryption::onBack()
{
   if ( 2 != m_SelectedIndex )
      return Menu::onBack();

   if ( ! m_pItemPass->isEditing() )
      return Menu::onBack();

   log_line("End edit pp");
   m_pItemPass->endEdit(false);

   char szBuff[MAX_PASS_LENGTH];
   strncpy(szBuff, m_pItemPass->getCurrentValue(), MAX_PASS_LENGTH-1);

   m_pItemPass->setCurrentValue("******");
   m_pItemPass->invalidate();
   invalidate();

   if ( strlen(szBuff) < 11 )
   {
      MenuConfirmation* pMC = new MenuConfirmation("Invalid Pass Phrase","You need to specify a pass phrase that has at least 3 words each at least 3 characters long.",2, true);
      pMC->m_yPos = 0.3;
      add_menu_to_stack(pMC);
      return 1;
   }

   spp(szBuff);
   m_bHasPassPhrase = true;

   MenuConfirmation* pMC = new MenuConfirmation("Pass Phrase Updated","Your pass phrase was updated. You need to enable encryption on the vehicles you wish (use [Vehicle]->[General] menu for that).",1, true);
   pMC->m_yPos = 0.3;

   char szInfo[256];
   sprintf(szInfo, "Your new pass phrase is: %s", szBuff);
   pMC->addTopLine(szInfo);
   add_menu_to_stack(pMC);

   log_line("Send local message about pp change");

   t_packet_header PH;
   radio_packet_init(&PH, PACKET_COMPONENT_LOCAL_CONTROL, PACKET_TYPE_LOCAL_CONTROL_PASSPHRASE_CHANGED, STREAM_ID_DATA);
   handle_commands_send_ruby_message(&PH, NULL, 0);

   return 1;
}


void MenuControllerEncryption::onReturnFromChild(int iChildMenuId, int returnValue)
{
   Menu::onReturnFromChild(iChildMenuId, returnValue);

   if ( 2 == (iChildMenuId/1000) )
   {
      m_pItemPass->setCurrentValue("");
      m_pItemPass->beginEdit();
      m_pItemPass->invalidate();
      invalidate();
      return;
   }

   if ( (3 == (iChildMenuId/1000)) && (1 == returnValue) )
   {
      m_pItemPass->setCurrentValue("");
      m_pItemPass->beginEdit();
      m_pItemPass->invalidate();
      invalidate();
      return;
   }


   if ( 1 == (iChildMenuId/1000) )
      menu_stack_pop(0);
}

void MenuControllerEncryption::onSelectItem()
{
   if ( 2 == m_SelectedIndex )
   {
      if ( ! m_pItemPass->isEditing() )
      {
         int countEncr = 0;
         for( int i=0; i<getControllerModelsCount(); i++ )
         {
            Model* pModel = getModelAtIndex(i);
            if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
               countEncr++;
         }

         if ( m_bHasPassPhrase && countEncr > 0 )
         {
            MenuConfirmation* pMC = new MenuConfirmation("Pass Phrase Change Confirmation","Do you want to change your pass phrase? You will no longer be able to communicate with the following vehicles that already use encryption and the current keys:",3);
            pMC->addTopLine("These vehicles are currently use encryption and the current keys:");
            pMC->addTopLine("");
            for( int i=0; i<getControllerModelsCount(); i++ )
            {
               Model* pModel = getModelAtIndex(i);
               if ( pModel->enc_flags != MODEL_ENC_FLAGS_NONE )
               {
                  char szBuff[128];
                  sprintf(szBuff, "%s", pModel->getLongName());
                  pMC->addTopLine(szBuff);
               }
            }
            pMC->addTopLine("");
            pMC->addTopLine("You should first disable encryption on these vehicles before you change the pass phrase.");
            pMC->addTopLine("Are you sure you want to continue?");
            pMC->m_yPos = 0.24;
            add_menu_to_stack(pMC);
            return;
         }
         m_pItemPass->setCurrentValue("");
      }
      m_pItemPass->beginEdit();
      m_pItemPass->invalidate();
      invalidate();
      return;
   }

   Menu::onSelectItem();

   if ( m_pMenuItems[m_SelectedIndex]->isEditing() )
      return;

   ControllerSettings* pCS = get_ControllerSettings();
   if ( NULL == pCS )
   {
      log_softerror_and_alarm("Failed to get pointer to controller settings structure");
      return;
   }

   Preferences* pp = get_Preferences();
   if ( NULL == pp )
   {
      log_softerror_and_alarm("Failed to get pointer to preferences structure");
      return;
   }
}

