#include "image_animation.h"
#include "../layout.h"
#include "../pages/experiential.h"
#include "../../utils/weather.h" // for forecast_hours

// Experiential image offset definitions
GPoint experiential_image_offsets[] = {
    {0, 10}, //mask
    {0, -15}, //cap
    {0, 0}, //sunglasses
    {0, -10}, //hat
    {0, 0}, //hat+scarf
    {10, 0}, //umbrella
    {0, 0} //hat+scarf
};

//TODO: make "from" image 50px too

// Global image animation context
static ImageAnimationContext s_image_animation_context = {0};

// KiMaybe animation timing definitions
#define KM_DURATION_MS 200

// Forward declaration for internal completion callback
static void image_animation_complete_callback(void);

// Manual implementation of gdraw_command_image_clone if not available
//uh, why
static GDrawCommandImage* manual_clone_image(GDrawCommandImage* source) {
    if (!source) return NULL;
    
    // Try the built-in clone function first
    GDrawCommandImage* cloned = gdraw_command_image_clone(source);
    if (cloned) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Using built-in gdraw_command_image_clone");
        return cloned;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "gdraw_command_image_clone failed or not available");
    return NULL;
}

// Function to selectively show images based on ready flags
static void show_ready_images(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Updating image visibility - prev:%s, current:%s, next:%s",
            s_image_animation_context.show_prev_ready ? "YES" : "NO",
            s_image_animation_context.show_current_ready ? "YES" : "NO", 
            s_image_animation_context.show_next_ready ? "YES" : "NO");
    
    // Mark the progressive image layer as dirty to redraw with updated visibility
    if (s_image_animation_context.progressive_image_layer) {
        layer_mark_dirty(s_image_animation_context.progressive_image_layer);
    }
}

// KMAnimation completion callbacks
static void km_animation_1_complete(void) {
    s_image_animation_context.km_animations_completed++;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM Animation 1 completed, total: %d/%d", 
            s_image_animation_context.km_animations_completed, s_image_animation_context.km_animations_expected);
    
    // Hide KM animation layer 1 now that its animation is complete
    if (s_image_animation_context.km_animation_layer_1) {
        layer_set_hidden(s_image_animation_context.km_animation_layer_1, true);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM Animation layer 1 hidden");
    }
    
    // Animation 1 always moves to the current position (prev→current for UP, next→current for DOWN)
    s_image_animation_context.show_current_ready = true;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation 1 complete - current image ready to show");
    
    // Trigger a redraw to show the current position image
    show_ready_images();
    
    // Check if all expected KM animations are done
    if (s_image_animation_context.km_animations_completed >= s_image_animation_context.km_animations_expected) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "All KM animations completed");
        // Call internal completion callback to handle cleanup
        image_animation_complete_callback();
    }
}

static void km_animation_2_complete(void) {
    s_image_animation_context.km_animations_completed++;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM Animation 2 completed, total: %d/%d", 
            s_image_animation_context.km_animations_completed, s_image_animation_context.km_animations_expected);
    
    // Hide KM animation layer 2 now that its animation is complete
    if (s_image_animation_context.km_animation_layer_2) {
        layer_set_hidden(s_image_animation_context.km_animation_layer_2, true);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM Animation layer 2 hidden");
    }
    
    // Animation 2 destination depends on direction
    if (s_image_animation_context.direction == ANIMATION_DIRECTION_UP) {
        // UP: current→next, so show next image
        s_image_animation_context.show_next_ready = true;
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation 2 complete (UP) - next image ready to show");
    } else {
        // DOWN: current→prev, so show prev image  
        s_image_animation_context.show_prev_ready = true;
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation 2 complete (DOWN) - prev image ready to show");
    }
    
    // Trigger a redraw to show the appropriate position image
    show_ready_images();
    
    // Check if all expected KM animations are done
    if (s_image_animation_context.km_animations_completed >= s_image_animation_context.km_animations_expected) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "All KM animations completed");
        // Call internal completion callback to handle cleanup
        image_animation_complete_callback();
    }
}

// Timer callback for delayed KM animation start
static void km_animation_delay_timer_callback(void* context) {
    s_image_animation_context.km_animation_delay_timer = NULL;
    
    // Determine which animation to start based on stored direction
    if (s_image_animation_context.direction == ANIMATION_DIRECTION_UP) {
        // UP: Start animation 1 after delay (reversed order from DOWN)
        if (s_image_animation_context.km_animation_1) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting delayed KM animation 1 (UP direction)");
            km_start_kmanimation(s_image_animation_context.km_animation_1, km_animation_1_complete);
        }
    } else {
        // DOWN: Start animation 1 after delay
        if (s_image_animation_context.km_animation_1) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting delayed KM animation 1 (DOWN direction)");
            km_start_kmanimation(s_image_animation_context.km_animation_1, km_animation_1_complete);
        }
    }
}

// Function to store current images before view update (for animation purposes)
static void store_current_images_for_animation(void) {
    if (s_image_animation_context.prev_image_ref && 
        s_image_animation_context.current_image_ref && 
        s_image_animation_context.next_image_ref) {
        
        s_image_animation_context.stored_prev_image = *s_image_animation_context.prev_image_ref;
        s_image_animation_context.stored_current_image = *s_image_animation_context.current_image_ref;
        s_image_animation_context.stored_next_image = *s_image_animation_context.next_image_ref;
        
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Stored current images for animation - prev: %p, current: %p, next: %p", 
                (void*)s_image_animation_context.stored_prev_image, 
                (void*)s_image_animation_context.stored_current_image, 
                (void*)s_image_animation_context.stored_next_image);
    }
}

// Function to hide original images during animation
static void hide_original_images(void) {
    if (!s_image_animation_context.images_hidden) {
        // Reset progressive visibility flags - all images hidden initially
        s_image_animation_context.show_prev_ready = false;
        s_image_animation_context.show_current_ready = false;
        s_image_animation_context.show_next_ready = false;
        
        s_image_animation_context.images_hidden = true;
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Images hidden - progressive visibility will be controlled by animation completion");
        
        // Mark the main images layer as dirty to hide images immediately
        if (s_image_animation_context.images_layer) {
            layer_mark_dirty(s_image_animation_context.images_layer);
        }
        
        // Mark the progressive image layer as dirty to start with clean state
        if (s_image_animation_context.progressive_image_layer) {
            layer_mark_dirty(s_image_animation_context.progressive_image_layer);
        }
    }
}

// Function to show original images after animation
static void show_original_images(void) {
    if (s_image_animation_context.images_hidden) {
        // Animation is complete - show all images
        s_image_animation_context.show_prev_ready = true;
        s_image_animation_context.show_current_ready = true;
        s_image_animation_context.show_next_ready = true;
        
        // Clear the stored values - no longer needed
        s_image_animation_context.stored_prev_image = NULL;
        s_image_animation_context.stored_current_image = NULL;
        s_image_animation_context.stored_next_image = NULL;
        
        s_image_animation_context.images_hidden = false;
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "All images now visible, animation complete");
        
        // Mark the main images layer as dirty to show all new hour images
        if (s_image_animation_context.images_layer) {
            layer_mark_dirty(s_image_animation_context.images_layer);
        }
        
        // Mark the progressive image layer as dirty to complete the animation
        if (s_image_animation_context.progressive_image_layer) {
            layer_mark_dirty(s_image_animation_context.progressive_image_layer);
        }
    }
}

// Function to clean up KM animations
static void cleanup_km_animations(void) {
    // Cancel delay timer if active
    if (s_image_animation_context.km_animation_delay_timer) {
        app_timer_cancel(s_image_animation_context.km_animation_delay_timer);
        s_image_animation_context.km_animation_delay_timer = NULL;
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Cancelled KM animation delay timer");
    }
    
    // Dispose of KM animations
    if (s_image_animation_context.km_animation_1) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Disposing of KM animation 1");
        km_dispose_kmanimation(s_image_animation_context.km_animation_1);
        s_image_animation_context.km_animation_1 = NULL;
    }
    if (s_image_animation_context.km_animation_2) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Disposing of KM animation 2");
        km_dispose_kmanimation(s_image_animation_context.km_animation_2);
        s_image_animation_context.km_animation_2 = NULL;
    }
    
    // Clean up temporary image copies
    if (s_image_animation_context.km_temp_image_1) {
        gdraw_command_image_destroy(s_image_animation_context.km_temp_image_1);
        s_image_animation_context.km_temp_image_1 = NULL;
    }
    if (s_image_animation_context.km_temp_image_2) {
        gdraw_command_image_destroy(s_image_animation_context.km_temp_image_2);
        s_image_animation_context.km_temp_image_2 = NULL;
    }
    
    s_image_animation_context.km_animations_completed = 0;
    s_image_animation_context.km_animations_expected = 0;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM animations cleaned up");
}

// KM Animation layer 1 drawing function
static void km_animation_layer_1_update_proc(Layer* layer, GContext* ctx) {
    // Draw the first temporary transformed image if it exists (during animation)
    if (s_image_animation_context.km_temp_image_1) {
        gdraw_command_image_draw(ctx, s_image_animation_context.km_temp_image_1, GPointZero);
    }
}

// KM Animation layer 2 drawing function
static void km_animation_layer_2_update_proc(Layer* layer, GContext* ctx) {
    // Draw the second temporary transformed image if it exists (during animation)
    if (s_image_animation_context.km_temp_image_2) {
        gdraw_command_image_draw(ctx, s_image_animation_context.km_temp_image_2, GPointZero);
    }
}

// Progressive image showing layer drawing function
static void progressive_image_layer_update_proc(Layer* layer, GContext* ctx) {
    // During animation, also draw the new images that are ready to be shown
    // The page system will have updated the image references to point to the correct new images
    if (s_image_animation_context.images_hidden) {
        // Get the current images from the page system (which update_view() will have set)
        GDrawCommandImage* current_prev = s_image_animation_context.prev_image_ref ? *s_image_animation_context.prev_image_ref : NULL;
        GDrawCommandImage* current_current = s_image_animation_context.current_image_ref ? *s_image_animation_context.current_image_ref : NULL;
        GDrawCommandImage* current_next = s_image_animation_context.next_image_ref ? *s_image_animation_context.next_image_ref : NULL;
        
        // Draw new images selectively based on ready flags
        if (s_image_animation_context.show_prev_ready && current_prev) {
            // Check if this should use small axis positioning (conditions page, hour 1, with precipitation)
            GPoint draw_pos = layout.prev_icon_pos;
            if (s_image_animation_context.current_page == 0 && // VIEW_PAGE_CONDITIONS
                s_image_animation_context.current_hour == 1 && 
                precipitation.precipitation_type > 0) {
                // Use small axis position for precipitation display
                draw_pos = layout.axis_small_pos;
                ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Using small axis position for precipitation display");
            }
            gdraw_command_image_draw(ctx, current_prev, draw_pos);
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Drawing ready prev image at final position");
        }
        if (s_image_animation_context.show_current_ready && current_current) {
            // Check if this should use axis positioning (conditions page, hour 0, with precipitation)
            GPoint draw_pos = layout.current_icon_pos;
            if (s_image_animation_context.current_page == 0 && // VIEW_PAGE_CONDITIONS
                s_image_animation_context.current_hour == 0 && 
                precipitation.precipitation_type > 0) {
                // Use axis position for precipitation display
                draw_pos = layout.axis_large_pos;
                ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Using axis position for precipitation display");
            }
            gdraw_command_image_draw(ctx, current_current, draw_pos);
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Drawing ready current image at final position");
        }
        if (s_image_animation_context.show_next_ready && current_next) {
            gdraw_command_image_draw(ctx, current_next, layout.next_icon_pos);
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Drawing ready next image at final position");
        }
    }
}

// Image animation completion callback
static void image_animation_complete_callback(void) {
    // Clean up KM animations
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Attempting KM animations cleanup");
    cleanup_km_animations();
    
    // Show original images
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Attempting to show original images");
    show_original_images();
    
    // Update state
    s_image_animation_context.state = ANIMATION_STATE_IDLE;
    
    // Call user completion callback if provided
    if (s_image_animation_context.on_complete) {
        s_image_animation_context.on_complete();
        s_image_animation_context.on_complete = NULL;
    }
}

void image_animation_init_system(void) {
    // Initialize state
    s_image_animation_context.state = ANIMATION_STATE_IDLE;
    s_image_animation_context.current_page = 0; // Default to conditions page
    s_image_animation_context.current_hour = 0; // Default to hour 0
    s_image_animation_context.km_animation_1 = NULL;
    s_image_animation_context.km_animation_2 = NULL;
    s_image_animation_context.km_temp_image_1 = NULL;
    s_image_animation_context.km_temp_image_2 = NULL;
    s_image_animation_context.images_hidden = false;
    s_image_animation_context.km_animations_completed = 0;
    s_image_animation_context.km_animations_expected = 0;
    s_image_animation_context.on_complete = NULL;
    
    // Initialize progressive visibility flags
    s_image_animation_context.show_prev_ready = false;
    s_image_animation_context.show_current_ready = false;
    s_image_animation_context.show_next_ready = false;
    
    // Initialize stored image values
    s_image_animation_context.stored_prev_image = NULL;
    s_image_animation_context.stored_current_image = NULL;
    s_image_animation_context.stored_next_image = NULL;
    
    // Initialize timer
    s_image_animation_context.km_animation_delay_timer = NULL;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image animation subsystem initialized");
}

void image_animation_init(Layer* parent_layer) {
    // Create progressive image layer (shows new images as they become ready)
    s_image_animation_context.progressive_image_layer = layer_create(layer_get_bounds(parent_layer));
    layer_set_update_proc(s_image_animation_context.progressive_image_layer, progressive_image_layer_update_proc);
    layer_add_child(parent_layer, s_image_animation_context.progressive_image_layer);
    
    // Create KM animation layer 1
    s_image_animation_context.km_animation_layer_1 = layer_create(layer_get_bounds(parent_layer));
    layer_set_update_proc(s_image_animation_context.km_animation_layer_1, km_animation_layer_1_update_proc);
    layer_add_child(parent_layer, s_image_animation_context.km_animation_layer_1);

    // Create KM animation layer 2
    s_image_animation_context.km_animation_layer_2 = layer_create(layer_get_bounds(parent_layer));
    layer_set_update_proc(s_image_animation_context.km_animation_layer_2, km_animation_layer_2_update_proc);
    layer_add_child(parent_layer, s_image_animation_context.km_animation_layer_2);
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image animation initialized");
}

void image_animation_set_image_layers(Layer* images_layer, 
                                   GDrawCommandImage** prev_image_ref,
                                   GDrawCommandImage** current_image_ref, 
                                   GDrawCommandImage** next_image_ref) {
    s_image_animation_context.images_layer = images_layer;
    s_image_animation_context.prev_image_ref = prev_image_ref;
    s_image_animation_context.current_image_ref = current_image_ref;
    s_image_animation_context.next_image_ref = next_image_ref;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image layers set: layer=%p, prev_ref=%p, current_ref=%p, next_ref=%p", 
            (void*)images_layer, (void*)prev_image_ref, (void*)current_image_ref, (void*)next_image_ref);
}

void image_animation_deinit(void) {
    // Stop any ongoing animation
    image_animation_stop();
    
    // Clean up KM animations
    cleanup_km_animations();
    
    // Clean up animation layers
    if (s_image_animation_context.progressive_image_layer) {
        layer_destroy(s_image_animation_context.progressive_image_layer);
        s_image_animation_context.progressive_image_layer = NULL;
    }
    if (s_image_animation_context.km_animation_layer_1) {
        layer_destroy(s_image_animation_context.km_animation_layer_1);
        s_image_animation_context.km_animation_layer_1 = NULL;
    }
    if (s_image_animation_context.km_animation_layer_2) {
        layer_destroy(s_image_animation_context.km_animation_layer_2);
        s_image_animation_context.km_animation_layer_2 = NULL;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image animation deinitialized");
}

void image_animation_deinit_system(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image animation subsystem deinitialized");
}

void image_animation_set_current_page(uint8_t page) {
    s_image_animation_context.current_page = page;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image animation current page set to: %d", page);
}

void image_animation_start(AnimationDirection direction, uint8_t hour, uint8_t page, void (*on_complete)(void)) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "=== IMAGE_ANIMATION_START CALLED ===");
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Direction: %s, Hour: %d, Page: %d", 
            direction == ANIMATION_DIRECTION_UP ? "UP" : "DOWN", hour, page);
    
    // Don't start if already animating
    if (s_image_animation_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Already animating, returning");
        return;
    }
    
    // Validate that we have the necessary image references
    if (!s_image_animation_context.prev_image_ref || 
        !s_image_animation_context.current_image_ref || 
        !s_image_animation_context.next_image_ref) {
        ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Image references not set for animation");
        return;
    }
    
    // Reset progressive visibility flags for new animation
    s_image_animation_context.show_prev_ready = false;
    s_image_animation_context.show_current_ready = false;
    s_image_animation_context.show_next_ready = false;
    
    // Store completion callback
    s_image_animation_context.on_complete = on_complete;
    s_image_animation_context.direction = direction;
    s_image_animation_context.current_page = page;
    s_image_animation_context.current_hour = hour;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image references available - prev_ref: %p, current_ref: %p, next_ref: %p", 
            (void*)s_image_animation_context.prev_image_ref, (void*)s_image_animation_context.current_image_ref, (void*)s_image_animation_context.next_image_ref);
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Image values - prev: %p, current: %p, next: %p", 
            (void*)*s_image_animation_context.prev_image_ref, (void*)*s_image_animation_context.current_image_ref, (void*)*s_image_animation_context.next_image_ref);
    
    // Determine which images to animate based on direction
    // Use the stored images (old images) for animation
    GDrawCommandImage* source_image_1 = NULL;
    GDrawCommandImage* source_image_2 = NULL;
    GRect from_rect_1, to_rect_1, from_rect_2, to_rect_2;
    
    // Check if we're on the experiential page and need to apply offsets
    bool is_experiential_page = (page == 2); // VIEW_PAGE_EXPERIENTIAL
    int offset_idx_1 = 0, offset_idx_2 = 0;
    if (is_experiential_page) {
        // Defensive: clamp hour to 0-11
        int h = (hour > 11 ? 11 : hour);
        int prev_h = (h > 0) ? h - 1 : 0;
        int next_h = (h < 11) ? h + 1 : 11;
        if (direction == ANIMATION_DIRECTION_UP) {
            // prev (25px) → current (50px): use current hour's icon
            // current (50px) → next (25px): use next hour's icon
            offset_idx_1 = forecast_hours[h].experiential_icon > 0 ? forecast_hours[h].experiential_icon - 1 : 0;
            offset_idx_2 = forecast_hours[next_h].experiential_icon > 0 ? forecast_hours[next_h].experiential_icon - 1 : 0;
        } else {
            // next (25px) → current (50px): use current hour's icon
            // current (50px) → prev (25px): use previous hour's icon
            offset_idx_1 = forecast_hours[h].experiential_icon > 0 ? forecast_hours[h].experiential_icon - 1 : 0;
            offset_idx_2 = forecast_hours[prev_h].experiential_icon > 0 ? forecast_hours[prev_h].experiential_icon - 1 : 0;
        }
    }
    
    if (direction == ANIMATION_DIRECTION_UP) {
        // UP: prev image → current position (becomes new current)
        // current image → next position
        source_image_1 = s_image_animation_context.stored_prev_image;
        source_image_2 = s_image_animation_context.stored_current_image;
        
        // Calculate source and destination rectangles
        // Check if we need to use axis positions for precipitation scenarios
        GPoint from_pos_1 = layout.prev_icon_pos;
        GPoint to_pos_1 = layout.current_icon_pos;
        
        // Adjust for axis image positioning if needed
        if (page == 0 && precipitation.precipitation_type > 0) { // VIEW_PAGE_CONDITIONS with precipitation
            // UP direction from hour 1 to hour 0: prev (small axis from hour 1) → current (large axis at hour 0)
            if (hour == 0) {
                from_pos_1 = layout.axis_small_pos;  // Coming from small axis
                to_pos_1 = layout.axis_large_pos;    // Going to large axis
            }
        }
        
        // Calculate dimensions for animation rectangles (UP direction)
        // For precipitation scenarios, we need to use actual graph dimensions instead of icon dimensions
        int from_width_1 = layout.icon_small;
        int from_height_1 = layout.icon_small;
        int to_width_1 = layout.icon_large;
        int to_height_1 = layout.icon_large;
        int from_width_2 = layout.icon_large;
        int from_height_2 = layout.icon_large;
        int to_width_2 = layout.icon_small;
        int to_height_2 = layout.icon_small;
        
        // Adjust dimensions for precipitation scenarios (UP direction)
        if (page == 0 && precipitation.precipitation_type > 0) { // VIEW_PAGE_CONDITIONS with precipitation
            if (hour == 0) {
                // UP direction from hour 1 to hour 0: prev (small axis from hour 1) → current (large axis at hour 0)
                // The large axis should use actual graph dimensions, not icon dimensions
                to_width_1 = layout.precipitation_graph_width;   // 84px instead of 50px
                to_height_1 = layout.precipitation_graph_height; // 40px instead of 50px
            }
        }
        
        from_rect_1 = GRect(from_pos_1.x, from_pos_1.y, from_width_1, from_height_1);
        to_rect_1 = GRect(to_pos_1.x, to_pos_1.y, to_width_1, to_height_1);
        
        // For second animation: current → next
        GPoint from_pos_2 = layout.current_icon_pos;
        GPoint to_pos_2 = layout.next_icon_pos;
        
        // Adjust for axis image positioning if needed
        if (page == 0 && precipitation.precipitation_type > 0) { // VIEW_PAGE_CONDITIONS with precipitation
            // UP direction from hour 1 to hour 0: current (normal icon at hour 1) → next (normal icon)
            // Need to check what the CURRENT stored image represents - if we're going TO hour 0,
            // the stored current image is from hour 1 (normal position)
            if (hour == 0) {
                // The stored current image was from hour 1, so it starts at normal current position
                from_pos_2 = layout.current_icon_pos;  // Normal current position from hour 1
                // to_pos_2 stays as normal next position
            }
        }
        
        from_rect_2 = GRect(from_pos_2.x, from_pos_2.y, from_width_2, from_height_2);
        to_rect_2 = GRect(to_pos_2.x, to_pos_2.y, to_width_2, to_height_2);
        
        // Apply experiential offsets if on experiential page
        if (is_experiential_page) {
            // For UP animation: prev (25x) → current (50px)
            to_rect_1.origin.x += experiential_image_offsets[offset_idx_1].x;
            to_rect_1.origin.y += experiential_image_offsets[offset_idx_1].y;
            // For current (50px) → next (25px), apply halved and negated offset
            to_rect_2.origin.x += -(experiential_image_offsets[offset_idx_2].x / 2);
            to_rect_2.origin.y += -(experiential_image_offsets[offset_idx_2].y / 2);
        }
        
    } else {
        // DOWN: next image → current position (becomes new current)
        // current image → prev position
        source_image_1 = s_image_animation_context.stored_next_image;
        source_image_2 = s_image_animation_context.stored_current_image;
        
        // Calculate source and destination rectangles
        // Check if we need to use axis positions for precipitation scenarios
        GPoint from_pos_1 = layout.next_icon_pos;
        GPoint to_pos_1 = layout.current_icon_pos;
        GPoint from_pos_2 = layout.current_icon_pos;
        GPoint to_pos_2 = layout.prev_icon_pos;
        
        // Adjust for axis image positioning if needed
        if (page == 0 && precipitation.precipitation_type > 0) { // VIEW_PAGE_CONDITIONS with precipitation
            // DOWN direction from hour 0 to hour 1: stored images are from hour 0
            if (hour == 1) {
                // Animation 1: next (normal) → current (normal) - stays normal
                // Animation 2: current (large axis from hour 0) → prev (small axis at hour 1)
                from_pos_2 = layout.axis_large_pos;  // Coming from large axis (hour 0)
                to_pos_2 = layout.axis_small_pos;    // Going to small axis (hour 1)
            }
        }
        
        // Calculate dimensions for animation rectangles
        // For precipitation scenarios, we need to use actual graph dimensions instead of icon dimensions
        int from_width_1 = layout.icon_small;
        int from_height_1 = layout.icon_small;
        int to_width_1 = layout.icon_large;
        int to_height_1 = layout.icon_large;
        int from_width_2 = layout.icon_large;
        int from_height_2 = layout.icon_large;
        int to_width_2 = layout.icon_small;
        int to_height_2 = layout.icon_small;
        
        // Adjust dimensions for precipitation scenarios
        if (page == 0 && precipitation.precipitation_type > 0) { // VIEW_PAGE_CONDITIONS with precipitation
            if (hour == 0) {
                // UP direction from hour 1 to hour 0: prev (small axis from hour 1) → current (large axis at hour 0)
                // The large axis should use actual graph dimensions, not icon dimensions
                to_width_1 = layout.precipitation_graph_width;   // 84px instead of 50px
                to_height_1 = layout.precipitation_graph_height; // 40px instead of 50px
            } else if (hour == 1) {
                // DOWN direction from hour 0 to hour 1: current (large axis from hour 0) → prev (small axis at hour 1)
                // The large axis should use actual graph dimensions, not icon dimensions
                from_width_2 = layout.precipitation_graph_width;   // 84px instead of 50px
                from_height_2 = layout.precipitation_graph_height; // 40px instead of 50px
            }
        }
        
        from_rect_1 = GRect(from_pos_1.x, from_pos_1.y, from_width_1, from_height_1);
        to_rect_1 = GRect(to_pos_1.x, to_pos_1.y, to_width_1, to_height_1);
        
        from_rect_2 = GRect(from_pos_2.x, from_pos_2.y, from_width_2, from_height_2);
        to_rect_2 = GRect(to_pos_2.x, to_pos_2.y, to_width_2, to_height_2);
        
        // Apply experiential offsets if on experiential page
        if (is_experiential_page) {
            // For DOWN animation: next (25x) → current (50px)
            to_rect_1.origin.x += experiential_image_offsets[offset_idx_1].x;
            to_rect_1.origin.y += experiential_image_offsets[offset_idx_1].y;
            // For current (50px) → prev (25px), apply halved and negated offset
            to_rect_2.origin.x += -(experiential_image_offsets[offset_idx_2].x / 2);
            to_rect_2.origin.y += -(experiential_image_offsets[offset_idx_2].y / 2);
        }
    }
    
    // Determine sweep direction based on animation direction
    SweepDirection sweep_direction;
    if (direction == ANIMATION_DIRECTION_UP) {
        // Up animation → sweep up (reveal from bottom)
        sweep_direction = KM_SWEEP_UP;
    } else {
        // Down animation → sweep down (reveal from top)
        sweep_direction = KM_SWEEP_DOWN;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation setup - Direction: %s, Sweep: %s", 
            direction == ANIMATION_DIRECTION_UP ? "UP" : "DOWN",
            sweep_direction == KM_SWEEP_UP ? "UP" : "DOWN");
    
    // Create temporary copies of the images and set up KM animations if images are available
    if (source_image_1) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Attempting to clone source_image_1: %p", (void*)source_image_1);
        s_image_animation_context.km_temp_image_1 = manual_clone_image(source_image_1);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Cloned image 1 result: %p", (void*)s_image_animation_context.km_temp_image_1);
        
        if (s_image_animation_context.km_temp_image_1) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Creating KM animation 1 with layer: %p", (void*)s_image_animation_context.km_animation_layer_1);
            s_image_animation_context.km_animation_1 = km_make_transformation_kmanimation(
                s_image_animation_context.km_animation_layer_1,
                s_image_animation_context.km_temp_image_1,
                from_rect_1,
                to_rect_1,
                sweep_direction,
                KM_DURATION_MS,
                KM_TRANSLATE_AND_SCALE
            );
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM animation 1 created: %p", (void*)s_image_animation_context.km_animation_1);
            if (s_image_animation_context.km_animation_1) {
                ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Created KM animation 1: from (%d,%d,%d,%d) to (%d,%d,%d,%d), sweep: %s", 
                        from_rect_1.origin.x, from_rect_1.origin.y, from_rect_1.size.w, from_rect_1.size.h,
                        to_rect_1.origin.x, to_rect_1.origin.y, to_rect_1.size.w, to_rect_1.size.h,
                        sweep_direction == KM_SWEEP_UP ? "UP" : "DOWN");
            } else {
                ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to create KM animation 1");
            }
        } else {
            ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to clone source_image_1");
        }
    } else {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "source_image_1 is NULL, skipping animation 1");
    }
    
    if (source_image_2) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Attempting to clone source_image_2: %p", (void*)source_image_2);
        s_image_animation_context.km_temp_image_2 = manual_clone_image(source_image_2);
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Cloned image 2 result: %p", (void*)s_image_animation_context.km_temp_image_2);
        
        if (s_image_animation_context.km_temp_image_2) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Creating KM animation 2 with layer: %p", (void*)s_image_animation_context.km_animation_layer_2);
            s_image_animation_context.km_animation_2 = km_make_transformation_kmanimation(
                s_image_animation_context.km_animation_layer_2,
                s_image_animation_context.km_temp_image_2,
                from_rect_2,
                to_rect_2,
                sweep_direction,
                KM_DURATION_MS,
                KM_TRANSLATE_AND_SCALE
            );
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "KM animation 2 created: %p", (void*)s_image_animation_context.km_animation_2);
            if (s_image_animation_context.km_animation_2) {
                ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Created KM animation 2: from (%d,%d,%d,%d) to (%d,%d,%d,%d), sweep: %s", 
                        from_rect_2.origin.x, from_rect_2.origin.y, from_rect_2.size.w, from_rect_2.size.h,
                        to_rect_2.origin.x, to_rect_2.origin.y, to_rect_2.size.w, to_rect_2.size.h,
                        sweep_direction == KM_SWEEP_UP ? "UP" : "DOWN");
            } else {
                ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to create KM animation 2");
            }
        } else {
            ANIMATION_LOG(APP_LOG_LEVEL_ERROR, "Failed to clone source_image_2");
        }
    } else {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "source_image_2 is NULL, skipping animation 2");
    }
    
    // Count how many animations were successfully created
    s_image_animation_context.km_animations_expected = 0;
    if (s_image_animation_context.km_animation_1) {
        s_image_animation_context.km_animations_expected++;
    }
    if (s_image_animation_context.km_animation_2) {
        s_image_animation_context.km_animations_expected++;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Expected %d animations to complete", s_image_animation_context.km_animations_expected);
    
    // If no animations were created, complete immediately
    if (s_image_animation_context.km_animations_expected == 0) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "No animations created, completing immediately");
        image_animation_complete_callback();
        return;
    }
    
    // Start the KM animations with staggered timing
    if (direction == ANIMATION_DIRECTION_UP) {
        // UP: Start animation 2 immediately (current→next), delay animation 1 (prev→current)
        if (s_image_animation_context.km_animation_2) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting immediate KM animation 2 (current→next, UP direction)");
            km_start_kmanimation(s_image_animation_context.km_animation_2, km_animation_2_complete);
        }
        if (s_image_animation_context.km_animation_1) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Scheduling delayed KM animation 1 (prev→current, UP direction) with %dms offset", ANIMATION_DELAY_MS);
            s_image_animation_context.km_animation_delay_timer = app_timer_register(
                ANIMATION_DELAY_MS, 
                km_animation_delay_timer_callback, 
                NULL
            );
        }
    } else {
        // DOWN: Start animation 2 immediately (current→prev), delay animation 1 (next→current)
        if (s_image_animation_context.km_animation_2) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting immediate KM animation 2 (current→prev, DOWN direction)");
            km_start_kmanimation(s_image_animation_context.km_animation_2, km_animation_2_complete);
        }
        if (s_image_animation_context.km_animation_1) {
            ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Scheduling delayed KM animation 1 (next→current, DOWN direction) with %dms offset", ANIMATION_DELAY_MS);
            s_image_animation_context.km_animation_delay_timer = app_timer_register(
                ANIMATION_DELAY_MS, 
                km_animation_delay_timer_callback, 
                NULL
            );
        }
    }
    
    // Hide the original images during animation (after we've set up the stored images for animation)
    hide_original_images();
    
    // Show both KM animation layers at the start of animation
    if (s_image_animation_context.km_animation_layer_1) {
        layer_set_hidden(s_image_animation_context.km_animation_layer_1, false);
        layer_mark_dirty(s_image_animation_context.km_animation_layer_1);
    }
    if (s_image_animation_context.km_animation_layer_2) {
        layer_set_hidden(s_image_animation_context.km_animation_layer_2, false);
        layer_mark_dirty(s_image_animation_context.km_animation_layer_2);
    }
    
    // Mark the progressive image layer as dirty to trigger initial draw
    if (s_image_animation_context.progressive_image_layer) {
        layer_mark_dirty(s_image_animation_context.progressive_image_layer);
    }
    
    // Update state
    s_image_animation_context.state = ANIMATION_STATE_ANIMATING;
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Started KM animations for direction: %s", 
            direction == ANIMATION_DIRECTION_UP ? "UP" : "DOWN");
}

bool image_animation_is_active(void) {
    return s_image_animation_context.state == ANIMATION_STATE_ANIMATING;
}

bool image_animation_are_images_hidden(void) {
    return s_image_animation_context.images_hidden;
}

void image_animation_store_current_images(void) {
    store_current_images_for_animation();
}

void image_animation_stop(void) {
    if (s_image_animation_context.state == ANIMATION_STATE_ANIMATING) {
        // Force completion callback
        image_animation_complete_callback();
    }
}

void image_animation_show_specific_at_destination(int animation_num) {
    // Implementation for debugging - show specific image at destination
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Show specific image at destination: %d", animation_num);
    // This can be implemented based on specific debugging needs
} 