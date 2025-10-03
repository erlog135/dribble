#include <pebble.h>
#include "resources.h"
#include "../utils/weather.h"

// Number of experiential resources
#define NUM_EXPERIENTIAL_RESOURCES 7
#define NUM_EMOJI_RESOURCES 4

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

// Array mapping emoji resource IDs
const uint32_t EMOJI_RESOURCE_IDS[] = {
    RESOURCE_ID_EMOJI_KISSING,        // 0: Kissing
    RESOURCE_ID_EMOJI_SMILE,          // 1: Smile
    RESOURCE_ID_EMOJI_TEETH,          // 2: Teeth
    RESOURCE_ID_EMOJI_WINKY_TONGUE    // 3: Winky Tongue
};

GDrawCommandImage** init_25px_condition_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * NUM_WEATHER_CONDITIONS);
    if (!images) return NULL;
    for (int i = 0; i < NUM_WEATHER_CONDITIONS; ++i) {
        images[i] = gdraw_command_image_create_with_resource(CONDITION_RESOURCE_IDS_25PX[i]);
    }
    return images;
}

GDrawCommandImage** init_50px_condition_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * NUM_WEATHER_CONDITIONS);
    if (!images) return NULL;
    for (int i = 0; i < NUM_WEATHER_CONDITIONS; ++i) {
        images[i] = gdraw_command_image_create_with_resource(CONDITION_RESOURCE_IDS_50PX[i]);
    }
    return images;
}

void deinit_25px_condition_images(GDrawCommandImage** condition_images_25px) {
    if (!condition_images_25px) return;
    for (int i = 0; i < NUM_WEATHER_CONDITIONS; ++i) {
        if (condition_images_25px[i]) {
            gdraw_command_image_destroy(condition_images_25px[i]);
        }
    }
    free(condition_images_25px);
}

void deinit_50px_condition_images(GDrawCommandImage** condition_images_50px) {
    if (!condition_images_50px) return;
    for (int i = 0; i < NUM_WEATHER_CONDITIONS; ++i) {
        if (condition_images_50px[i]) {
            gdraw_command_image_destroy(condition_images_50px[i]);
        }
    }
    free(condition_images_50px);
}

// Axis image functions
GDrawCommandImage* init_axis_small_image() {
    return gdraw_command_image_create_with_resource(RESOURCE_ID_AXIS_SMALL);
}

GDrawCommandImage* init_axis_large_image() {
    return gdraw_command_image_create_with_resource(RESOURCE_ID_AXIS_LARGE);
}

void deinit_axis_image(GDrawCommandImage* axis_image) {
    if (axis_image) {
        gdraw_command_image_destroy(axis_image);
    }
}

// Airflow image functions
GDrawCommandImage* init_wind_vane_image() {
    return gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_VANE);
}

GDrawCommandImage* init_wind_vane_angle_image() {
    return gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_VANE_ANGLE);
}

GDrawCommandImage** init_wind_speed_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * 6);  // 3 regular + 3 angled
    if (!images) return NULL;
    
    // Initialize the three regular wind speed images
    images[0] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_1);
    images[2] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_2);
    images[4] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_3);
    
    // Initialize the three angled wind speed images
    images[1] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_1_ANGLE);
    images[3] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_2_ANGLE);
    images[5] = gdraw_command_image_create_with_resource(RESOURCE_ID_WIND_SPEED_3_ANGLE);
    
    return images;
}

void deinit_wind_speed_images(GDrawCommandImage** wind_speed_images) {
    if (!wind_speed_images) return;
    
    for (int i = 0; i < 6; ++i) {  // Clean up all 6 images
        if (wind_speed_images[i]) {
            gdraw_command_image_destroy(wind_speed_images[i]);
        }
    }
    free(wind_speed_images);
}

// Experiential image functions
GDrawCommandImage** init_25px_experiential_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * NUM_EXPERIENTIAL_RESOURCES);
    if (!images) return NULL;
    for (int i = 0; i < NUM_EXPERIENTIAL_RESOURCES; ++i) {
        images[i] = gdraw_command_image_create_with_resource(EXPERIENTIAL_RESOURCE_IDS_25PX[i]);
    }
    return images;
}

GDrawCommandImage** init_50px_experiential_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * NUM_EXPERIENTIAL_RESOURCES);
    if (!images) return NULL;
    for (int i = 0; i < NUM_EXPERIENTIAL_RESOURCES; ++i) {
        images[i] = gdraw_command_image_create_with_resource(EXPERIENTIAL_RESOURCE_IDS_50PX[i]);
    }
    return images;
}

GDrawCommandImage** init_emoji_images() {
    GDrawCommandImage** images = malloc(sizeof(GDrawCommandImage*) * NUM_EMOJI_RESOURCES);
    if (!images) return NULL;
    for (int i = 0; i < NUM_EMOJI_RESOURCES; ++i) {
        images[i] = gdraw_command_image_create_with_resource(EMOJI_RESOURCE_IDS[i]);
    }
    return images;
}

void deinit_25px_experiential_images(GDrawCommandImage** experiential_images_25px) {
    if (!experiential_images_25px) return;
    for (int i = 0; i < NUM_EXPERIENTIAL_RESOURCES; ++i) {
        if (experiential_images_25px[i]) {
            gdraw_command_image_destroy(experiential_images_25px[i]);
        }
    }
    free(experiential_images_25px);
}

void deinit_50px_experiential_images(GDrawCommandImage** experiential_images_50px) {
    if (!experiential_images_50px) return;
    for (int i = 0; i < NUM_EXPERIENTIAL_RESOURCES; ++i) {
        if (experiential_images_50px[i]) {
            gdraw_command_image_destroy(experiential_images_50px[i]);
        }
    }
    free(experiential_images_50px);
}

void deinit_emoji_images(GDrawCommandImage** emoji_images) {
    if (!emoji_images) return;
    for (int i = 0; i < NUM_EMOJI_RESOURCES; ++i) {
        if (emoji_images[i]) {
            gdraw_command_image_destroy(emoji_images[i]);
        }
    }
    free(emoji_images);
}
