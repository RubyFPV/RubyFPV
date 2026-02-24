#include "osd_plugins_utils.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>


#ifdef __cplusplus
extern "C" {
#endif


float plugin_osd_convertKm(float km, int iTargetMeasureUnit)
{
   if ( iTargetMeasureUnit == 0 || iTargetMeasureUnit == 2 )
      return km;
   if ( iTargetMeasureUnit == 1 || iTargetMeasureUnit == 3 )
      return km*0.621371;
   return km;
}


float plugin_osd_convertMeters(float m, int iTargetMeasureUnit)
{
   if ( iTargetMeasureUnit == 0 || iTargetMeasureUnit == 2 )
      return m;
   if ( iTargetMeasureUnit == 1 || iTargetMeasureUnit == 3 )
      return m*3.28084;
   return m;
}

float plugin_osd_convertTemperature(float c, int iTargetMeasureUnit)
{
   if ( iTargetMeasureUnit == 0 || iTargetMeasureUnit == 2 )
      return c;
   if ( iTargetMeasureUnit == 1 || iTargetMeasureUnit == 3 )
      return c*1.8 + 32.0;
   return c;
}


#ifdef __cplusplus
}
#endif

