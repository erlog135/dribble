#pragma once

#include <pebble.h>

// Settings structure to hold all user preferences
typedef struct ClaySettings {
    char temperature_units[4];    // "F" or "C"
    char velocity_units[4];       // "mph", "kph", or "mps"
    char distance_units[4];       // "mi" or "km"
    char pressure_units[4];       // "mb" or "in"
    char precipitation_units[4];  // "in" or "mm"
    int32_t refresh_interval;     // 20, 30, or 60 minutes
    bool self_refresh;            // true or false - enables self-refresh functionality
    int32_t display_interval;     // 1 or 2 hours
    bool animate;                 // true or false - enables animations
    int16_t wind_vane_direction;  // 0 = wind heading, 1 = wind origin
} ClaySettings;

// Initialize preferences with default values
void prefs_init(void);

// Load preferences from persistent storage
void prefs_load(void);

// Save preferences to persistent storage
void prefs_save(void);

// Get the current settings
ClaySettings* prefs_get_settings(void);
