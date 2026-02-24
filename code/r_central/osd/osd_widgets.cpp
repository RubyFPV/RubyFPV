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

#include "osd_widgets.h"
#include "osd_common.h"
#include "osd_widgets_builtin.h"

#include "../../base/config.h"
#include "../../base/models_list.h"
#include "../colors.h"
#include "../../renderer/render_engine.h"
#include "../shared_vars.h"
#include "../shared_vars_osd.h"
#include "../shared_vars_state.h"

type_osd_widget s_ListOSDWidgets[MAX_OSD_WIDGETS];
int s_iListOSDWidgetsCount = 0;

int osd_widgets_get_count()
{
   return s_iListOSDWidgetsCount;
}

type_osd_widget* osd_widgets_get(int iIndex)
{
   if ( (iIndex < 0) || (iIndex >= s_iListOSDWidgetsCount) )
      return NULL;
   return &(s_ListOSDWidgets[iIndex]);
}

void _osd_widgets_add(u32 uGUID, int iVersion, const char* szName)
{
   if ( s_iListOSDWidgetsCount >= MAX_OSD_WIDGETS )
      return;

   // Don't add duplicate widgets
   for( int i=0; i<s_iListOSDWidgetsCount; i++ )
      if ( s_ListOSDWidgets[i].info.uGUID == uGUID )
         return;

   s_ListOSDWidgets[s_iListOSDWidgetsCount].info.uGUID = uGUID;
   s_ListOSDWidgets[s_iListOSDWidgetsCount].info.iVersion = iVersion;
   strcpy(s_ListOSDWidgets[s_iListOSDWidgetsCount].info.szName, szName);

   for( int i=0; i<MAX_MODELS; i++ )
   for( int k=0; k<MODEL_MAX_OSD_SCREENS; k++ )
   {
      memset(&(s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k]), 0, sizeof(type_osd_widget_display_info));
      s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k].bShow = false;
      s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k].fXPos = 0.7;
      s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k].fYPos = 0.2;
      s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k].fWidth = 0;
      s_ListOSDWidgets[s_iListOSDWidgetsCount].display_info[i][k].fHeight = 0;
   }
   s_iListOSDWidgetsCount++;
}

bool osd_widgets_load()
{
   s_iListOSDWidgetsCount = 0;
   bool bFailed = false;

   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_OSD_WIDGETS);

   if ( access(szFile, R_OK) == -1 )
   {
      log_line("No OSD widgets configuration file present (%s). Skipping OSD widgets.", szFile);
      return false;
   }

   FILE* fd = fopen(szFile, "rb");
   if ( NULL == fd )
   {
      log_softerror_and_alarm("Failed to load OSD widgets configuration file.");
      bFailed = true;
      //_osd_widgets_add(OSD_WIDGET_ID_BUILTIN_ALTITUDE, 1, "Altitude Gauge");
      return false;
   }


   if ( 1 != fscanf(fd, "%*s %d", &s_iListOSDWidgetsCount) )
   { bFailed = true;   s_iListOSDWidgetsCount = 0; }


   for( int i=0; i<s_iListOSDWidgetsCount; i++ )
   {
      if ( bFailed )
         break;
      if ( 3 != fscanf(fd, "%u %d %s", &s_ListOSDWidgets[i].info.uGUID, &s_ListOSDWidgets[i].info.iVersion, s_ListOSDWidgets[i].info.szName) )
         bFailed = true;

      if ( bFailed )
         break;

      for( int k=0; k<(int)strlen(s_ListOSDWidgets[i].info.szName); k++ )
         if ( s_ListOSDWidgets[i].info.szName[k] == '*' )
            s_ListOSDWidgets[i].info.szName[k] = ' ';

      int iCountModels = 0;
      int iCountProfiles = 0;
      if ( 2 != fscanf( fd, "%*s %d %*s %d", &iCountModels, &iCountProfiles) )
         bFailed = true;
      if ( (iCountModels < 0) || (iCountProfiles < 0) )
         bFailed = true;
      if ( bFailed )
         break;

      for( int iModel=0; iModel<iCountModels; iModel++ )
      for( int iProfile=0; iProfile<iCountProfiles; iProfile++)
      {
         if ( bFailed )
            break;
         int iIndex = iModel;
         if ( iIndex >= MAX_MODELS )
            iIndex = MAX_MODELS-1;

         int iOSDProfile = iProfile;
         if ( iOSDProfile >= MODEL_MAX_OSD_SCREENS )
            iOSDProfile = MODEL_MAX_OSD_SCREENS-1;

         int tmp = 0;
         if ( 6 != fscanf(fd, "%u %d %f %f %f %f\n",
                &s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].uVehicleId,
                &tmp,
                &s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].fXPos,
                &s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].fYPos,
                &s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].fWidth,
                &s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].fHeight ) )
            bFailed = true;

         if ( bFailed )
            break;

         s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].bShow = tmp?true:false;

         int iCountParams = 0;
         if ( 1 != fscanf( fd, "%*s %d", &iCountParams) )
            bFailed = true;
         if ( iCountParams < 0 )
            bFailed = true;
         if ( bFailed )
            break;

         for ( int k=0; k<iCountParams; k++ )
         {
            int tmp2 = 0;
            if ( 1 != fscanf(fd, "%d", &tmp2) )
               bFailed = true;

            if (bFailed )
               break;

            if ( k < MAX_OSD_WIDGET_PARAMS )
            {
               s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].iParams[k] = tmp2;
               s_ListOSDWidgets[i].display_info[iIndex][iOSDProfile].iParamsCount = k;
            }
         }
      }
      if ( ! bFailed )
         log_line("Loaded configuration for OSD widget: [%s], id: %u, ver: %d, models: %d, params: %d",
          s_ListOSDWidgets[i].info.szName, s_ListOSDWidgets[i].info.uGUID, s_ListOSDWidgets[i].info.iVersion, iCountModels, s_ListOSDWidgets[i].display_info[0][0].iParamsCount);
   }

   fclose(fd);

   //_osd_widgets_add(OSD_WIDGET_ID_BUILTIN_ALTITUDE, 1, "Altitude Gauge");

   if ( bFailed )
      log_softerror_and_alarm("Failed to load OSD widgets configuration file.");
   else
      log_line("Loaded configuration for %d OSD widgets.", s_iListOSDWidgetsCount);

   return (!bFailed);
}

bool osd_widgets_save()
{
   char szFile[128];
   strcpy(szFile, FOLDER_CONFIG);
   strcat(szFile, FILE_CONFIG_CONTROLLER_OSD_WIDGETS);
   FILE* fd = fopen(szFile, "wb");
   if ( NULL == fd )
      return false;

   fprintf(fd, "count: %d\n", s_iListOSDWidgetsCount);
   for( int i=0; i<s_iListOSDWidgetsCount; i++ )
   {
      char szBuff[128];
      strcpy(szBuff, s_ListOSDWidgets[i].info.szName);
      for( int k=0; k<(int)strlen(szBuff); k++ )
         if ( szBuff[k] == ' ' )
            szBuff[k] = '*';
      fprintf(fd, "%u %d %s\n", s_ListOSDWidgets[i].info.uGUID, s_ListOSDWidgets[i].info.iVersion, szBuff);

      int iCountModels = 0;
      for( int k=0; k<MAX_MODELS; k++ )
      {
         if ( s_ListOSDWidgets[i].display_info[k][0].uVehicleId != 0 )
         if ( s_ListOSDWidgets[i].display_info[k][0].uVehicleId != MAX_U32 )
            iCountModels++;
      }
      fprintf(fd, "   models: %d profiles: %d\n", iCountModels, MODEL_MAX_OSD_SCREENS);
      for( int k=0; k<MAX_MODELS; k++ )
      {
         if ( (s_ListOSDWidgets[i].display_info[k][0].uVehicleId == 0) || (s_ListOSDWidgets[i].display_info[k][0].uVehicleId == MAX_U32) )
             continue;
         for( int j=0; j<MODEL_MAX_OSD_SCREENS; j++ )
         {
             fprintf(fd, "      %u %d %.2f %.2f %.2f %.2f\n",
                s_ListOSDWidgets[i].display_info[k][j].uVehicleId,
                (int) s_ListOSDWidgets[i].display_info[k][j].bShow,
                s_ListOSDWidgets[i].display_info[k][j].fXPos,
                s_ListOSDWidgets[i].display_info[k][j].fYPos,
                s_ListOSDWidgets[i].display_info[k][j].fWidth,
                s_ListOSDWidgets[i].display_info[k][j].fHeight );

             fprintf(fd, "      params %d ", s_ListOSDWidgets[i].display_info[k][j].iParamsCount);
             for( int iParam=0; iParam < s_ListOSDWidgets[i].display_info[k][j].iParamsCount; iParam ++ )
                fprintf(fd, "%d ", s_ListOSDWidgets[i].display_info[k][j].iParams[iParam]);
             fprintf(fd, "\n");
         }
      }
   }
   fclose(fd);
   return true;
}

void _osd_render_widget(int iIndex, int iModelIndex, u32 uModelId, int iOSDScreen)
{
   if ( (iIndex < 0) || (iIndex >= s_iListOSDWidgetsCount) )
      return;
   if ( (iModelIndex < 0) || (iModelIndex >= MAX_MODELS) )
      return;

   if ( s_ListOSDWidgets[iIndex].info.uGUID == OSD_WIDGET_ID_BUILTIN_ALTITUDE )
      osd_widget_builtin_altitude_render(&s_ListOSDWidgets[iIndex], &(s_ListOSDWidgets[iIndex].display_info[iModelIndex][iOSDScreen]), uModelId, iOSDScreen);
}

int osd_widget_add_to_model(type_osd_widget* pWidget, u32 uVehicleId)
{
   if ( (NULL == pWidget) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return -1;

   int iIndexFree = -1;
   for( int i=0; i<MAX_MODELS; i++ )
   {
       if ( pWidget->display_info[i][0].uVehicleId == uVehicleId )
          return i;
       if ( pWidget->display_info[i][0].uVehicleId == 0 )
       {
          iIndexFree = i;
          break;
       }
   }

   // No more room. Remove invalid models from list
   if ( iIndexFree == -1 )
   {
      for( int i=0; i<MAX_MODELS; i++ )
      {
         bool bValidModel = false;
         for( int k=0; k<getControllerModelsCount(); k++ )
         {
            Model *p = getModelAtIndex(k);
            if ( NULL == p )
               continue;

            if ( p->uVehicleId == pWidget->display_info[i][0].uVehicleId )
            {
               bValidModel = true;
               break;
            }
         }
         if ( ! bValidModel )
         {
             for( int k=0; k<MODEL_MAX_OSD_SCREENS; k++ )
                pWidget->display_info[i][k].uVehicleId = 0;
             iIndexFree = i;
             break;
         }
      }
   }

   if ( -1 == iIndexFree )
      return -1;

   for( int k=0; k<MODEL_MAX_OSD_SCREENS; k++ )
      pWidget->display_info[iIndexFree][k].uVehicleId = uVehicleId;

   return iIndexFree;
}

int osd_widget_get_model_index(type_osd_widget* pWidget, u32 uVehicleId)
{
   if ( (NULL == pWidget) || (0 == uVehicleId) || (MAX_U32 == uVehicleId) )
      return -1;

   for( int i=0; i<MAX_MODELS; i++ )
   {
       if ( pWidget->display_info[i][0].uVehicleId == uVehicleId )
          return i;
       if ( pWidget->display_info[i][0].uVehicleId == 0 )
          return -1;
   }
   return -1;
}

void osd_widgets_on_new_vehicle_added_to_controller(u32 uVehicleId)
{
   for( int i=0; i<s_iListOSDWidgetsCount; i++ )
   {
      if ( s_ListOSDWidgets[i].info.uGUID == OSD_WIDGET_ID_BUILTIN_ALTITUDE )
         osd_widget_builtin_altitude_on_new_vehicle(&s_ListOSDWidgets[i], uVehicleId);
   }

}

void osd_widgets_on_main_vehicle_changed(u32 uCurrentVehicleId)
{
   for( int i=0; i<s_iListOSDWidgetsCount; i++ )
   {
      if ( s_ListOSDWidgets[i].info.uGUID == OSD_WIDGET_ID_BUILTIN_ALTITUDE )
         osd_widget_builtin_altitude_on_main_vehicle_changed(&s_ListOSDWidgets[i], uCurrentVehicleId);
   }
}

void osd_widgets_render(u32 uCurrentVehicleId, int iOSDScreen)
{
   if ( (0 == uCurrentVehicleId) || (MAX_U32 == uCurrentVehicleId) )
      return;
   if ( (NULL == g_pCurrentModel) || (iOSDScreen < 0) || (iOSDScreen >= MODEL_MAX_OSD_SCREENS) )
      return;

   for( int iWidget=0; iWidget<s_iListOSDWidgetsCount; iWidget++ )
   {
      for( int iModel=0; iModel<MAX_MODELS; iModel++ )
      {
         if ( (s_ListOSDWidgets[iWidget].display_info[iModel][0].uVehicleId == 0) || (s_ListOSDWidgets[iWidget].display_info[iModel][0].uVehicleId == MAX_U32) )
            break;

         if ( s_ListOSDWidgets[iWidget].display_info[iModel][iOSDScreen].uVehicleId == uCurrentVehicleId )
         if ( s_ListOSDWidgets[iWidget].display_info[iModel][iOSDScreen].bShow )
         {
            _osd_render_widget(iWidget, iModel, uCurrentVehicleId, iOSDScreen);
         }
      }
   }
}
