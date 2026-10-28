#pragma once

#include <pebble.h>

// Color functions for weather conditions, airflow, and experiential states
GColor get_condition_color(int condition_code);
GColor get_airflow_color(int airflow_intensity);
GColor get_experiential_color(int experiential_index);

// PDC icons are loaded on demand for only the visible prev/current/next slots
// and destroyed when no longer shown. The caller owns every returned image and
// must destroy it with gdraw_command_image_destroy().
GDrawCommandImage* load_condition_image(uint8_t condition_code, bool large);
GDrawCommandImage* load_experiential_image(uint8_t icon_1_based, bool large);
GDrawCommandImage* load_wind_vane_image(int8_t direction);
GDrawCommandImage* load_wind_speed_image(uint32_t resource_id);
GDrawCommandImage* load_axis_image(bool large);

// The emoji is chosen once per app launch; each call loads that same emoji.
GDrawCommandImage* load_random_emoji_image(void);