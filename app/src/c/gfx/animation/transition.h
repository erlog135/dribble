#pragma once
#include <pebble.h>
#include "animation.h"

// Logging is now controlled by ANIMATION_LOGGING in animation.h
// Uncomment ANIMATION_LOGGING in animation.h to enable animation debug logging

// Transition animation timing definitions
#define TRANSITION_ANIMATION_DURATION_MS 150
#define TRANSITION_IMAGE_ANIMATION_DURATION_MS 200
#define TRANSITION_IMAGE_TEXT_DELAY_MS 100

typedef struct {
    AnimationState state;
    
    // Text layers to animate (not owned by transition system)
    TextLayer* current_time_layer;
    TextLayer* current_text_layer;
    TextLayer* prev_time_layer;
    TextLayer* next_time_layer;
    
    // Image references (not owned by transition system)
    Layer* images_layer;
    GDrawCommandImage** prev_image_ref;
    GDrawCommandImage** current_image_ref;
    GDrawCommandImage** next_image_ref;
    
    // Animation objects
    Animation* spawn_animation;
    PropertyAnimation* current_time_animation;
    PropertyAnimation* current_text_animation;
    PropertyAnimation* prev_time_animation;
    PropertyAnimation* next_time_animation;
    PropertyAnimation* image_progress_animation;
    
    // Timers for animation coordination
    AppTimer* text_animation_delay_timer;
    
    // Completion callback
    void (*on_complete)(void);
} TransitionAnimationContext;

/**
 * Initialize the transition animation subsystem
 */
void transition_animation_init_system(void);

/**
 * Initialize the transition animation system with a parent layer
 * @param parent_layer The parent layer to add animation layers to
 */
void transition_animation_init(Layer* parent_layer);

/**
 * Set the text layers that will be animated during transitions
 * @param current_time_layer The main time layer
 * @param current_text_layer The main text layer
 * @param prev_time_layer The previous time layer
 * @param next_time_layer The next time layer
 */
void transition_animation_set_layers(TextLayer* current_time_layer, 
                                   TextLayer* current_text_layer,
                                   TextLayer* prev_time_layer, 
                                   TextLayer* next_time_layer);

/**
 * Set the image layers that will be animated during transitions
 * @param images_layer The layer that draws the images
 * @param prev_image_ref Reference to the prev image pointer
 * @param current_image_ref Reference to the current image pointer
 * @param next_image_ref Reference to the next image pointer
 */
void transition_animation_set_image_layers(Layer* images_layer, 
                                         GDrawCommandImage** prev_image_ref,
                                         GDrawCommandImage** current_image_ref, 
                                         GDrawCommandImage** next_image_ref);

/**
 * Clean up the transition animation system
 */
void transition_animation_deinit(void);

/**
 * Clean up the transition animation subsystem
 */
void transition_animation_deinit_system(void);

/**
 * Start a page transition animation
 * @param on_complete Callback function to call when animation completes
 */
void transition_animation_start(void (*on_complete)(void));

/**
 * Check if a transition animation is currently active
 * @return true if animation is active, false otherwise
 */
bool transition_animation_is_active(void);

/**
 * Stop any active transition animation
 */
void transition_animation_stop(void);

/**
 * Check if transition animation is active and get current image positions
 * @param prev_pos Output parameter for prev image position
 * @param current_pos Output parameter for current image position  
 * @param next_pos Output parameter for next image position
 * @return true if animation is active, false if normal drawing should be used
 */
bool transition_animation_get_image_positions(GPoint* prev_pos, GPoint* current_pos, GPoint* next_pos);