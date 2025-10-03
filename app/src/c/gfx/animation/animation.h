#pragma once
#include <pebble.h>

// Conditional logging for animation system
// Uncomment the line below to enable animation debug logging
// #define ANIMATION_LOGGING

#ifdef ANIMATION_LOGGING
  #define ANIMATION_LOG(level, fmt, ...) APP_LOG(level, fmt, ##__VA_ARGS__)
#else
  #define ANIMATION_LOG(level, fmt, ...)
#endif

// Common animation states
typedef enum {
    ANIMATION_STATE_IDLE,
    ANIMATION_STATE_ANIMATING
} AnimationState;

// Common animation directions
typedef enum {
    ANIMATION_DIRECTION_UP,
    ANIMATION_DIRECTION_DOWN
} AnimationDirection;

// Common animation timing constants
#define ANIMATION_DURATION_MS 300
#define ANIMATION_DELAY_MS 100

// Common animation system functions
void animation_system_init(void);
void animation_system_deinit(void);

// Custom animation curve for back out with overshoot effect
AnimationProgress animation_back_out_overshoot_curve(AnimationProgress linear_distance);

// Custom animation curve for transition (goes out and back)
AnimationProgress animation_transition_out_and_back_curve(AnimationProgress linear_distance);

// Common animation utility functions
bool animation_system_is_any_active(void);
void animation_system_stop_all(void); 