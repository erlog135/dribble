#pragma once

#include <pebble.h>

Layer* init_airflow_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image);
void draw_airflow(Layer* layer, GContext* ctx);

/**
 * @brief Updates the wind vane image to point in the specified direction
 * 
 * @param direction Wind direction (0-7):
 *                 0: Right (0°)
 *                 1: Bottom-right (45°)
 *                 2: Down (90°)
 *                 3: Bottom-left (135°)
 *                 4: Left (180°)
 *                 5: Top-left (225°)
 *                 6: Up (270°)
 *                 7: Top-right (315°)
 */
void update_wind_vane_direction(uint8_t direction);

void set_airflow_view(int hour);

/**
 * @brief Resets the anemometer timeout timer
 * 
 * Call this function to restart the 1-minute timeout for stopping
 * the anemometer animation due to inactivity.
 */
void reset_anemometer_timeout(void);

/**
 * @brief Updates the image references for the specified hour
 * 
 * @param hour Hour index (0-11) to display
 * @param prev_image Reference to store the previous hour's image
 * @param current_image Reference to store the current hour's image  
 * @param next_image Reference to store the next hour's image
 */
void update_airflow_images(int hour, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image); 