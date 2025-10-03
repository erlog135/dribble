#include "background_animation.h"

// Background animation context
typedef struct {
    AnimationState state;
    BackgroundAnimationDirection direction;
    GColor animation_color;
    
    // Window reference for background color changes
    Window* window;
    
    // Animation layer and rectangle
    Layer* animation_layer;
    GRect rect_bounds;
    
    // Property animation
    PropertyAnimation* rect_animation;
    
    // Completion callback
    void (*on_complete)(void);
} BackgroundAnimationContext;

static BackgroundAnimationContext s_context = {0};

// Custom animation curve with pronounced ease in/out
// Uses a cubic curve that starts and ends slowly with rapid acceleration in the middle
static AnimationProgress custom_pronounced_ease_curve(AnimationProgress linear_progress) {
    // Convert to normalized float between 0 and 1
    float t = (float)linear_progress / ANIMATION_NORMALIZED_MAX;
    
    // Cubic ease-in-out curve with extra pronounced effect
    // Formula: t < 0.5 ? 4 * t^3 : 1 - 4 * (1 - t)^3
    float result;
    if (t < 0.5f) {
        // Ease in: slow start with cubic acceleration
        result = 4.0f * t * t * t;
    } else {
        // Ease out: fast middle with cubic deceleration
        float f = 1.0f - t;
        result = 1.0f - 4.0f * f * f * f;
    }
    
    // Convert back to AnimationProgress
    return (AnimationProgress)(result * ANIMATION_NORMALIZED_MAX);
}

// Animation update callback
static void background_animation_update(struct Animation *animation, const AnimationProgress progress) {
    // Calculate the current rectangle position and size based on progress
    GRect layer_bounds = layer_get_bounds(s_context.animation_layer);
    
    switch (s_context.direction) {
        case BACKGROUND_ANIMATION_FROM_LEFT: {
            // Slide in from left edge
            int width = (int)((float)layer_bounds.size.w * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                0,
                0,
                width,
                layer_bounds.size.h
            );
            break;
        }
        case BACKGROUND_ANIMATION_FROM_RIGHT: {
            // Slide in from right edge
            int width = (int)((float)layer_bounds.size.w * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                layer_bounds.size.w - width,
                0,
                width,
                layer_bounds.size.h
            );
            break;
        }
        case BACKGROUND_ANIMATION_FROM_TOP: {
            // Slide in from top edge
            int height = (int)((float)layer_bounds.size.h * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                0,
                0,
                layer_bounds.size.w,
                height
            );
            break;
        }
        case BACKGROUND_ANIMATION_FROM_BOTTOM: {
            // Slide in from bottom edge
            int height = (int)((float)layer_bounds.size.h * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                0,
                layer_bounds.size.h - height,
                layer_bounds.size.w,
                height
            );
            break;
        }
    }
    
    // Mark layer as dirty to redraw
    layer_mark_dirty(s_context.animation_layer);
}

// Animation completion callback
static void background_animation_complete(struct Animation *animation, bool finished, void *context) {
    if (finished) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Background animation completed");
        
        // Change window background color
        if (s_context.window) {
            window_set_background_color(s_context.window, s_context.animation_color);
        }
        
        // Hide the animation layer
        layer_set_hidden(s_context.animation_layer, true);
        
        // Reset state
        s_context.state = ANIMATION_STATE_IDLE;
        
        // Call completion callback
        if (s_context.on_complete) {
            s_context.on_complete();
        }
    }
}

// Layer update procedure to draw the expanding rectangle
static void background_animation_layer_update(Layer* layer, GContext* ctx) {
    if (s_context.state != ANIMATION_STATE_ANIMATING) {
        return;
    }
    
    // Set the fill color
    graphics_context_set_fill_color(ctx, s_context.animation_color);
    
    // Draw the rectangle
    graphics_fill_rect(ctx, s_context.rect_bounds, 0, GCornerNone);
}

// Public API implementation
void background_animation_init_system(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Initializing background animation subsystem");
    memset(&s_context, 0, sizeof(BackgroundAnimationContext));
    s_context.state = ANIMATION_STATE_IDLE;
}

void background_animation_init(Layer* parent_layer, Window* window) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Initializing background animation system");
    
    s_context.window = window;
    
    // Create animation layer (same size as parent)
    GRect bounds = layer_get_bounds(parent_layer);
    s_context.animation_layer = layer_create(bounds);
    layer_set_update_proc(s_context.animation_layer, background_animation_layer_update);
    layer_set_hidden(s_context.animation_layer, true);
    
    // Add as child layer - it will draw behind other elements since it's added first
    // The layer ordering in Pebble is based on the order they're added to the parent
    layer_add_child(parent_layer, s_context.animation_layer);
}

void background_animation_deinit(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Deinitializing background animation system");
    
    // Stop any active animation
    background_animation_stop();
    
    // Clean up layer
    if (s_context.animation_layer) {
        layer_destroy(s_context.animation_layer);
        s_context.animation_layer = NULL;
    }
    
    s_context.window = NULL;
}

void background_animation_deinit_system(void) {
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Deinitializing background animation subsystem");
    // Nothing specific to clean up for subsystem
}

void background_animation_start(BackgroundAnimationDirection direction, 
                               GColor color, 
                               void (*on_complete)(void)) {
    // Don't start if already animating
    if (s_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Background animation already active");
        return;
    }
    
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Starting background animation, direction: %d", direction);
    
    s_context.state = ANIMATION_STATE_ANIMATING;
    s_context.direction = direction;
    s_context.animation_color = color;
    s_context.on_complete = on_complete;
    
    // Initialize rectangle bounds to zero
    s_context.rect_bounds = GRect(0, 0, 0, 0);
    
    // Show the animation layer
    layer_set_hidden(s_context.animation_layer, false);
    
    // Create and configure property animation
    s_context.rect_animation = property_animation_create_layer_frame(s_context.animation_layer, NULL, NULL);
    
    Animation* animation = property_animation_get_animation(s_context.rect_animation);
    animation_set_duration(animation, ANIMATION_DURATION_MS);
    animation_set_custom_curve(animation, custom_pronounced_ease_curve);
    
    // Set custom update and completion handlers
    static const AnimationImplementation animation_implementation = {
        .update = background_animation_update,
    };
    animation_set_implementation(animation, &animation_implementation);
    
    animation_set_handlers(animation, (AnimationHandlers) {
        .stopped = background_animation_complete,
    }, NULL);
    
    // Start the animation
    animation_schedule(animation);
}

bool background_animation_is_active(void) {
    return s_context.state == ANIMATION_STATE_ANIMATING;
}

void background_animation_stop(void) {
    if (s_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Stopping background animation");
        
        if (s_context.rect_animation) {
            Animation* animation = property_animation_get_animation(s_context.rect_animation);
            animation_unschedule(animation);
            property_animation_destroy(s_context.rect_animation);
            s_context.rect_animation = NULL;
        }
        
        // Hide the animation layer
        if (s_context.animation_layer) {
            layer_set_hidden(s_context.animation_layer, true);
        }
        
        s_context.state = ANIMATION_STATE_IDLE;
    }
}
