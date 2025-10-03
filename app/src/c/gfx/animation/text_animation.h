#pragma once
#include <pebble.h>
#include "animation.h"

typedef struct {
    AnimationState state;
    AnimationDirection direction;
    
    // Main layers (not owned by animation system)
    TextLayer* main_time_layer;
    TextLayer* main_text_layer;
    TextLayer* prev_time_layer;
    TextLayer* next_time_layer;
    
    // Temporary layers for animation (owned by animation system)
    Layer* animation_layer;
    TextLayer* temp_incoming_time_layer;
    
    // Animation objects
    Animation* spawn_animation;
    PropertyAnimation* incoming_time_animation;
    PropertyAnimation* prev_time_animation;
    PropertyAnimation* current_time_animation;
    PropertyAnimation* next_time_animation;
    PropertyAnimation* current_text_animation;
    
    // Completion callback
    void (*on_complete)(void);
} TextAnimationContext;

/**
 * Initialize the text animation subsystem
 */
void text_animation_init_system(void);

/**
 * Initialize the text animation system with a parent layer
 * @param parent_layer The parent layer to add animation layers to
 */
void text_animation_init(Layer* parent_layer);

/**
 * Set the main text layers that will be animated
 * @param current_time_layer The main time layer
 * @param current_text_layer The main text layer
 */
void text_animation_set_main_layers(TextLayer* current_time_layer, TextLayer* current_text_layer);

/**
 * Set the secondary time layers (prev/next) that will be animated
 * @param prev_time_layer The previous time layer
 * @param next_time_layer The next time layer
 */
void text_animation_set_secondary_layers(TextLayer* prev_time_layer, TextLayer* next_time_layer);

/**
 * Set the image references for animation support (forwards to image animation system)
 * @param images_layer The layer that draws the original images
 * @param prev_image_ref Reference to the prev image pointer
 * @param current_image_ref Reference to the current image pointer
 * @param next_image_ref Reference to the next image pointer
 */
void text_animation_set_image_layers(Layer* images_layer, 
                                   GDrawCommandImage** prev_image_ref,
                                   GDrawCommandImage** current_image_ref, 
                                   GDrawCommandImage** next_image_ref);

/**
 * Clean up the text animation system
 */
void text_animation_deinit(void);

/**
 * Clean up the text animation subsystem
 */
void text_animation_deinit_system(void);

/**
 * Start a text animation
 * @param direction The direction of animation (up or down)
 * @param target_hour The target hour index (0-11) after animation completes
 * @param time_text The new time text to display
 * @param content_text The new content text to display
 * @param on_complete Callback function to call when animation completes
 */
void text_animation_start(AnimationDirection direction, uint8_t target_hour, const char* time_text, const char* content_text, void (*on_complete)(void));

/**
 * Check if an animation is currently active
 * @return true if animation is active, false otherwise
 */
bool text_animation_is_active(void);

/**
 * Stop any active animation
 */
void text_animation_stop(void); 