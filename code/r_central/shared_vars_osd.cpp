#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware_radio.h"
#include "shared_vars.h"
#include "osd/osd_common.h"
#include <math.h>

float g_fOSDDbm[MAX_RADIO_INTERFACES];
float g_fOSDSNR[MAX_RADIO_INTERFACES];
u32   g_uOSDDbmLastCaptureTime[MAX_RADIO_INTERFACES];


void shared_vars_osd_reset_before_pairing()
{
   for( int i=0; i<MAX_RADIO_INTERFACES; i++ )
   {
      g_fOSDDbm[i] = -200.0f;
      g_fOSDSNR[i] = 0.0;
      g_uOSDDbmLastCaptureTime[i] = 0;
   }
}

void shared_vars_osd_update()
{
   Model* pActiveModel = osd_get_current_data_source_vehicle_model();

   for ( int i=0; i<hardware_get_radio_interfaces_count(); i++ )
   {
      radio_hw_info_t* pRadioHWInfo = hardware_get_radio_info(i);
      if ( NULL == pRadioHWInfo )
         continue;
      if ( ! pRadioHWInfo->isHighCapacityInterface )
         continue;
      
      int iRadioDBM = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMVideoForInterface;
      int iSNR = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRVideoForInterface;
      u32 uTimeUpdate = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeVideo;
      
      if ( (NULL == pActiveModel) || (! pActiveModel->hasCamera()) )
      {
         iRadioDBM = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxDBMDataForInterface;
         iSNR = g_SMControllerRTInfo.radioInterfacesSignals[i].iMaxSNRDataForInterface;
         uTimeUpdate = g_SMControllerRTInfo.radioInterfacesSignals[i].uLastUpdateTimeData;
      }

      if ( (iRadioDBM > -500) && (iRadioDBM < 500) )
      {
         if ( 0 == g_uOSDDbmLastCaptureTime[i] )
            g_fOSDDbm[i] = iRadioDBM;
         else
            g_fOSDDbm[i] = (g_fOSDDbm[i]*5 + iRadioDBM)/6;
      }

      if ( (iSNR > -500) && (iSNR < 500) )
      {
         if ( 0 == g_uOSDDbmLastCaptureTime[i] )
            g_fOSDSNR[i] = iSNR;
         else
            g_fOSDSNR[i] = (g_fOSDSNR[i]*5 + iSNR)/6;
      }

      g_uOSDDbmLastCaptureTime[i] = uTimeUpdate;
   }
}