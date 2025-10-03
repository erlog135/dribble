#include "text_animation.h"
#include "image_animation.h"
#include "../layout.h"
#include "../../utils/weather.h"

// Global text animation context
static TextAnimationContext s_text_animation_context = {0};

// Animation completion callback
static void text_animation_complete_callback(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation completion callback called");
    
    // Hide all temporary layers and move them off-screen
    if (s_text_animation_context.temp_incoming_time_layer) {
        // Move the temporary layer off-screen first, then hide it
        GRect off_screen = GRect(-100, -100, 50, 50); // Move far off-screen
        layer_set_frame(text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer), off_screen);
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer), true);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Temporary incoming time layer moved off-screen and hidden");
    }
    
    // Restore all layers to their proper layout positions and show them
    if (s_text_animation_context.main_time_layer) {
        layer_set_frame(text_layer_get_layer(s_text_animation_context.main_time_layer), layout.current_time_bounds);
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.main_time_layer), false);
    }
    if (s_text_animation_context.main_text_layer) {
        layer_set_frame(text_layer_get_layer(s_text_animation_context.main_text_layer), layout.current_text_bounds);
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.main_text_layer), false);
    }
    if (s_text_animation_context.prev_time_layer) {
        layer_set_frame(text_layer_get_layer(s_text_animation_context.prev_time_layer), layout.prev_time_bounds);
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.prev_time_layer), false);
    }
    if (s_text_animation_context.next_time_layer) {
        layer_set_frame(text_layer_get_layer(s_text_animation_context.next_time_layer), layout.next_time_bounds);
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.next_time_layer), false);
    }
    
    // Clean up spawn animation (this automatically cleans up all component animations)
    if (s_text_animation_context.spawn_animation) {
        animation_destroy(s_text_animation_context.spawn_animation);
        s_text_animation_context.spawn_animation = NULL;
    }
    
    // Reset individual animation pointers to NULL (don't destroy them - spawn owns them)
    s_text_animation_context.incoming_time_animation = NULL;
    s_text_animation_context.prev_time_animation = NULL;
    s_text_animation_context.current_time_animation = NULL;
    s_text_animation_context.next_time_animation = NULL;
    s_text_animation_context.current_text_animation = NULL;
    
    // Update state
    s_text_animation_context.state = ANIMATION_STATE_IDLE;
    
    // Call user completion callback if provided
    if (s_text_animation_context.on_complete) {
        s_text_animation_context.on_complete();
        s_text_animation_context.on_complete = NULL;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation completion callback finished");
}

// Animation stopped handler
static void text_animation_stopped_handler(Animation* animation, bool finished, void* context) {
    if (finished && animation == s_text_animation_context.spawn_animation) {
        text_animation_complete_callback();
    }
}

void text_animation_init_system(void) {
    // Initialize image animation system
    image_animation_init_system();
    
    // Initialize state
    s_text_animation_context.state = ANIMATION_STATE_IDLE;
    s_text_animation_context.spawn_animation = NULL;
    s_text_animation_context.incoming_time_animation = NULL;
    s_text_animation_context.prev_time_animation = NULL;
    s_text_animation_context.current_time_animation = NULL;
    s_text_animation_context.next_time_animation = NULL;
    s_text_animation_context.current_text_animation = NULL;
    s_text_animation_context.on_complete = NULL;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation subsystem initialized");
}

void text_animation_init(Layer* parent_layer) {
    // Create animation layer
    s_text_animation_context.animation_layer = layer_create(layer_get_bounds(parent_layer));
    layer_add_child(parent_layer, s_text_animation_context.animation_layer);
    
    // Initialize image animation system
    image_animation_init(parent_layer);
    
    // Create temporary incoming time layer
    s_text_animation_context.temp_incoming_time_layer = text_layer_create(layout.current_time_bounds);
    text_layer_set_background_color(s_text_animation_context.temp_incoming_time_layer, GColorClear);
    text_layer_set_font(s_text_animation_context.temp_incoming_time_layer, fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS));
    layer_set_hidden(text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer), true);
    
    // Add temp incoming time layer to animation layer
    layer_add_child(s_text_animation_context.animation_layer, text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer));
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation initialized: incoming=%p", 
            (void*)s_text_animation_context.temp_incoming_time_layer);
}

void text_animation_set_main_layers(TextLayer* current_time_layer, TextLayer* current_text_layer) {
    s_text_animation_context.main_time_layer = current_time_layer;
    s_text_animation_context.main_text_layer = current_text_layer;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation main layers set: time=%p, text=%p", 
            (void*)current_time_layer, (void*)current_text_layer);
}

void text_animation_set_secondary_layers(TextLayer* prev_time_layer, TextLayer* next_time_layer) {
    s_text_animation_context.prev_time_layer = prev_time_layer;
    s_text_animation_context.next_time_layer = next_time_layer;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation secondary layers set: prev=%p, next=%p", 
            (void*)prev_time_layer, (void*)next_time_layer);
}

// Wrapper function to forward image layer setup to image animation system
void text_animation_set_image_layers(Layer* images_layer, 
                                   GDrawCommandImage** prev_image_ref,
                                   GDrawCommandImage** current_image_ref, 
                                   GDrawCommandImage** next_image_ref) {
    // Forward to image animation system
    image_animation_set_image_layers(images_layer, prev_image_ref, current_image_ref, next_image_ref);
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation image layers forwarded to image animation system");
}

void text_animation_deinit(void) {
    // Stop any ongoing animation
    text_animation_stop();
    
    // Deinitialize image animation system
    image_animation_deinit();
    
    // Clean up text layers
    if (s_text_animation_context.temp_incoming_time_layer) {
        text_layer_destroy(s_text_animation_context.temp_incoming_time_layer);
        s_text_animation_context.temp_incoming_time_layer = NULL;
    }
    
    // Clean up animation layer
    if (s_text_animation_context.animation_layer) {
        layer_destroy(s_text_animation_context.animation_layer);
        s_text_animation_context.animation_layer = NULL;
    }
}

void text_animation_deinit_system(void) {
    // Clean up image animation system
    image_animation_deinit_system();
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Text animation subsystem deinitialized");
}

void text_animation_start(AnimationDirection direction, uint8_t target_hour, const char* time_text, const char* content_text, void (*on_complete)(void)) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "=== TEXT_ANIMATION_START CALLED ===");
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Direction: %s, Target hour: %d", 
            direction == ANIMATION_DIRECTION_UP ? "UP" : "DOWN", target_hour);
    
    // Don't start if already animating
    if (s_text_animation_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Already animating, returning");
        return;
    }
    
    // Validate that we have the necessary layers
    if (!s_text_animation_context.main_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "main_time_layer not set for animation");
        return;
    }
    if (!s_text_animation_context.main_text_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "main_text_layer not set for animation");
        return;
    }
    if (!s_text_animation_context.prev_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "prev_time_layer not set for animation");
        return;
    }
    if (!s_text_animation_context.next_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "next_time_layer not set for animation");
        return;
    }
    if (!s_text_animation_context.temp_incoming_time_layer) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "temp_incoming_time_layer not set for animation");
        return;
    }
    
    // Update the main text layer content immediately to the new content
    text_layer_set_text(s_text_animation_context.main_text_layer, content_text);
    
    // Ensure temporary incoming time layer is hidden initially
    if (s_text_animation_context.temp_incoming_time_layer) {
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer), true);
    }
    
    // Determine if we should show incoming time based on target hour boundaries (1 hour away)
    bool show_incoming_time = true;
    if (direction == ANIMATION_DIRECTION_UP) {
        // For UP: incoming time shows hour (target_hour - 1)
        int incoming_hour_index = target_hour - 1;
        show_incoming_time = (incoming_hour_index >= 0);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "UP animation to hour %d, incoming hour index: %d, show_incoming_time: %s", 
                target_hour, incoming_hour_index, show_incoming_time ? "YES" : "NO");
        if (show_incoming_time) {
            // Set incoming time text from forecast hours array
            text_layer_set_text(s_text_animation_context.temp_incoming_time_layer, 
                               forecast_hours[incoming_hour_index].hour_string);
        }
    } else {
        // For DOWN: incoming time shows hour (target_hour + 1)  
        int incoming_hour_index = target_hour + 1;
        show_incoming_time = (incoming_hour_index <= 11);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "DOWN animation to hour %d, incoming hour index: %d, show_incoming_time: %s", 
                target_hour, incoming_hour_index, show_incoming_time ? "YES" : "NO");
        if (show_incoming_time) {
            // Set incoming time text from forecast hours array
            text_layer_set_text(s_text_animation_context.temp_incoming_time_layer, 
                               forecast_hours[incoming_hour_index].hour_string);
        }
    }
    
    // Show the temporary incoming time layer if needed
    if (show_incoming_time) {
        layer_set_hidden(text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer), false);
    }
    
    // Calculate proper off-screen positions based on screen dimensions
    GRect off_screen_top = GRect(layout.current_time_bounds.origin.x, 
                                -layout.current_time_bounds.size.h - 10,  // Well above screen
                                layout.current_time_bounds.size.w, 
                                layout.current_time_bounds.size.h);
    
    GRect off_screen_bottom = GRect(layout.current_time_bounds.origin.x, 
                                   layout.screen_height + 10,  // Well below screen
                                   layout.current_time_bounds.size.w, 
                                   layout.current_time_bounds.size.h);
    
    // Text "hop" positions - move up or down and back to normal
    GRect text_hop_up = GRect(layout.current_text_bounds.origin.x,
                             layout.current_text_bounds.origin.y - 20,  // Hop up 20 pixels
                             layout.current_text_bounds.size.w,
                             layout.current_text_bounds.size.h);
    
    GRect text_hop_down = GRect(layout.current_text_bounds.origin.x,
                               layout.current_text_bounds.origin.y + 20,  // Hop down 20 pixels
                               layout.current_text_bounds.size.w,
                               layout.current_text_bounds.size.h);
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation bounds - prev:(%d,%d), current:(%d,%d), next:(%d,%d)", 
            layout.prev_time_bounds.origin.x, layout.prev_time_bounds.origin.y,
            layout.current_time_bounds.origin.x, layout.current_time_bounds.origin.y,
            layout.next_time_bounds.origin.x, layout.next_time_bounds.origin.y);
    
    // Create individual animations based on direction
    if (direction == ANIMATION_DIRECTION_UP) {
        // UP: New time comes from top, moves to prev position (only if we have incoming time)
        if (show_incoming_time) {
            s_text_animation_context.incoming_time_animation = property_animation_create_layer_frame(
                text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer),
                &off_screen_top, &layout.prev_time_bounds
            );
        }
        
        // Prev time moves to current position
        s_text_animation_context.prev_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.prev_time_layer),
            &layout.prev_time_bounds, &layout.current_time_bounds
        );
        
        // Current time moves to next position
        s_text_animation_context.current_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.main_time_layer),
            &layout.current_time_bounds, &layout.next_time_bounds
        );
        
        // Next time moves off screen bottom
        s_text_animation_context.next_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.next_time_layer),
            &layout.next_time_bounds, &off_screen_bottom
        );
        
        // Current text hops down (up button press makes text hop down)
        s_text_animation_context.current_text_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.main_text_layer),
            &text_hop_up, &layout.current_text_bounds
        );
        
    } else {
        // DOWN: New time comes from bottom, moves to next position (only if we have incoming time)
        if (show_incoming_time) {
            s_text_animation_context.incoming_time_animation = property_animation_create_layer_frame(
                text_layer_get_layer(s_text_animation_context.temp_incoming_time_layer),
                &off_screen_bottom, &layout.next_time_bounds
            );
        }
        
        // Next time moves to current position
        s_text_animation_context.next_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.next_time_layer),
            &layout.next_time_bounds, &layout.current_time_bounds
        );
        
        // Current time moves to prev position
        s_text_animation_context.current_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.main_time_layer),
            &layout.current_time_bounds, &layout.prev_time_bounds
        );
        
        // Prev time moves off screen top
        s_text_animation_context.prev_time_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.prev_time_layer),
            &layout.prev_time_bounds, &off_screen_top
        );
        
        // Current text hops up (down button press makes text hop up)
        s_text_animation_context.current_text_animation = property_animation_create_layer_frame(
            text_layer_get_layer(s_text_animation_context.main_text_layer),
            &text_hop_down, &layout.current_text_bounds
        );
    }
    
    // Build array of valid animations
    Animation* animations[5];
    int anim_count = 0;
    
    if (s_text_animation_context.incoming_time_animation) {
        animations[anim_count++] = property_animation_get_animation(s_text_animation_context.incoming_time_animation);
    }
    if (s_text_animation_context.prev_time_animation) {
        animations[anim_count++] = property_animation_get_animation(s_text_animation_context.prev_time_animation);
    }
    if (s_text_animation_context.current_time_animation) {
        animations[anim_count++] = property_animation_get_animation(s_text_animation_context.current_time_animation);
    }
    if (s_text_animation_context.next_time_animation) {
        animations[anim_count++] = property_animation_get_animation(s_text_animation_context.next_time_animation);
    }
    if (s_text_animation_context.current_text_animation) {
        animations[anim_count++] = property_animation_get_animation(s_text_animation_context.current_text_animation);
    }
    
    // Verify we have animations to spawn
    if (anim_count == 0) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "No valid animations created for spawn");
        text_animation_complete_callback();
        return;
    }
    
    // Configure all animations with common curve
    for (int i = 0; i < anim_count; i++) {
        if (animations[i]) {
            animation_set_duration(animations[i], ANIMATION_DURATION_MS);
            animation_set_custom_curve(animations[i], animation_back_out_overshoot_curve);
        }
    }
    
    // Start image animation in parallel
    
    // Create and start spawn animation
    s_text_animation_context.spawn_animation = animation_spawn_create_from_array(animations, anim_count);
    
    if (s_text_animation_context.spawn_animation) {
        // Set completion handler on spawn animation
        animation_set_handlers(s_text_animation_context.spawn_animation, (AnimationHandlers) {
            .stopped = text_animation_stopped_handler,
        }, NULL);
        
        // Store context
        s_text_animation_context.state = ANIMATION_STATE_ANIMATING;
        s_text_animation_context.direction = direction;
        s_text_animation_context.on_complete = on_complete;
        
        // Schedule spawn animation
        animation_schedule(s_text_animation_context.spawn_animation);
        
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "TEXT ANIMATION STARTED - direction: %d, animations: %d", direction, anim_count);
    } else {
        // Clean up on failure
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to create spawn animation");
        text_animation_complete_callback();
    }
}

bool text_animation_is_active(void) {
    return s_text_animation_context.state == ANIMATION_STATE_ANIMATING;
}

void text_animation_stop(void) {
    if (s_text_animation_context.state == ANIMATION_STATE_ANIMATING) {
        // Stop spawn animation
        if (s_text_animation_context.spawn_animation) {
            animation_unschedule(s_text_animation_context.spawn_animation);
        }
        
        // Stop image animation
        image_animation_stop();
        
        // Force completion callback
        text_animation_complete_callback();
    }
} 