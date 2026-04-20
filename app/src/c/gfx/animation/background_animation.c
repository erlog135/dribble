#include "background_animation.h"

// Background animation context
typedef struct {
    AnimationState state;
    BackgroundAnimationDirection direction;
    GColor animation_color;
    
    // Window reference for background color changes
    Window* window;
    
    // Animation layer and rectangle (used on non-round screens)
    Layer* animation_layer;
    GRect rect_bounds;

    // Active animation (plain Animation + custom update; no PropertyAnimation
    // needed since the custom update fully computes each frame)
    Animation* rect_animation;
    
    // Completion callback
    void (*on_complete)(void);

#ifdef PBL_ROUND
    // Circle animation parameters (used on round screens)
    GPoint circle_start;           // Starting center (at a display edge)
    GPoint circle_end;             // Ending center (display center)
    int32_t circle_radius_end;     // Final radius to cover the display
    GPoint circle_current_center;  // Current animated center
    int32_t circle_current_radius; // Current animated radius
#endif
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
#ifdef PBL_ROUND
    // On round screens: interpolate circle center from edge start to display center,
    // and radius from 0 to the full display radius.
    int32_t dx = s_context.circle_end.x - s_context.circle_start.x;
    int32_t dy = s_context.circle_end.y - s_context.circle_start.y;
    s_context.circle_current_center = GPoint(
        s_context.circle_start.x + (int32_t)((int64_t)dx * progress / ANIMATION_NORMALIZED_MAX),
        s_context.circle_start.y + (int32_t)((int64_t)dy * progress / ANIMATION_NORMALIZED_MAX)
    );
    s_context.circle_current_radius =
        (int32_t)((int64_t)s_context.circle_radius_end * progress / ANIMATION_NORMALIZED_MAX);
#else
    // On rectangular screens: calculate the current rectangle position and size.
    GRect layer_bounds = layer_get_bounds(s_context.animation_layer);
    switch (s_context.direction) {
        case BACKGROUND_ANIMATION_FROM_LEFT: {
            int width = (int)((float)layer_bounds.size.w * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(0, 0, width, layer_bounds.size.h);
            break;
        }
        case BACKGROUND_ANIMATION_FROM_RIGHT: {
            int width = (int)((float)layer_bounds.size.w * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                layer_bounds.size.w - width, 0, width, layer_bounds.size.h);
            break;
        }
        case BACKGROUND_ANIMATION_FROM_TOP: {
            int height = (int)((float)layer_bounds.size.h * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(0, 0, layer_bounds.size.w, height);
            break;
        }
        case BACKGROUND_ANIMATION_FROM_BOTTOM: {
            int height = (int)((float)layer_bounds.size.h * progress / ANIMATION_NORMALIZED_MAX);
            s_context.rect_bounds = GRect(
                0, layer_bounds.size.h - height, layer_bounds.size.w, height);
            break;
        }
    }
#endif

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

// Layer update procedure to draw the expanding shape
static void background_animation_layer_update(Layer* layer, GContext* ctx) {
    if (s_context.state != ANIMATION_STATE_ANIMATING) {
        return;
    }
    
    graphics_context_set_fill_color(ctx, s_context.animation_color);
    
#ifdef PBL_ROUND
    // On round screens: draw an expanding/moving filled circle
    graphics_fill_circle(ctx, s_context.circle_current_center,
                         (uint16_t)s_context.circle_current_radius);
#else
    // On rectangular screens: draw an expanding filled rectangle
    graphics_fill_rect(ctx, s_context.rect_bounds, 0, GCornerNone);
#endif
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
    
#ifdef PBL_ROUND
    {
        // Set up circle animation: start at the display edge corresponding to the
        // direction, end at the display center with radius = half the display width.
        GRect bounds = layer_get_bounds(s_context.animation_layer);
        int center_x = bounds.size.w / 2;
        int center_y = bounds.size.h / 2;

        s_context.circle_end = GPoint(center_x, center_y);
        s_context.circle_radius_end = bounds.size.w / 2;
        s_context.circle_current_radius = 0;

        switch (direction) {
            case BACKGROUND_ANIMATION_FROM_RIGHT:
                s_context.circle_start = GPoint(bounds.size.w, center_y);
                break;
            case BACKGROUND_ANIMATION_FROM_TOP:
                s_context.circle_start = GPoint(center_x, 0);
                break;
            case BACKGROUND_ANIMATION_FROM_BOTTOM:
                s_context.circle_start = GPoint(center_x, bounds.size.h);
                break;
            case BACKGROUND_ANIMATION_FROM_LEFT:
            default:
                s_context.circle_start = GPoint(0, center_y);
                break;
        }
        s_context.circle_current_center = s_context.circle_start;
    }
#else
    // Initialize rectangle bounds to zero
    s_context.rect_bounds = GRect(0, 0, 0, 0);
#endif

    // Show the animation layer
    layer_set_hidden(s_context.animation_layer, false);

    // Create a plain Animation (no PropertyAnimation needed: our custom
    // .update callback computes the frame directly from s_context state).
    s_context.rect_animation = animation_create();
    animation_set_duration(s_context.rect_animation, ANIMATION_DURATION_MS);
    animation_set_custom_curve(s_context.rect_animation, custom_pronounced_ease_curve);

    static const AnimationImplementation animation_implementation = {
        .update = background_animation_update,
    };
    animation_set_implementation(s_context.rect_animation, &animation_implementation);

    animation_set_handlers(s_context.rect_animation, (AnimationHandlers) {
        .stopped = background_animation_complete,
    }, NULL);

    animation_schedule(s_context.rect_animation);
}

bool background_animation_is_active(void) {
    return s_context.state == ANIMATION_STATE_ANIMATING;
}

void background_animation_stop(void) {
    if (s_context.state == ANIMATION_STATE_ANIMATING) {
        ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Stopping background animation");
        
        if (s_context.rect_animation) {
            animation_unschedule(s_context.rect_animation);
            animation_destroy(s_context.rect_animation);
            s_context.rect_animation = NULL;
        }
        
        // Hide the animation layer
        if (s_context.animation_layer) {
            layer_set_hidden(s_context.animation_layer, true);
        }
        
        s_context.state = ANIMATION_STATE_IDLE;
    }
}
