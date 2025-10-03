#pragma once

#include <pebble.h>

// Color functions for weather conditions, airflow, and experiential states
GColor get_condition_color(int condition_code);
GColor get_airflow_color(int airflow_intensity);
GColor get_experiential_color(int experiential_index);

GDrawCommandImage** init_25px_condition_images();
GDrawCommandImage** init_50px_condition_images();
void deinit_25px_condition_images(GDrawCommandImage** condition_images_25px);
void deinit_50px_condition_images(GDrawCommandImage** condition_images_50px);

// Axis image functions
GDrawCommandImage* init_axis_small_image();
GDrawCommandImage* init_axis_large_image();
void deinit_axis_image(GDrawCommandImage* axis_image);

// Airflow image functions
GDrawCommandImage* init_wind_vane_image();
GDrawCommandImage* init_wind_vane_angle_image();
GDrawCommandImage** init_wind_speed_images();  // Returns array of 6 images: 3 regular + 3 angled
void deinit_wind_speed_images(GDrawCommandImage** wind_speed_images);

// Experiential image functions
GDrawCommandImage** init_25px_experiential_images();
GDrawCommandImage** init_50px_experiential_images();
GDrawCommandImage** init_emoji_images();
void deinit_25px_experiential_images(GDrawCommandImage** experiential_images_25px);
void deinit_50px_experiential_images(GDrawCommandImage** experiential_images_50px);
void deinit_emoji_images(GDrawCommandImage** emoji_images);