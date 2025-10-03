// Uncomment the line below to disable all log statements in this file
// #define APP_LOG_LEVEL APP_LOG_LEVEL_NONE

#include "transition.h"
#include "../layout.h"

// Global transition animation context
static TransitionAnimationContext s_transition_animation_context = {0};

// Forward declarations
static void transition_animation_stopped_handler(Animation* animation, bool finished, void* context);
static void transition_animation_complete_callback(void);

// Animation completion callback
static void transition_animation_complete_callback(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation completion callback called");
    
    // Restore all layers to their proper layout positions
    if (s_transition_animation_context.current_time_layer) {
        layer_set_frame(text_layer_get_layer(s_transition_animation_context.current_time_layer), layout.current_time_bounds);
    }
    if (s_transition_animation_context.current_text_layer) {
        layer_set_frame(text_layer_get_layer(s_transition_animation_context.current_text_layer), layout.current_text_bounds);
    }
    if (s_transition_animation_context.prev_time_layer) {
        layer_set_frame(text_layer_get_layer(s_transition_animation_context.prev_time_layer), layout.prev_time_bounds);
    }
    if (s_transition_animation_context.next_time_layer) {
        layer_set_frame(text_layer_get_layer(s_transition_animation_context.next_time_layer), layout.next_time_bounds);
    }
    
    // Mark images layer as dirty to redraw with normal positions
    if (s_transition_animation_context.images_layer) {
        layer_mark_dirty(s_transition_animation_context.images_layer);
    }
    
    // Clean up spawn animation (this automatically cleans up all component animations)
    if (s_transition_animation_context.spawn_animation) {
        animation_destroy(s_transition_animation_context.spawn_animation);
        s_transition_animation_context.spawn_animation = NULL;
    }
    
    // Reset individual animation pointers to NULL (don't destroy them - spawn owns them)
    s_transition_animation_context.current_time_animation = NULL;
    s_transition_animation_context.current_text_animation = NULL;
    s_transition_animation_context.prev_time_animation = NULL;
    s_transition_animation_context.next_time_animation = NULL;
    s_transition_animation_context.image_progress_animation = NULL;
    
    // Clean up image layer animation
    if (s_transition_animation_context.image_progress_animation) {
        property_animation_destroy(s_transition_animation_context.image_progress_animation);
        s_transition_animation_context.image_progress_animation = NULL;
    }
    
    // Cancel text animation delay timer
    if (s_transition_animation_context.text_animation_delay_timer) {
        app_timer_cancel(s_transition_animation_context.text_animation_delay_timer);
        s_transition_animation_context.text_animation_delay_timer = NULL;
    }
    

    
    // Update state
    s_transition_animation_context.state = ANIMATION_STATE_IDLE;
    
    // Call user completion callback if provided
    if (s_transition_animation_context.on_complete) {
        s_transition_animation_context.on_complete();
        s_transition_animation_context.on_complete = NULL;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation completion callback finished");
}

// Timer callback for text animation delay
static void text_animation_delay_callback(void* context) {
    s_transition_animation_context.text_animation_delay_timer = NULL;
    
    // Start the text animations now
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting text animations after delay");
    
    // Build array of text animations
    Animation* text_animations[4];
    int text_anim_count = 0;
    
    if (s_transition_animation_context.current_time_animation) {
        text_animations[text_anim_count++] = property_animation_get_animation(s_transition_animation_context.current_time_animation);
    }
    if (s_transition_animation_context.current_text_animation) {
        text_animations[text_anim_count++] = property_animation_get_animation(s_transition_animation_context.current_text_animation);
    }
    if (s_transition_animation_context.prev_time_animation) {
        text_animations[text_anim_count++] = property_animation_get_animation(s_transition_animation_context.prev_time_animation);
    }
    if (s_transition_animation_context.next_time_animation) {
        text_animations[text_anim_count++] = property_animation_get_animation(s_transition_animation_context.next_time_animation);
    }
    
    // Configure text animations with out-and-back curve
    for (int i = 0; i < text_anim_count; i++) {
        if (text_animations[i]) {
            animation_set_duration(text_animations[i], TRANSITION_ANIMATION_DURATION_MS);
            animation_set_custom_curve(text_animations[i], animation_transition_out_and_back_curve);
        }
    }
    
    // Create and start text spawn animation
    s_transition_animation_context.spawn_animation = animation_spawn_create_from_array(text_animations, text_anim_count);
    
    if (s_transition_animation_context.spawn_animation) {
        // Set completion handler on spawn animation
        animation_set_handlers(s_transition_animation_context.spawn_animation, (AnimationHandlers) {
            .stopped = transition_animation_stopped_handler,
        }, NULL);
        
        // Schedule spawn animation
        animation_schedule(s_transition_animation_context.spawn_animation);
        
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "TEXT ANIMATIONS STARTED after delay");
    } else {
        // Clean up on failure
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to create text spawn animation");
        transition_animation_complete_callback();
    }
}





// Animation stopped handler
static void transition_animation_stopped_handler(Animation* animation, bool finished, void* context) {
    if (finished && animation == s_transition_animation_context.spawn_animation) {
        transition_animation_complete_callback();
    }
}

void transition_animation_init_system(void) {
    // Initialize state
    s_transition_animation_context.state = ANIMATION_STATE_IDLE;
    s_transition_animation_context.spawn_animation = NULL;
    s_transition_animation_context.current_time_animation = NULL;
    s_transition_animation_context.current_text_animation = NULL;
    s_transition_animation_context.prev_time_animation = NULL;
    s_transition_animation_context.next_time_animation = NULL;
    s_transition_animation_context.image_progress_animation = NULL;
    s_transition_animation_context.on_complete = NULL;
    
    // Initialize image references
    s_transition_animation_context.images_layer = NULL;
    s_transition_animation_context.prev_image_ref = NULL;
    s_transition_animation_context.current_image_ref = NULL;
    s_transition_animation_context.next_image_ref = NULL;
    
    // Initialize timers
    s_transition_animation_context.text_animation_delay_timer = NULL;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation subsystem initialized");
}

void transition_animation_init(Layer* parent_layer) {
    // No additional layer needed for transition animations
    // We animate the existing text layers directly
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation initialized");
}

void transition_animation_set_layers(TextLayer* current_time_layer, 
                                   TextLayer* current_text_layer,
                                   TextLayer* prev_time_layer, 
                                   TextLayer* next_time_layer) {
    s_transition_animation_context.current_time_layer = current_time_layer;
    s_transition_animation_context.current_text_layer = current_text_layer;
    s_transition_animation_context.prev_time_layer = prev_time_layer;
    s_transition_animation_context.next_time_layer = next_time_layer;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation layers set: time=%p, text=%p, prev=%p, next=%p", 
            (void*)current_time_layer, (void*)current_text_layer, 
            (void*)prev_time_layer, (void*)next_time_layer);
}

void transition_animation_set_image_layers(Layer* images_layer, 
                                         GDrawCommandImage** prev_image_ref,
                                         GDrawCommandImage** current_image_ref, 
                                         GDrawCommandImage** next_image_ref) {
    s_transition_animation_context.images_layer = images_layer;
    s_transition_animation_context.prev_image_ref = prev_image_ref;
    s_transition_animation_context.current_image_ref = current_image_ref;
    s_transition_animation_context.next_image_ref = next_image_ref;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation image layers set: layer=%p, prev_ref=%p, current_ref=%p, next_ref=%p", 
            (void*)images_layer, (void*)prev_image_ref, (void*)current_image_ref, (void*)next_image_ref);
}

void transition_animation_deinit(void) {
    // Stop any ongoing animation
    transition_animation_stop();
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation deinitialized");
}

void transition_animation_deinit_system(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation subsystem deinitialized");
}

void transition_animation_start(void (*on_complete)(void)) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "=== TRANSITION_ANIMATION_START CALLED ===");
    
    // Don't start if already animating
    if (s_transition_animation_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Already animating, returning");
        return;
    }
    
    // Validate that we have the necessary layers
    if (!s_transition_animation_context.current_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "current_time_layer not set for transition animation");
        return;
    }
    if (!s_transition_animation_context.current_text_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "current_text_layer not set for transition animation");
        return;
    }
    if (!s_transition_animation_context.prev_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "prev_time_layer not set for transition animation");
        return;
    }
    if (!s_transition_animation_context.next_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "next_time_layer not set for transition animation");
        return;
    }
    
    // Calculate hop left positions (move left by 10 pixels)
    GRect time_hop_left = GRect(layout.current_time_bounds.origin.x - 10,
                               layout.current_time_bounds.origin.y,
                               layout.current_time_bounds.size.w,
                               layout.current_time_bounds.size.h);
    
    GRect text_hop_left = GRect(layout.current_text_bounds.origin.x - 5,
                               layout.current_text_bounds.origin.y,
                               layout.current_text_bounds.size.w,
                               layout.current_text_bounds.size.h);
    
    GRect prev_hop_left = GRect(layout.prev_time_bounds.origin.x - 10,
                               layout.prev_time_bounds.origin.y,
                               layout.prev_time_bounds.size.w,
                               layout.prev_time_bounds.size.h);
    
    GRect next_hop_left = GRect(layout.next_time_bounds.origin.x - 10,
                               layout.next_time_bounds.origin.y,
                               layout.next_time_bounds.size.w,
                               layout.next_time_bounds.size.h);
    
    // Calculate image slide-in positions (start from right side of screen)
    GRect prev_image_start = GRect(layout.screen_width + 20, layout.prev_icon_pos.y, 
                                  layout.icon_small, layout.icon_small);
    GRect current_image_start = GRect(layout.screen_width + 20, layout.current_icon_pos.y, 
                                     layout.icon_large, layout.icon_large);
    GRect next_image_start = GRect(layout.screen_width + 20, layout.next_icon_pos.y, 
                                  layout.icon_small, layout.icon_small);
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Creating transition animations - hop left by 10 pixels");
    
    // Create text animations: move from original position to hop position and back
    // The custom curve will handle the "back" part automatically
    s_transition_animation_context.current_time_animation = property_animation_create_layer_frame(
        text_layer_get_layer(s_transition_animation_context.current_time_layer),
        &layout.current_time_bounds, &time_hop_left
    );
    
    s_transition_animation_context.current_text_animation = property_animation_create_layer_frame(
        text_layer_get_layer(s_transition_animation_context.current_text_layer),
        &layout.current_text_bounds, &text_hop_left
    );
    
    s_transition_animation_context.prev_time_animation = property_animation_create_layer_frame(
        text_layer_get_layer(s_transition_animation_context.prev_time_layer),
        &layout.prev_time_bounds, &prev_hop_left
    );
    
    s_transition_animation_context.next_time_animation = property_animation_create_layer_frame(
        text_layer_get_layer(s_transition_animation_context.next_time_layer),
        &layout.next_time_bounds, &next_hop_left
    );
    
    // Set up image animation - animate the entire images layer
    if (s_transition_animation_context.images_layer) {
        // Calculate the layer's current frame and target frame
        GRect current_frame = layer_get_frame(s_transition_animation_context.images_layer);
        GRect start_frame = GRect(current_frame.origin.x + (layout.screen_width + 20), 
                                 current_frame.origin.y,
                                 current_frame.size.w,
                                 current_frame.size.h);
        
        // Immediately move the layer off-screen to prevent flash of new images
        layer_set_frame(s_transition_animation_context.images_layer, start_frame);
        
        // Create property animation for the layer frame - from off-screen right to normal position
        s_transition_animation_context.image_progress_animation = property_animation_create_layer_frame(
            s_transition_animation_context.images_layer,
            &start_frame, &current_frame
        );
        
        if (s_transition_animation_context.image_progress_animation) {
            // Configure the animation
            Animation* image_anim = property_animation_get_animation(s_transition_animation_context.image_progress_animation);
            animation_set_duration(image_anim, TRANSITION_IMAGE_ANIMATION_DURATION_MS);
            animation_set_custom_curve(image_anim, animation_back_out_overshoot_curve);
            
            // Schedule the animation
            animation_schedule(image_anim);
            
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image layer animation started");
        } else {
            ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to create image layer animation");
        }
    }
    
    // Store context and completion callback
    s_transition_animation_context.state = ANIMATION_STATE_ANIMATING;
    s_transition_animation_context.on_complete = on_complete;
    
    // Start image animation immediately
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting image animation immediately");
    
    // Schedule text animation delay timer
    s_transition_animation_context.text_animation_delay_timer = app_timer_register(
        TRANSITION_IMAGE_TEXT_DELAY_MS,
        text_animation_delay_callback,
        NULL
    );
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "TRANSITION ANIMATION STARTED - images immediately, text after %dms delay", TRANSITION_IMAGE_TEXT_DELAY_MS);
}

bool transition_animation_is_active(void) {
    return s_transition_animation_context.state == ANIMATION_STATE_ANIMATING;
}

void transition_animation_stop(void) {
    if (s_transition_animation_context.state == ANIMATION_STATE_ANIMATING) {
        // Stop spawn animation
        if (s_transition_animation_context.spawn_animation) {
            animation_unschedule(s_transition_animation_context.spawn_animation);
        }
        
        // Stop image layer animation
        if (s_transition_animation_context.image_progress_animation) {
            property_animation_destroy(s_transition_animation_context.image_progress_animation);
            s_transition_animation_context.image_progress_animation = NULL;
        }
        
        // Cancel text animation delay timer
        if (s_transition_animation_context.text_animation_delay_timer) {
            app_timer_cancel(s_transition_animation_context.text_animation_delay_timer);
            s_transition_animation_context.text_animation_delay_timer = NULL;
        }
        
        // Force completion callback
        transition_animation_complete_callback();
    }
}

bool transition_animation_get_image_positions(GPoint* prev_pos, GPoint* current_pos, GPoint* next_pos) {
    if (s_transition_animation_context.state != ANIMATION_STATE_ANIMATING) {
        // Not animating, return normal positions - the viewer.c drawing logic will handle axis positioning
        if (prev_pos) *prev_pos = layout.prev_icon_pos;
        if (current_pos) *current_pos = layout.current_icon_pos;
        if (next_pos) *next_pos = layout.next_icon_pos;
        return false;
    }
    
    // When the layer is animating, return normal positions since the layer itself is moving
    // The viewer.c drawing logic will handle axis positioning for individual images
    if (prev_pos) *prev_pos = layout.prev_icon_pos;
    if (current_pos) *current_pos = layout.current_icon_pos;
    if (next_pos) *next_pos = layout.next_icon_pos;
    
    return true;
}
