#pragma once

#include <pebble.h>

Layer* init_conditions_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image);
void draw_conditions(Layer* layer, GContext* ctx);

/**
 * @brief Sets the conditions view for the specified hour
 *
 * @param hour Hour index (0-11) to display, or a negative value to disable the view.
 */
void set_conditions_view(int hour);

/**
 * @brief Deinitializes the conditions layers and frees resources.
 */
void deinit_conditions_layers(void);

/**
 * @brief Accessors for the axis images used in the precipitation view.
 *
 * The viewer uses these to identify which draw position to use for the image
 * currently occupying a slot, without re-deriving the (page, hour, precipitation)
 * condition that conditions.c already evaluated.
 */
GDrawCommandImage* conditions_get_axis_small_image(void);
GDrawCommandImage* conditions_get_axis_large_image(void);
