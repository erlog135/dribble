#include "weather.h"
#include <string.h>

// Array of condition strings indexed by condition code
static const char* WEATHER_CONDITION_STRINGS[] = {
    "Clear",           // 0
    "Cloudy",          // 1
    "Partly\nCloudy",   // 2
    "Rainy",            // 3
    "Snowy",            // 4
    "Thunder",         // 5
    "Mixed",           // 6
    "Severe",          // 7 - not used
    "Windy",           // 8
    "Foggy",           // 9
    "Hail",            // 10
    "Clear",           // 11 - night
    "Partly\nCloudy"   // 12 - night
};

static const char* PRECIPITATION_STRINGS[] = {
    "Clear",
    "Precip.",
    "Rain",
    "Snow",
    "Sleet",
    "Hail",
    "Mixed"
};

// Array of forecast hours
ForecastHour forecast_hours[12];

const char* get_weather_condition_string(int condition_code) {
    if (condition_code >= 0 && condition_code < NUM_WEATHER_CONDITIONS) {
        return WEATHER_CONDITION_STRINGS[condition_code];
    }
    return "Unknown";
}

const char* get_wind_direction_string(int direction_code) {
    // gets string before it is reduced to 8 directions
    int index = direction_code % 16; // ensure index is 0-15

    static const char* directions_16[] = {
        "N",    // 0
        "NNE",  // 1
        "NE",   // 2
        "ENE",  // 3
        "E",    // 4
        "ESE",  // 5
        "SE",   // 6
        "SSE",  // 7
        "S",    // 8
        "SSW",  // 9
        "SW",   // 10
        "WSW",  // 11
        "W",    // 12
        "WNW",  // 13
        "NW",   // 14
        "NNW"   // 15
    };

    return directions_16[index];
}

const char* get_precipitation_string(int precipitation_code) {
    if (precipitation_code >= 0 && precipitation_code < NUM_PRECIPITATION_TYPES) {
        return PRECIPITATION_STRINGS[precipitation_code];
    }
    return "Precip.";
}

const char* get_uv_index_string(int uv_index) {
    if (uv_index >= 7) {
        return "High";
    }
    if (uv_index >= 3) {
        return "Med";
    }
    return "Low";
}

uint8_t km_to_miles(uint8_t kilo) {
    return (uint8_t)(kilo * 0.621371f + 0.5f);
}

int8_t fahrenheit_to_celsius(int8_t fahrenheit) {
    return (int8_t)((fahrenheit - 32) * 5.0f / 9.0f + 0.5f);
}

uint8_t kph_to_mph(uint8_t kph) {
    return (uint8_t)(kph * 0.621371f + 0.5f);
}

uint8_t kph_to_mps(uint8_t kph) {
    return (uint8_t)(kph * 0.277778f + 0.5f);
}

uint8_t mb_to_inHg(uint8_t mb) {
    return (uint8_t)(mb * 0.02953f + 0.5f);
}

// Convert wind direction in degrees to 8-direction code (0-7)
uint8_t get_wind_direction_code(uint16_t degrees) {
    // Normalize degrees to 0-359
    degrees = degrees % 360;
    
    // Convert to 8-direction code
    // Each direction covers 45 degrees, centered on its angle
    // For example, direction 0 (right) covers 337.5° to 22.5°
    return ((degrees + 22) % 360) / 45;
}
