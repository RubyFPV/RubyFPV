#pragma once

bool osd_is_debug();
float osd_show_home(float xPos, float yPos, bool showHeading, float fScale);
float osd_render_radio_link_tag(float xPos, float yPos, int iRadioLink, bool bVehicle, bool bDraw);

void osd_render_msposd_buffer(int iFCType, int iOSDFontType, int iCols, int iRows, u16* pCharBuffer);

void osd_disable_rendering();
void osd_enable_rendering();
void osd_render_all();

void osd_add_stats_flight_end();
void osd_remove_stats_flight_end();
bool osd_is_stats_flight_end_on();
void osd_add_stats_total_flights();

void osd_start_flash_osd_elements();
