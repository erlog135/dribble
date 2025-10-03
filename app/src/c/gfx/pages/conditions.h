#pragma once

#include <pebble.h>

Layer* init_conditions_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image);
void draw_conditions(Layer* layer, GContext* ctx);

/**
 * @brief Sets the conditions view for the specified hour
 * 
 * @param hour Hour index (0-11) to display, or -1 to disable the view
 */
void set_conditions_view(int hour);

/**
 * @brief Deinitializes the conditions layers and frees resources
 */
void deinit_conditions_layers(void); 