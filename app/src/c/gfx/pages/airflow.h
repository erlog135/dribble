#pragma once

#include <pebble.h>

Layer* init_airflow_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image);
void deinit_airflow_layers(void);
void draw_airflow(Layer* layer, GContext* ctx);

/**
 * @brief Activates/deactivates the airflow page and binds it to a forecast hour.
 *
 * @param hour Hour index (0-11) to display, or a negative value to disable the view.
 */
void set_airflow_view(int hour);

/**
 * @brief Resets the anemometer timeout timer.
 *
 * Call this function to restart the 1-minute timeout that stops the anemometer
 * animation due to inactivity.
 */
void reset_anemometer_timeout(void);
