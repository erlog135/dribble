#pragma once
#include <pebble.h>
#include "animation.h"
#include "../kimaybe/kimaybe.h"
#include "../kimaybe/transform.h"

/**
 * since the25s don't correspond exactly with their 50x locations,
 * an offset should be applied during a KMAnimation on the 50px image destination
 * to somewhat cleanly transition one to the other.
 * 
 * when going back from 5025x, these should be halved and negated
 * when applied to the 25px destination position to counteract them
 * already being offset.
 */
extern GPoint experiential_image_offsets[];

typedef struct {
    AnimationState state;
    AnimationDirection direction;
    
    // Current page and hour tracking
    uint8_t current_page;                   // Current page (0=conditions, 1=airflow, 2=experiential)
    uint8_t current_hour;                   // Current hour (0-11)
    
    // Image layer references
    Layer* images_layer;                    // Reference to the images layer for hiding during animation
    GDrawCommandImage** prev_image_ref;     // Reference to prev image pointer
    GDrawCommandImage** current_image_ref;  // Reference to current image pointer
    GDrawCommandImage** next_image_ref;     // Reference to next image pointer
    
    // Stored original image values for restoration
    GDrawCommandImage* stored_prev_image;   // Original prev image value
    GDrawCommandImage* stored_current_image; // Original current image value  
    GDrawCommandImage* stored_next_image;   // Original next image value
    
    // Progressive image visibility flags
    bool show_prev_ready;                   // Whether prev image should be shown
    bool show_current_ready;                // Whether current image should be shown
    bool show_next_ready;                   // Whether next image should be shown
    
    // KMAnimation objects for image transformations
    Layer* progressive_image_layer;         // Layer for progressive image showing
    Layer* km_animation_layer_1;            // Layer for first KM animation
    Layer* km_animation_layer_2;            // Layer for second KM animation
    KMAnimation* km_animation_1;            // First image animation
    KMAnimation* km_animation_2;            // Second image animation
    GDrawCommandImage* km_temp_image_1;     // Temporary copy for first animation
    GDrawCommandImage* km_temp_image_2;     // Temporary copy for second animation
    bool images_hidden;                     // Flag to track if original images are hidden
    int km_animations_completed;            // Counter for completed KM animations
    int km_animations_expected;             // Expected number of animations to complete
    AppTimer* km_animation_delay_timer;     // Timer for staggered animation start
    
    // Completion callback
    void (*on_complete)(void);
} ImageAnimationContext;

/**
 * Initialize the image animation subsystem
 */
void image_animation_init_system(void);

/**
 * Initialize the image animation system with a parent layer
 * @param parent_layer The parent layer to add animation layers to
 */
void image_animation_init(Layer* parent_layer);

/**
 * Set the image references for KMAnimation support
 * @param images_layer The layer that draws the original images
 * @param prev_image_ref Reference to the prev image pointer
 * @param current_image_ref Reference to the current image pointer
 * @param next_image_ref Reference to the next image pointer
 */
void image_animation_set_image_layers(Layer* images_layer, 
                                   GDrawCommandImage** prev_image_ref,
                                   GDrawCommandImage** current_image_ref, 
                                   GDrawCommandImage** next_image_ref);

/**
 * Clean up the image animation system
 */
void image_animation_deinit(void);

/**
 * Clean up the image animation subsystem
 */
void image_animation_deinit_system(void);

/**
 * Start an image animation
 * @param direction The direction of animation (up or down)
 * @param hour The current hour index (0-11)
 * @param page The current page (0=conditions, 1=airflow, 2=experiential)
 * @param on_complete Callback function to call when animation completes
 */
void image_animation_start(AnimationDirection direction, uint8_t hour, uint8_t page, void (*on_complete)(void));

/**
 * Check if an image animation is currently active
 * @return true if animation is active, false otherwise
 */
bool image_animation_is_active(void);

/**
 * Stop any active image animation
 */
void image_animation_stop(void);

/**
 * Check if the animation system is currently hiding images
 * @return true if images are hidden during animation, false otherwise
 */
bool image_animation_are_images_hidden(void);

/**
 * Store current images before view update (for animation purposes)
 * Call this before update_view() to preserve current images for animation
 */
void image_animation_store_current_images(void);

/**
 * Show specific image at destination (for debugging)
 * @param animation_num The animation number to show
 */
void image_animation_show_specific_at_destination(int animation_num); 

/**
 * Set the current page for the image animation system
 * @param page The current page (0=conditions, 1ow, 2=experiential)
 */
void image_animation_set_current_page(uint8_t page); 