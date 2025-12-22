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

#include "../base/base.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/ctrl_settings.h"
#include "keyboard.h"
#include "timers.h"
#include <pthread.h>
#include <errno.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "warnings.h"

#define MAX_INPUT_EVENTS 64
#define MAX_INPUT_DEVICES 6

bool s_bKeyboardInitDone = false;
pthread_t s_pThreadKeyboard;
pthread_mutex_t s_pThreadKeyboardMutex;

u32 s_uNextKeyboardDetectTime = 0;
int s_iKeyboardDetectTryCount = 0;
bool s_bHasLongPressFlag = false;
typedef struct 
{
   int iFile;
   bool bIgnore;
   unsigned short id[4];
   char szName[256];
   unsigned long evbit;
} input_device_info;

input_device_info s_InputDevicesInfo[MAX_INPUT_DEVICES];

u32 s_uKeyboardInputEvents[MAX_INPUT_EVENTS];
int s_iCountKeyboardInputEvents = 0;
u32 s_uKeyboardInputEventsSum = 0;
u32 s_uKeyboardLastQAActionEventTime = 0;


bool _input_device_has_key(int iFile, int iKey)
{
  size_t nchar = KEY_MAX/8 + 1;
  unsigned char bits[nchar];
  ioctl(iFile, EVIOCGBIT(EV_KEY, sizeof(bits)), &bits);
  if ( bits[iKey/8] & (1 << (iKey % 8)) )
     return true;
  return false;
}

void _close_remove_input_device_info(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= MAX_INPUT_DEVICES) )
      return;

   warnings_add_input_device_removed(s_InputDevicesInfo[iIndex].szName);

   if ( s_InputDevicesInfo[iIndex].iFile >= 0 )
      close(s_InputDevicesInfo[iIndex].iFile);

   s_InputDevicesInfo[iIndex].iFile = -1;
   s_InputDevicesInfo[iIndex].szName[0] = 0;
   s_InputDevicesInfo[iIndex].bIgnore = false;
}

bool _keyboard_try_detect()
{
   char szBuff[256];
   bool bDetectedNewDevices = false;

   //log_line("[Keyboard] try detection");

   // First, check if current devices are still valid

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      if ( s_InputDevicesInfo[i].iFile < 0 )
         continue;

      sprintf(szBuff, "/dev/input/event%d", i);
      if ( access( szBuff, R_OK ) == -1 )
      {
         log_line("[Keyboard] Input device index %d, name [%s] is no longer present or working. Closing it.", i, s_InputDevicesInfo[i].szName);
         _close_remove_input_device_info(i);
         continue;
      }

      unsigned short id[4];
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGID, id) < 0 )
      {
         log_line("[Keyboard] Input device index %d, name [%s] is no longer present or working. Closing it.", i, s_InputDevicesInfo[i].szName);
         _close_remove_input_device_info(i);
         continue;
      }

      if (( s_InputDevicesInfo[i].id[0] != id[0] ) ||
         ( s_InputDevicesInfo[i].id[1] != id[1] ) ||
         ( s_InputDevicesInfo[i].id[2] != id[2] ) ||
         ( s_InputDevicesInfo[i].id[3] != id[3] ) )
      {
         log_line("[Keyboard] Input device index %d, name [%s] is no longer present or working. Closing it.", i, s_InputDevicesInfo[i].szName);
         _close_remove_input_device_info(i);
         continue;
      }

      char device_name[256];
      device_name[0] = 0;
      device_name[254] = 0;
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGNAME(sizeof(device_name) - 1), &device_name) < 0 )
      {
         log_line("[Keyboard] Input device index %d, name [%s] is no longer present or working. Closing it.", i, s_InputDevicesInfo[i].szName);
         _close_remove_input_device_info(i);
         continue;
      }
      if ( 0 != strcmp(s_InputDevicesInfo[i].szName, device_name) )
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGNAME(sizeof(device_name) - 1), &device_name) < 0 )
      {
         log_line("[Keyboard] Input device index %d, name [%s] is no longer present or working. Closing it.", i, s_InputDevicesInfo[i].szName);
         _close_remove_input_device_info(i);
         continue;
      }
   }

   // Check for new devices

   if ( access( "/dev/input/event0", R_OK ) == -1 )
      return false;

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      // Valid device, skip it
      if ( s_InputDevicesInfo[i].iFile >= 0 )
         continue;
      if ( s_InputDevicesInfo[i].bIgnore )
         continue;

      sprintf(szBuff, "/dev/input/event%d", i);
      if ( access( szBuff, R_OK ) == -1 )
         continue;

      s_InputDevicesInfo[i].iFile = open(szBuff, O_RDONLY | O_NONBLOCK);
      if ( s_InputDevicesInfo[i].iFile < 0 )
      {
         log_softerror_and_alarm("[Keyboard] Failed to open input device [%s], error: [%s]", szBuff, strerror(errno));
         continue;
      }
      s_InputDevicesInfo[i].szName[0] = 0;
      s_InputDevicesInfo[i].id[0] = 0;
      s_InputDevicesInfo[i].id[1] = 0;
      s_InputDevicesInfo[i].id[2] = 0;
      s_InputDevicesInfo[i].id[3] = 0;
      
      unsigned long evbit = 0;
      memset((u8*)&evbit, 0, sizeof(evbit));
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGBIT(0, sizeof(evbit)), &evbit) < 0 )
      {
         log_softerror_and_alarm("[Keyboard] Failed to ioctl for bits, device [%s], error: [%s]", szBuff, strerror(errno));
         close(s_InputDevicesInfo[i].iFile);
         s_InputDevicesInfo[i].iFile = -1;
         continue;
      }

      bool bHasKeyEvents = false;
      if ( evbit & (1<<EV_KEY) )
        bHasKeyEvents = true;

      char ch = 'a';
      int asci = (int)ch;
      if ( ! _input_device_has_key(s_InputDevicesInfo[i].iFile, 32) )
         bHasKeyEvents = false;
      if ( ! _input_device_has_key(s_InputDevicesInfo[i].iFile, asci) )
         bHasKeyEvents = false;

      if ( ! bHasKeyEvents )
      {
         close(s_InputDevicesInfo[i].iFile);
         s_InputDevicesInfo[i].iFile = -1;
         continue;
      }

      unsigned short id[4];
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGID, id) < 0 )
      {
         log_softerror_and_alarm("[Keyboard] Failed to ioctl for id, device [%s], error: [%s]", szBuff, strerror(errno));
         close(s_InputDevicesInfo[i].iFile);
         s_InputDevicesInfo[i].iFile = -1;
         continue;
      }

      s_InputDevicesInfo[i].id[0] = id[0];
      s_InputDevicesInfo[i].id[1] = id[1];
      s_InputDevicesInfo[i].id[2] = id[2];
      s_InputDevicesInfo[i].id[3] = id[3];

      char device_name[256];
      device_name[0] = 0;
      device_name[254] = 0;
      if ( ioctl(s_InputDevicesInfo[i].iFile, EVIOCGNAME(sizeof(device_name) - 1), &device_name) < 0 )
      {
         log_softerror_and_alarm("[Keyboard] Failed to ioctl for name, device [%s], error: [%s]", szBuff, strerror(errno));
         close(s_InputDevicesInfo[i].iFile);
         s_InputDevicesInfo[i].iFile = -1;
         continue;
      }
      strcpy(s_InputDevicesInfo[i].szName, device_name);

      // check for duplicate
      bool bDuplicate = false;
      for( int k=0; k<i; k++ )
      {
         if ( s_InputDevicesInfo[k].iFile >= 0 )
         if ( s_InputDevicesInfo[k].id[0] == id[0] )
         if ( s_InputDevicesInfo[k].id[1] == id[1] )
         if ( s_InputDevicesInfo[k].id[2] == id[2] )
         if ( s_InputDevicesInfo[k].id[3] == id[3] )
            bDuplicate = true;
      }

      if ( bDuplicate )
      {
         close(s_InputDevicesInfo[i].iFile);
         s_InputDevicesInfo[i].iFile = -1;
         continue;
      }

      log_line("[Keyboard] Found new input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x", id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);
      log_line("[Keyboard] New input device name: [%s]", device_name);
      log_line("[Keyboard] Saved it to index %d, file id: %d", i, s_InputDevicesInfo[i].iFile);

      warnings_add_input_device_added(device_name);
      bDetectedNewDevices = true;
   }
   return bDetectedNewDevices;
}

void _add_input_event(u32 uEvent, bool bFromKeyboard)
{
   if ( (uEvent == INPUT_EVENT_PRESS_QA1) ||
        (uEvent == INPUT_EVENT_PRESS_QA2) ||
        (uEvent == INPUT_EVENT_PRESS_QA3) )
   {
      if ( get_current_timestamp_ms() < s_uKeyboardLastQAActionEventTime + 100 )
         return;
      s_uKeyboardLastQAActionEventTime = get_current_timestamp_ms();
   }

   //if ( uEvent == INPUT_EVENT_PRESS_MENU )
      log_line("[Input] Added input event %u %s, %d events already in queue", uEvent, bFromKeyboard?"from keyboard":"from GPIO", s_iCountKeyboardInputEvents);
   int iLock = pthread_mutex_lock(&s_pThreadKeyboardMutex);

   s_uKeyboardInputEvents[s_iCountKeyboardInputEvents] = uEvent;
   s_iCountKeyboardInputEvents++;
   if ( s_iCountKeyboardInputEvents >= MAX_INPUT_EVENTS-1 )
   {
      for( int p=0; p<s_iCountKeyboardInputEvents-1; p++ )
         s_uKeyboardInputEvents[p] = s_uKeyboardInputEvents[p+1];
      s_iCountKeyboardInputEvents--;
   }
   if ( 0 == iLock )
         pthread_mutex_unlock(&s_pThreadKeyboardMutex);
}

int _read_keyboard_input_events()
{
   fd_set readset;
   int iMaxFD = 0;
   struct timeval tv;

   FD_ZERO(&readset);
   bool bHasAnyInputDevice = false;

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      if ( s_InputDevicesInfo[i].iFile < 0 )
        continue;
      bHasAnyInputDevice = true;
      FD_SET(s_InputDevicesInfo[i].iFile, &readset);
      if ( s_InputDevicesInfo[i].iFile > iMaxFD )
         iMaxFD = s_InputDevicesInfo[i].iFile;
   }

   if ( ! bHasAnyInputDevice )
      return 0;

   tv.tv_sec = 0;
   tv.tv_usec = 100; // 0.1 miliseconds timeout

   int selectResult = select(iMaxFD+1, &readset, NULL, NULL, &tv);
   if ( selectResult <= 0 )
      return selectResult;

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      if ( s_InputDevicesInfo[i].iFile < 0 )
         continue;
      if( 0 == FD_ISSET(s_InputDevicesInfo[i].iFile, &readset) )
         continue;

      struct input_event events[64];

      int iRead = read(s_InputDevicesInfo[i].iFile, events, sizeof(struct input_event) * 64);
      if ( iRead < 0 )
      {
         _close_remove_input_device_info(i);
         return iRead;
      }
      if ( iRead < (int)sizeof(struct input_event) )
         continue;

      for( int k=0; k < iRead/(int)sizeof(struct input_event); k++)
      {
         if ( events[k].value == 2 )
            log_line("[Keyboard] Autorepeat key %d", events[k].code);

         if ( events[k].value == 1 )
         {
            u32 uEvent = 0;

            if ( (events[k].code == 28) || (events[k].code == 57) || (events[k].code == 96) )
               uEvent = INPUT_EVENT_PRESS_MENU;
            else if ( (events[k].code == 14) || (events[k].code == 1) )
               uEvent = INPUT_EVENT_PRESS_BACK;
            else if ( (events[k].code == 103) || (events[k].code == 22) || (events[k].code == 72) || (events[k].code == 75) )
               uEvent = INPUT_EVENT_PRESS_PLUS;
            else if ( (events[k].code == 108) || (events[k].code == 32) || (events[k].code == 80) || (events[k].code == 77) )
               uEvent = INPUT_EVENT_PRESS_MINUS;
            else if ( (events[k].code == 2) || (events[k].code == 79) )
               uEvent = INPUT_EVENT_PRESS_QA1;
            else if ( (events[k].code == 3) )
               uEvent = INPUT_EVENT_PRESS_QA2;
            else if ( (events[k].code == 4) || (events[k].code == 81) || (events[k].code == 76) )
               uEvent = INPUT_EVENT_PRESS_QA3;
            else if ( events[k].code != 69 )
            {
               log_line("[Keyboard] Pressed unknown key %d", events[k].code);
               uEvent = ((u32)events[k].code) << 16;
            }
            if ( uEvent > 0 )
               _add_input_event(uEvent, true);
         }
      }
   }
   return 0;
}

static void * _thread_keyboard(void *argument)
{
   log_line("[Keyboard] Thread started.");
   ControllerSettings* pCS = get_ControllerSettings();
   if ( pCS->iPrioritiesAdjustment )
      hw_set_current_thread_raw_priority("keyboard", pCS->iThreadPriorityOthers);
   hw_log_current_thread_attributes("keyboard");
   
   bool* pbInitialized = (bool*) argument;
   
   while ( (*pbInitialized) )
   {
      hardware_loop();
      if ( ! (*pbInitialized) )
         break;
      if ( isKeyMenuPressed() )
         _add_input_event(INPUT_EVENT_PRESS_MENU, false);
      if ( isKeyBackPressed() )
         _add_input_event(INPUT_EVENT_PRESS_BACK, false);
      if ( isKeyMinusPressed() )
         _add_input_event(INPUT_EVENT_PRESS_MINUS, false);
      if ( isKeyPlusPressed() )
         _add_input_event(INPUT_EVENT_PRESS_PLUS, false);
      if ( isKeyQA1Pressed() )
         _add_input_event(INPUT_EVENT_PRESS_QA1, false);
      if ( isKeyQA2Pressed() )
         _add_input_event(INPUT_EVENT_PRESS_QA2, false);
      if ( isKeyQA3Pressed() )
         _add_input_event(INPUT_EVENT_PRESS_QA3, false);

      if ( isKeyMinusLongPressed() )
         _add_input_event(INPUT_EVENT_PRESS_MINUS, false);
      if ( isKeyPlusLongPressed() )
         _add_input_event(INPUT_EVENT_PRESS_PLUS, false);

      if ( isKeyMinusLongLongPressed() || isKeyPlusLongLongPressed() || isKeyMenuLongLongPressed() )
         s_bHasLongPressFlag = true;
      else
         s_bHasLongPressFlag = false;
      hardware_sleep_ms(10);

      if ( ! (*pbInitialized) )
         break;

      if ( g_TimeNow > s_uNextKeyboardDetectTime )
      {
         s_iKeyboardDetectTryCount++;
         if ( s_iKeyboardDetectTryCount < 3 )
            s_uNextKeyboardDetectTime = g_TimeNow + 1000 * s_iKeyboardDetectTryCount;
         else
            s_uNextKeyboardDetectTime = g_TimeNow + 3000;

         if ( _keyboard_try_detect() )
         {
            s_iKeyboardDetectTryCount = 0;
            s_uNextKeyboardDetectTime = g_TimeNow + 2000;
         }
      }

      if ( _read_keyboard_input_events() < 0 )
         s_uNextKeyboardDetectTime = g_TimeNow + 500;
   }

   log_line("[Keyboard] Thread stopped.");
   return NULL;
}

int keyboard_init()
{
   if ( s_bKeyboardInitDone )
      return 0;

   ControllerSettings* pCS = get_ControllerSettings();

   s_bKeyboardInitDone = true;
   s_uNextKeyboardDetectTime = g_TimeNow + 2000;
   s_iKeyboardDetectTryCount = 0;
   s_iCountKeyboardInputEvents = 0;

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      s_InputDevicesInfo[i].iFile = -1;
      s_InputDevicesInfo[i].szName[0] = 0;
      s_InputDevicesInfo[i].bIgnore = false;
   }

   if ( 0 != pthread_mutex_init(&s_pThreadKeyboardMutex, NULL) )
   {
      log_error_and_alarm("[Keyboard] Failed to init mutex.");
      return 0;
   }

   pthread_attr_t attr;
   int iPrio = pCS->iThreadPriorityCentral;
   if ( pCS->iPrioritiesAdjustment && (iPrio > 2) && (iPrio < 98) )
   {
      iPrio+=2;
      hw_init_worker_thread_attrs(&attr, (pCS->iCoresAdjustment?CORE_AFFINITY_CENTRAL_UI:-1), -1, SCHED_FIFO, iPrio, "keyboard");
   }
   else
      hw_init_worker_thread_attrs(&attr, (pCS->iCoresAdjustment?CORE_AFFINITY_CENTRAL_UI:-1), -1, SCHED_OTHER, 0, "keyboard");
   if ( 0 != pthread_create(&s_pThreadKeyboard, &attr, &_thread_keyboard, (void*)&s_bKeyboardInitDone) )
   {
      pthread_attr_destroy(&attr);
      log_error_and_alarm("[Keyboard] Failed to create thread.");
      return 0;
   }
   pthread_attr_destroy(&attr);
   return 1;
}

int keyboard_uninit()
{
   if ( ! s_bKeyboardInitDone )
      return 0;

   s_bKeyboardInitDone = false;
   
   pthread_mutex_lock(&s_pThreadKeyboardMutex);
   pthread_mutex_unlock(&s_pThreadKeyboardMutex);

   pthread_cancel(s_pThreadKeyboard);
   pthread_mutex_destroy(&s_pThreadKeyboardMutex);

   for( int i=0; i<MAX_INPUT_DEVICES; i++ )
   {
      if ( s_InputDevicesInfo[i].iFile >= 0 )
         close(s_InputDevicesInfo[i].iFile);

     s_InputDevicesInfo[i].iFile = -1;
     s_InputDevicesInfo[i].szName[0] = 0;
   }

   log_line("[Keyboard] Uninited.");
   return 0;
}

u32 keyboard_get_next_input_event()
{
   if ( s_iCountKeyboardInputEvents == 0 )
      return 0;

   u32 uReturn = 0;
   int iLock = pthread_mutex_lock(&s_pThreadKeyboardMutex);

   uReturn = s_uKeyboardInputEvents[0];
   for( int i=0; i<s_iCountKeyboardInputEvents-1; i++ )
      s_uKeyboardInputEvents[i] = s_uKeyboardInputEvents[i+1];
   s_iCountKeyboardInputEvents--;

   if ( 0 == iLock )
      pthread_mutex_unlock(&s_pThreadKeyboardMutex);

   return uReturn;
}

int keyboard_consume_input_events()
{
   s_uKeyboardInputEventsSum = 0;
   if ( 0 == s_iCountKeyboardInputEvents )
      return 0;

   int iCounter = 20;
   //int i = s_iCountKeyboardInputEvents;
   while ( iCounter > 0 )
   {
      if ( 0 == s_iCountKeyboardInputEvents )
         break;
      u32 uEvent = keyboard_get_next_input_event();
      s_uKeyboardInputEventsSum |= (uEvent & 0xFFFF);
      // If unknown key was pressed, it is stored in the 3rd byte
      if ( uEvent & 0xFF0000 )
      {
         s_uKeyboardInputEventsSum &= 0xFF00FFFF;
         s_uKeyboardInputEventsSum |= uEvent;
      }
      iCounter--;
   }
   //log_line("[Keyboard] Consumed %d events: %u", i, s_uKeyboardInputEventsSum);
   return 0;
}

int keyboard_has_long_press_flag()
{
   return s_bHasLongPressFlag?1:0;
}

u32 keyboard_get_triggered_input_events()
{
   return s_uKeyboardInputEventsSum;
}

void keyboard_clear_triggered_back_event()
{
   s_uKeyboardInputEventsSum &= ~INPUT_EVENT_PRESS_BACK; 
}

u32 keyboard_add_triggered_gpio_input_events()
{
   if ( isKeyMenuPressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_MENU;
   if ( isKeyBackPressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_BACK;
   if ( isKeyMinusPressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_MINUS;
   if ( isKeyPlusPressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_PLUS;
   if ( isKeyQA1Pressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_QA1;
   if ( isKeyQA2Pressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_QA2;
   if ( isKeyQA3Pressed() )
      s_uKeyboardInputEventsSum |= INPUT_EVENT_PRESS_QA3;

   return s_uKeyboardInputEventsSum;
}