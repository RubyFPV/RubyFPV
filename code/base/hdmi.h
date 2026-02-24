#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int hdmi_enum_modes();

int hdmi_load_current_mode();

int hdmi_get_resolutions_count();

int hdmi_get_current_resolution_index();
int hdmi_get_current_resolution_group();
int hdmi_get_current_resolution_mode();
int hdmi_get_current_resolution_width();
int hdmi_get_current_resolution_height();
int hdmi_get_current_resolution_refresh();
int hdmi_get_current_resolution_refresh_count();
int hdmi_get_current_resolution_refresh_index();

int hdmi_get_resolution_width(int index);
int hdmi_get_resolution_height(int index);
int hdmi_get_resolution_refresh_count(int index);
int hdmi_get_resolution_refresh_rate(int index, int index2);

int hdmi_set_current_resolution(int width, int height, int refresh);

int hdmi_get_best_resolution_index_for(int iWidth, int iHeight, int iRefresh);

#ifdef __cplusplus
}
#endif
