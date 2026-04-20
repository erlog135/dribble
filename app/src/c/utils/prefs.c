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
    // Load defaults first; keep them unless a full struct is recovered from
    // persistent storage. Partial/short reads (e.g. after changing the
    // struct layout) would otherwise leave some fields uninitialized.
    prv_default_settings();

    int read = persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
    if (read != (int)sizeof(settings)) {
        prv_default_settings();
    }
}

void prefs_save(void) {
    persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

ClaySettings* prefs_get_settings(void) {
    return &settings;
}
