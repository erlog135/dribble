#include "demo.h"
#include <stdlib.h>
#include <string.h>

// Preset data structure for each forecast hour
typedef struct {
    const char* hour_string;
    int temp;
    int wind_speed;
    int wind_gust;
    int wind_dir;  // 0-15 for 16 directions
    int pressure;
    uint8_t conditions_icon;
    uint8_t experiential_icon;
    const char* experiential_string;
} PresetHourData;

// Customize these preset values for each hour (starting at 8AM, 2-hour intervals)
static const PresetHourData preset_hours[12] = {
    // hour_string, temp, wind_speed, wind_gust, wind_dir, pressure, conditions_icon, experiential_icon, experiential_string
    {"8AM", 68, 8, 12, 4, 1013, WEATHER_CONDITION_PARTLY_CLOUDY, 2, "Feels 66°\nUVI 4\nVis. 18mi"},
    {"10AM", 74, 10, 15, 5, 1012, WEATHER_CONDITION_CLEAR, 2, "Feels 72°\nUVI 6\nVis. 20mi"},
    {"12PM", 82, 12, 18, 6, 1011, WEATHER_CONDITION_CLEAR, 3, "Feels 80°\nUVI 9\nVis. 20mi"},
    {"2PM", 86, 14, 21, 7, 1010, WEATHER_CONDITION_CLEAR, 3, "Feels 84°\nUVI 10\nVis. 20mi"},
    {"4PM", 84, 15, 22, 0, 1009, WEATHER_CONDITION_PARTLY_CLOUDY, 3, "Feels 82°\nUVI 8\nVis. 18mi"},
    {"6PM", 78, 16, 24, 1, 1008, WEATHER_CONDITION_PARTLY_CLOUDY, 2, "Feels 76°\nUVI 4\nVis. 16mi"},
    {"8PM", 72, 14, 20, 2, 1007, WEATHER_CONDITION_PARTLY_CLOUDY, 0, "Feels 70°\nUVI 1\nVis. 15mi"},
    {"10PM", 68, 12, 17, 3, 1006, WEATHER_CONDITION_CLOUDY, 0, "Feels 66°\nUVI 0\nVis. 12mi"},
    {"12AM", 64, 10, 15, 4, 1005, WEATHER_CONDITION_CLOUDY, 0, "Feels 62°\nUVI 0\nVis. 10mi"},
    {"2AM", 62, 8, 12, 5, 1004, WEATHER_CONDITION_CLOUDY, 0, "Feels 60°\nUVI 0\nVis. 8mi"},
    {"4AM", 60, 6, 9, 6, 1003, WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT, 0, "Feels 58°\nUVI 0\nVis. 10mi"},
    {"6AM", 62, 7, 11, 7, 1004, WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT, 0, "Feels 60°\nUVI 0\nVis. 12mi"},
};

// Populate the global forecast_hours array with preset data
void demo_populate_forecast_hours(void) {
    for (int i = 0; i < 12; i++) {
        const PresetHourData* preset = &preset_hours[i];
        
        // Set hour string
        strncpy(forecast_hours[i].hour_string, preset->hour_string, 4);
        forecast_hours[i].hour_string[4] = '\0';
        
        // Set wind data
        forecast_hours[i].wind_speed = preset->wind_speed;
        forecast_hours[i].wind_direction = preset->wind_dir / 2;  // Convert to 0-7 range
        
        // For demo mode, assume default wind vane direction (wind heading, not origin)
        // The incoming wind direction represents where wind is coming FROM (meteorological convention)
        // Since default is wind heading (where wind is blowing TO), rotate 180 degrees (add 4)
        forecast_hours[i].wind_direction = (forecast_hours[i].wind_direction + 4) % 8;
        
        // Calculate wind speed level and resource ID based on wind speed
        uint8_t wind_speed_level;
        if (preset->wind_speed <= 16) {
            wind_speed_level = 0;  // Slow
        } else if (preset->wind_speed <= 32) {
            wind_speed_level = 1;  // Med
        } else {
            wind_speed_level = 2;  // Fast
        }
        
        // Base resource IDs for each speed level (first in each group)
        const uint32_t speed_base_ids[] = {
            RESOURCE_ID_WIND_SPEED_SLOW_N,   // 0: Slow
            RESOURCE_ID_WIND_SPEED_MED_N,    // 1: Med
            RESOURCE_ID_WIND_SPEED_FAST_N    // 2: Fast
        };
        
        // Calculate resource ID: base + direction offset
        forecast_hours[i].wind_speed_resource_id = speed_base_ids[wind_speed_level] + forecast_hours[i].wind_direction;
        
        // Set condition and experiential icons
        forecast_hours[i].conditions_icon = preset->conditions_icon;
        forecast_hours[i].experiential_icon = preset->experiential_icon;
        
        // Format conditions string
        snprintf(forecast_hours[i].conditions_string, MAX_STRING_LENGTH - 1,
                "%d°F\n%s",
                preset->temp,
                get_weather_condition_string(preset->conditions_icon));
        
        // Format airflow string
        snprintf(forecast_hours[i].airflow_string, MAX_STRING_LENGTH - 1,
                "%d mph %s\n%d mph gusts\n%d mb",
                preset->wind_speed,
                get_wind_direction_string(preset->wind_dir),
                preset->wind_gust,
                preset->pressure);
        
        // Use preset experiential string
        strncpy(forecast_hours[i].experiential_string, preset->experiential_string, MAX_STRING_LENGTH - 1);
        forecast_hours[i].experiential_string[MAX_STRING_LENGTH - 1] = '\0';
    }
}

// Populate the global precipitation variable with preset rain data
void demo_populate_precipitation(void) {
    // Set precipitation type to rain
    precipitation.precipitation_type = 2; // rain
    
    // Set preset rain intensity pattern - light rain starting in 15 minutes, lasting about 45 minutes
    for (int i = 0; i < PRECIPITATION_INTERVALS; i++) {
        if (i < 2) {
            // No rain for first 15 minutes (3 * 5min intervals)
            precipitation.precipitation_intensity[i] = 0;
        } else if (i < 6) {
            // Light rain for next 45 minutes (9 * 5min intervals)
            precipitation.precipitation_intensity[i] = 1;
        } else  if (i < 10) {
            // Moderate rain for next 30 minutes (6 * 5min intervals)
            precipitation.precipitation_intensity[i] = 2;
        } else {
            // Yes rain after that
            precipitation.precipitation_intensity[i] = 3;
        }
    }
    
    // Get the temperature from the first forecast hour
    char temp_line[MAX_STRING_LENGTH];
    strncpy(temp_line, forecast_hours[0].conditions_string, 
            strchr(forecast_hours[0].conditions_string, '\n') - forecast_hours[0].conditions_string);
    temp_line[strchr(forecast_hours[0].conditions_string, '\n') - forecast_hours[0].conditions_string] = '\0';
    
    // Format precipitation string based on current intensity
    if (precipitation.precipitation_intensity[0] == 0) {
        // No precipitation is happening now
        // Find first non-zero intensity
        int start = 0;
        while (start < PRECIPITATION_INTERVALS && precipitation.precipitation_intensity[start] == 0) {
            start++;
        }
        
        if (start < PRECIPITATION_INTERVALS) {
            // Precipitation is coming later
            int minutes_until = start * 5;
            snprintf(precipitation.precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nin %dm", temp_line, get_precipitation_string(precipitation.precipitation_type), minutes_until);
        } else {
            // No precipitation
            snprintf(precipitation.precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\nNo precipitation", temp_line);
        }
    } else {
        // Precipitation is happening now, find when it ends
        int end = 0;
        while (end < PRECIPITATION_INTERVALS && precipitation.precipitation_intensity[end] > 0) {
            end++;
        }
        end--; // Move back to last non-zero intensity
        
        int duration = (end + 1) * 5; // Duration in minutes
        if (duration > 60) {
            snprintf(precipitation.precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nfor 1h+", temp_line, get_precipitation_string(precipitation.precipitation_type));
        } else {
            snprintf(precipitation.precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nfor %dm", temp_line, get_precipitation_string(precipitation.precipitation_type), duration);
        }
    }
} 