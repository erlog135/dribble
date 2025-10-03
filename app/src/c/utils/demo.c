#include "demo.h"
#include <stdlib.h>
#include <string.h>

// Populate the global forecast_hours array with mock data
void demo_populate_forecast_hours(void) {
    // Base values
    int base_temp = 72;  // 72°F
    int base_wind = 8;   // 8 mph
    int base_pressure = 1013;  // 1013 mb
    
    for (int i = 0; i < 12; i++) {
        // Generate random variations
        int temp = base_temp + (rand() % 11 - 5);
        int feels_like = temp + (rand() % 7 - 3);
        int wind_speed = base_wind + (rand() % 11);
        int wind_gust = wind_speed * (15 + rand() % 6) / 10;
        int visibility = 10 + (rand() % 11);
        int pressure = base_pressure + (rand() % 11 - 5);
        int wind_dir = rand() % 16;  // 0-15 for 16 directions
        int uv_index = rand() % 11;  // 0-10 UV index
        
        // Set hour string
        if (i == 0) {
            strcpy(forecast_hours[i].hour_string, "12AM");
        } else if (i < 12) {
            snprintf(forecast_hours[i].hour_string, 5, "%dAM", i);
        } else if (i == 12) {
            strcpy(forecast_hours[i].hour_string, "12PM");
        } else {
            snprintf(forecast_hours[i].hour_string, 5, "%dPM", i - 12);
        }
        
        // Set wind direction and icons
        forecast_hours[i].wind_direction = wind_dir / 2;
        
        // Set wind speed icon based on wind speed
        if (wind_speed <= 16) {
            forecast_hours[i].wind_speed_icon = 0;
        } else if (wind_speed <= 32) {
            forecast_hours[i].wind_speed_icon = 2;
        } else {
            forecast_hours[i].wind_speed_icon = 4;
        }
        
        // Set random conditions and experiential icons
        // Pick a random value from 0-12, but skip 5-8
        int icon = rand() % 9; // 0-8
        if (icon >= 6) icon += 3; // skip 5-8, so 5->8, 6->9, 7->10, 8->11
        forecast_hours[i].conditions_icon = icon;
        forecast_hours[i].experiential_icon = rand() % 7 + 1;
        
        // Format conditions string
        snprintf(forecast_hours[i].conditions_string, MAX_STRING_LENGTH - 1,
                "%d°F\n%s",
                temp,
                get_weather_condition_string(forecast_hours[i].conditions_icon));
        
        // Format airflow string
        snprintf(forecast_hours[i].airflow_string, MAX_STRING_LENGTH - 1,
                "%d mph %s\n%d mph gusts\n%d mb",
                wind_speed,
                get_wind_direction_string(wind_dir),
                wind_gust,
                pressure);
        
        // Format experiential string
        snprintf(forecast_hours[i].experiential_string, MAX_STRING_LENGTH - 1,
                "Demo\nDemo\nDemo");
    }
    
    // Set last 4 experiential icons to specific demo values
    forecast_hours[8].experiential_icon = 0;
    forecast_hours[9].experiential_icon = 2;
    forecast_hours[10].experiential_icon = 3;
    forecast_hours[11].experiential_icon = 3;
}

// Populate the global precipitation variable with mock data
void demo_populate_precipitation(void) {
    // Generate random precipitation type
    precipitation.precipitation_type = rand() % 7;
    
    // Generate random precipitation intensity pattern
    for (int i = 0; i < PRECIPITATION_INTERVALS; i++) {
        precipitation.precipitation_intensity[i] = rand() % 4;
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