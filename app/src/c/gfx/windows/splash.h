#pragma once

#include <pebble.h>

/**
 * @brief Creates and returns the splash screen window
 * 
 * The splash window displays during app startup and handles initial weather data loading.
 * It shows a loading image and status text, then transitions to the main viewer on success
 * or displays an error message on failure. It will also handle different launch conditions,
 * to server as either a one(/zero)-click update or an ordinary launch.
 * 
 * @return Window* The created splash window
 */
Window* splash_window_create(void);

/**
 * @brief Destroys the splash window and cleans up resources
 * 
 * @param window The splash window to destroy
 */
void splash_window_destroy(Window* window);

/**
 * @brief Updates the status text displayed on the splash screen
 * 
 * @param status_text Text to display (e.g. "Loading weather...", "Error: No connection")
 */
void splash_set_status_text(const char* status_text);

/**
 * @brief Sets the splash image to display
 * 
 * @param image GDrawCommandImage to display, or NULL to hide image
 */
void splash_set_image(GDrawCommandImage* image);

/**
 * @brief Callback function type for splash completion
 * 
 * This callback is called when the splash screen has finished loading data
 * and is ready to transition to the main viewer.
 * 
 * @param success True if data loading was successful, false if there was an error
 */
typedef void (*SplashCompletionCallback)(bool success);

/**
 * @brief Sets the completion callback for the splash screen
 * 
 * @param callback Function to call when loading is complete
 */
void splash_set_completion_callback(SplashCompletionCallback callback);

/**
 * @brief Starts the weather data loading process
 * 
 * This will initiate the data request and update the status text accordingly.
 * The completion callback will be called when finished.
 */
void splash_start_loading(void);

/**
 * @brief Handles incoming app messages during loading
 * 
 * This function should be called by main.c when inbox messages are received
 * while the splash screen is active.
 * 
 * @param iter Dictionary iterator from the inbox message
 */
void splash_handle_inbox_message(DictionaryIterator *iter);
