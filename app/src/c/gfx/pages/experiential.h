#pragma once

#include <pebble.h>

#define NUM_EXPERIENTIAL_IMAGES 7 //this is in resources too :/

Layer* init_experiential_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image);
void draw_experiential(Layer* layer, GContext* ctx);

/**
 * @brief Sets the experiential view for the specified hour
 * 
 * @param hour Hour index (0-11) to display, or-1o disable the view
 */
void set_experiential_view(int hour);

/**
 * @brief Gets the emoji image for the current experiential view
 * 
 * @return Pointer to the emoji image, or NULL if not available
 */
GDrawCommandImage* get_experiential_emoji(void);