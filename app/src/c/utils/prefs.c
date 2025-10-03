#include "prefs.h"

#define SETTINGS_KEY 1

static ClaySettings settings;

static void prv_default_settings(void) {
    strcpy(settings.temperature_units, "F");
    strcpy(settings.velocity_units, "mph");
    strcpy(settings.distance_units, "mi");
    strcpy(settings.pressure_units, "mb");
    strcpy(settings.precipitation_units, "in");
    settings.refresh_interval = 30;
    settings.self_refresh = true;
    settings.display_interval = 2;
    settings.animate = true;
    settings.wind_vane_direction = 0;
}

void prefs_init(void) {
    prv_default_settings();
}

void prefs_load(void) {
    // Load the default settings
    prv_default_settings();
    // Read settings from persistent storage, if they exist
    persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

void prefs_save(void) {
    persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

ClaySettings* prefs_get_settings(void) {
    return &settings;
}
