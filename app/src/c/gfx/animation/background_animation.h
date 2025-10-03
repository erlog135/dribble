#pragma once
#include <pebble.h>
#include "animation.h"

// Background animation direction for different transition types
typedef enum {
    BACKGROUND_ANIMATION_FROM_LEFT,   // Slide in from left
    BACKGROUND_ANIMATION_FROM_RIGHT,  // Slide in from right
    BACKGROUND_ANIMATION_FROM_TOP,    // Slide in from top
    BACKGROUND_ANIMATION_FROM_BOTTOM  // Slide in from bottom
} BackgroundAnimationDirection;

/**
 * Initialize the background animation subsystem
 */
void background_animation_init_system(void);

/**
 * Initialize the background animation system with a parent layer
 * @param parent_layer The parent layer to add animation layers to
 * @param window The window whose background color will be changed
 */
void background_animation_init(Layer* parent_layer, Window* window);

/**
 * Clean up the background animation system
 */
void background_animation_deinit(void);

/**
 * Clean up the background animation subsystem
 */
void background_animation_deinit_system(void);

/**
 * Start a background transition animation
 * @param direction The direction of the animation (horizontal/vertical)
 * @param color The color of the expanding rectangle and final background
 * @param on_complete Callback function to call when animation completes
 */
void background_animation_start(BackgroundAnimationDirection direction, 
                               GColor color, 
                               void (*on_complete)(void));

/**
 * Check if a background animation is currently active
 * @return true if animation is active, false otherwise
 */
bool background_animation_is_active(void);

/**
 * Stop any active background animation
 */
void background_animation_stop(void);
