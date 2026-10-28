#include <pebble.h>
#include "resources.h"
#include "../utils/weather.h"

// Number of experiential resources
#define NUM_EXPERIENTIAL_RESOURCES 7

// Color functions for conditions, airflow, and experiential states

// Get color for weather condition based on condition code
GColor get_condition_color(int condition_code) {
    switch (condition_code) {
        case 0:  return GColorYellow;        // CLEAR (sunny day)
        case 1:  return GColorLightGray;     // CLOUDY
        case 2:  return GColorPastelYellow;  // PARTLY_CLOUDY
        case 3:  return GColorPictonBlue;          // RAIN
        case 4:  return GColorBabyBlueEyes;         // SNOW
        case 5:  return GColorVeryLightBlue;        // THUNDER
        case 6:  return GColorPictonBlue;          // MIXED (rain/snow)
        case 7:  return GColorSunsetOrange;           // SEVERE
        case 8:  return GColorLightGray;      // WINDY
        case 9:  return GColorLightGray;     // FOGGY
        case 10: return GColorBabyBlueEyes;       // HAIL
        case 11: return GColorLavenderIndigo;      // CLEAR NIGHT
        case 12: return GColorRichBrilliantLavender;   // PARTLY CLOUDY NIGHT
        default: return GColorWhite;         // Default fallback
    }
}

// Get color for airflow intensity (3 darkness levels)
// From light to dark indicating increasing wind intensity
GColor get_airflow_color(int airflow_intensity) {
    switch (airflow_intensity) {
        case 0:  return GColorWhite;         // Light airflow
        case 1:  return GColorCeleste;       // Medium airflow
        case 2:  return GColorLightGray;     // High airflow
        default: return GColorWhite;         // Default fallback
    }
}

// Get color for experiential resource based on index
GColor get_experiential_color(int experiential_index) {
    switch (experiential_index) {
        case 0:  return GColorMediumSpringGreen;// None or default
        case 1:  return GColorSunsetOrange;     // Bad AQI
        case 2:  return GColorPastelYellow;     // Medium UVI
        case 3:  return GColorIcterine;         // High UVI
        case 4:  return GColorCyan;             // Cold
        case 5:  return GColorPictonBlue;      // Really Cold
        case 6:  return GColorVividCerulean;       // Rain (precipitation indicator)
        case 7:  return GColorLightGray;        // Foggy
        default: return GColorWhite;            // Default fallback
    }
}

// Array mapping weather condition codes to their corresponding 50px resource IDs
// Index corresponds to the condition code (see weather.h)
const uint32_t CONDITION_RESOURCE_IDS_50PX[] = {
    RESOURCE_ID_SUNNY_50PX,           // 0: CLEAR
    RESOURCE_ID_CLOUDY_50PX,          // 1: CLOUDY
    RESOURCE_ID_PARTLY_CLOUDY_50PX,   // 2: PARTLY_CLOUDY
    RESOURCE_ID_HEAVY_RAIN_50PX,      // 3: RAIN
    RESOURCE_ID_HEAVY_SNOW_50PX,      // 4: SNOW
    RESOURCE_ID_STORMY_50PX,            // 5: THUNDER
    RESOURCE_ID_RAINING_SNOWING_50PX, // 6: MIXED
    RESOURCE_ID_GENERIC_WEATHER_50PX, // 7: SEVERE
    RESOURCE_ID_WINDY_50PX,         // 8: WINDY
    RESOURCE_ID_CLOUDY_50PX,        // 9: FOGGY
    RESOURCE_ID_HEAVY_RAIN_50PX,    // 10: HAIL
    RESOURCE_ID_CLEAR_NIGHT_50PX, // 11: CLEAR NIGHT
    RESOURCE_ID_PARTLY_CLOUDY_NIGHT_50PX  // 12: PARTLY CLOUDY NIGHT
};



// Array mapping weather condition codes to their corresponding 25px resource IDs
const uint32_t CONDITION_RESOURCE_IDS_25PX[] = {
    RESOURCE_ID_SUNNY_25PX,           // 0: CLEAR
    RESOURCE_ID_CLOUDY_25PX,          // 1: CLOUDY
    RESOURCE_ID_PARTLY_CLOUDY_25PX,   // 2: PARTLY_CLOUDY
    RESOURCE_ID_HEAVY_RAIN_25PX,      // 3: RAIN
    RESOURCE_ID_HEAVY_SNOW_25PX,      // 4: SNOW
    RESOURCE_ID_STORMY_25PX, // 5: THUNDER
    RESOURCE_ID_RAINING_SNOWING_25PX, // 6: MIXED
    RESOURCE_ID_GENERIC_WEATHER_25PX, // 7: SEVERE
    RESOURCE_ID_WINDY_25PX, // 8: WINDY
    RESOURCE_ID_CLOUDY_25PX, // 9: FOGGY
    RESOURCE_ID_HEAVY_RAIN_25PX, // 10: HAIL
    RESOURCE_ID_CLEAR_NIGHT_25PX, // 11: CLEAR NIGHT
    RESOURCE_ID_PARTLY_CLOUDY_NIGHT_25PX  // 12: PARTLY CLOUDY NIGHT
};

// Array mapping experiential resource IDs for 25px images
const uint32_t EXPERIENTIAL_RESOURCE_IDS_25PX[] = {
    RESOURCE_ID_BAD_AQI_25PX,         // 0: Bad AQI
    RESOURCE_ID_MEDIUM_UVI_25PX,      // 1: Medium UVI
    RESOURCE_ID_HIGH_UVI_25PX,        // 2: High UVI
    RESOURCE_ID_COLD_25PX,            // 3: Cold
    RESOURCE_ID_REALLY_COLD_25PX,     // 4: Really Cold
    RESOURCE_ID_RAIN_25PX,            // 5: Rain
    RESOURCE_ID_FOGGY_25PX            // 6: Foggy
};

// Array mapping experiential resource IDs for 50px images (without the none icon)
const uint32_t EXPERIENTIAL_RESOURCE_IDS_50PX[] = {
    RESOURCE_ID_BAD_AQI_50PX,         // 0: Bad AQI
    RESOURCE_ID_MEDIUM_UVI_50PX,      // 1: Medium UVI
    RESOURCE_ID_HIGH_UVI_50PX,        // 2: High UVI
    RESOURCE_ID_COLD_50PX,            // 3: Cold
    RESOURCE_ID_REALLY_COLD_50PX,     // 4: Really Cold
    RESOURCE_ID_RAIN_50PX,            // 5: Rain
    RESOURCE_ID_FOGGY_50PX            // 6: Foggy
};


static bool use_sleepy_moon = false;
static bool moon_decided = false;

//Determine if moon is sleepy or not
static void decide_moon(void) {
    if (!moon_decided) {
        use_sleepy_moon = (rand() % 4 == 0);  // 1 in 4 chance
        moon_decided = true;
    }
}

GDrawCommandImage* load_condition_image(uint8_t code, bool large) {
    if (code >= NUM_WEATHER_CONDITIONS) return NULL;
    decide_moon();
    uint32_t id = large ? CONDITION_RESOURCE_IDS_50PX[code] : CONDITION_RESOURCE_IDS_25PX[code];
    if (code == 11 && use_sleepy_moon) {
        id = large ? RESOURCE_ID_SLEEPY_MOON_50PX : RESOURCE_ID_SLEEPY_MOON_25PX;
    }
    return gdraw_command_image_create_with_resource(id);
}

GDrawCommandImage* load_experiential_image(uint8_t icon, bool large) {
    if (icon == 0 || icon > NUM_EXPERIENTIAL_RESOURCES) return NULL;
    return gdraw_command_image_create_with_resource(
        large ? EXPERIENTIAL_RESOURCE_IDS_50PX[icon - 1] : EXPERIENTIAL_RESOURCE_IDS_25PX[icon - 1]);
}

GDrawCommandImage* load_wind_vane_image(int8_t dir) {
    static const uint32_t ids[] = {
        RESOURCE_ID_WIND_VANE_N, RESOURCE_ID_WIND_VANE_NE, RESOURCE_ID_WIND_VANE_E,
        RESOURCE_ID_WIND_VANE_SE, RESOURCE_ID_WIND_VANE_S, RESOURCE_ID_WIND_VANE_SW,
        RESOURCE_ID_WIND_VANE_W, RESOURCE_ID_WIND_VANE_NW
    };
    if (dir < 0 || dir >= 8) return NULL;
    return gdraw_command_image_create_with_resource(ids[dir]);
}

GDrawCommandImage* load_wind_speed_image(uint32_t resource_id) {
    if (resource_id == 0) return NULL;
    return gdraw_command_image_create_with_resource(resource_id);
}

// The emoji is chosen once per app launch. On low-memory platforms the image is
// loaded/unloaded as the page changes, but always uses this same choice.
GDrawCommandImage* load_random_emoji_image(void) {
    static const uint32_t emoji_ids[] = {
        RESOURCE_ID_EMOJI_KISSING, RESOURCE_ID_EMOJI_SMILE,
        RESOURCE_ID_EMOJI_TEETH, RESOURCE_ID_EMOJI_WINKY_TONGUE
    };
    static int8_t chosen = -1;
    if (chosen < 0) {
        chosen = rand() % 4;
    }
    return gdraw_command_image_create_with_resource(emoji_ids[chosen]);
}

GDrawCommandImage* load_axis_image(bool large) {
    return gdraw_command_image_create_with_resource(large ? RESOURCE_ID_AXIS_LARGE : RESOURCE_ID_AXIS_SMALL);
}
