#pragma once

#include <pebble.h>

/**
 * @brief Creates and returns the forecast viewer window
 * 
 * This window handles the forecast display interface including:
 * - Time navigation (prev/current/next hour display)
 * - Page switching between conditions/airflow/experiential views
 * - Animation support for smooth transitions
 * - Image and text layer management
 * 
 * @return Window* The created viewer window
 */
Window* viewer_window_create(void);

/**
 * @brief Destroys the viewer window and cleans up resources
 * 
 * @param window The viewer window to destroy
 */
void viewer_window_destroy(Window* window);

/**
 * @brief Updates the forecast view for the given hour and page
 * 
 * This function is called to refresh the display when forecast data changes
 * or when navigation occurs. It updates both the displayed content and images
 * based on the current hour and page selection.
 * 
 * @param hour Hour index (0-11) to display
 * @param page Page type (0=conditions, 1=airflow, 2=experiential)
 */
void viewer_update_view(uint8_t hour, uint8_t page);

/**
 * @brief Gets the current hour being viewed
 * 
 * @return uint8_t Current hour index (0-11)
 */
uint8_t viewer_get_current_hour(void);

/**
 * @brief Gets the current page being viewed
 * 
 * @return uint8_t Current page (0=conditions, 1=airflow, 2=experiential)
 */
uint8_t viewer_get_current_page(void);

/**
 * @brief Sets the current hour and page for viewing
 * 
 * This function updates the internal state and refreshes the display
 * 
 * @param hour Hour index (0-11) to set
 * @param page Page type to set (0=conditions, 1=airflow, 2=experiential)
 */
void viewer_set_current_view(uint8_t hour, uint8_t page);
