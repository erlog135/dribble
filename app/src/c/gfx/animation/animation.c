#include "animation.h"
#include "text_animation.h"
#include "image_animation.h"

// Global animation system state
static bool s_animation_system_initialized = false;

// Custom animation curve for back out with overshoot effect
AnimationProgress animation_back_out_overshoot_curve(AnimationProgress linear_distance) {
    // Convert to float for calculations
    float t = (float)linear_distance / ANIMATION_NORMALIZED_MAX;
    
    // Back out curve with natural overshoot
    // Uses a modified back-out easing with increased overshoot parameter
    float s = 1.5f; // Overshoot parameter (higher = more overshoot)
    
    // Standard back-out formula: (t-1)^3 * (1+s) + (t-1)^2 * s + 1
    // This naturally creates an overshoot and settle-back behavior
    t = t - 1.0f;
    float result = t * t * t * (1.0f + s) + t * t * s + 1.0f;
    
    // Clamp result to prevent extreme values
    if (result < 0.0f) result = 0.0f;
    if (result > 1.2f) result = 1.2f; // Allow slight overshoot
    
    // Convert back to integer
    return (AnimationProgress)(result * ANIMATION_NORMALIZED_MAX);
}

// Custom animation curve for transition (goes out and back)
AnimationProgress animation_transition_out_and_back_curve(AnimationProgress linear_distance) {
    // Convert to float for calculations
    float t = (float)linear_distance / ANIMATION_NORMALIZED_MAX;
    
    // Create a curve that goes out to 1.0 at 50% progress, then back to 0.0
    // This creates a "hop out and back" effect
    float result;
    if (t < 0.5f) {
        // First half: go out (0.0 to 1.0)
        result = t * 2.0f; // Linear from 0 to 1
    } else {
        // Second half: come back (1.0 to 0.0)
        result = 1.0f - ((t - 0.5f) * 2.0f); // Linear from 1 to 0
    }
    
    // Apply easing to make it smoother
    result = result * result * (3.0f - 2.0f * result); // Smoothstep
    
    // Clamp result
    if (result < 0.0f) result = 0.0f;
    if (result > 1.0f) result = 1.0f;
    
    // Convert back to integer
    return (AnimationProgress)(result * ANIMATION_NORMALIZED_MAX);
}

void animation_system_init(void) {
    if (s_animation_system_initialized) {
        return;
    }
    
    // Initialize subsystems
    text_animation_init_system();
    image_animation_init_system();
    
    s_animation_system_initialized = true;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation system initialized");
}

void animation_system_deinit(void) {
    if (!s_animation_system_initialized) {
        return;
    }
    
    // Stop all animations
    animation_system_stop_all();
    
    // Deinitialize subsystems
    text_animation_deinit_system();
    image_animation_deinit_system();
    
    s_animation_system_initialized = false;
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "Animation system deinitialized");
}

bool animation_system_is_any_active(void) {
    return text_animation_is_active() || image_animation_is_active();
}

void animation_system_stop_all(void) {
    text_animation_stop();
    image_animation_stop();
    ANIMATION_LOG(APP_LOG_LEVEL_DEBUG, "All animations stopped");
} 