#pragma once
#ifdef __cplusplus
extern "C" {
#endif

float plugin_osd_convertKm(float km, int iTargetMeasureUnit);
float plugin_osd_convertMeters(float m, int iTargetMeasureUnit);
float plugin_osd_convertTemperature(float c, int iTargetMeasureUnit);

#ifdef __cplusplus
}
#endif
