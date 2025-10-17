#include "msgproc.h"
#include "utils_common.h"
#include "weather.h"
#include "prefs.h"
#include <string.h>

// TODO: build wind icons here
void unpack_hour_package(HourPackage weather_data, ForecastHour* forecast_hour) {
    // Hour (uint8)
    int hour = weather_data[0];

    // Temperature (int8)
    int temp = (int8_t)weather_data[1];

    // Feels like temperature (int8)
    int feels_like = (int8_t)weather_data[2];

    // Wind speed (uint8)
    int wind_speed = weather_data[3];

    forecast_hour->wind_speed = wind_speed;


    // Set wind speed icon based on wind speed thresholds
    if (wind_speed <= 19) { 
        //beaufort 3 and below
        forecast_hour->wind_speed_icon = 0;
    } else if (wind_speed <= 38) { 
        //beaufort 5 and below
        forecast_hour->wind_speed_icon = 2; 
    } else { 
        //beaufort 6 and above
        forecast_hour->wind_speed_icon = 4;
    }

    // Wind gust speed (uint8) and data flags
    int wind_gust = weather_data[4];
    
    // Visibility (uint8)
    int visibility = weather_data[5];

    // Pressure (int8, difference from 1000mb)
    int pressure_mb = ((int8_t)weather_data[6]) + 1000;

    // Wind direction (4 bits) and air quality (4 bits)
    uint8_t wind_dir4 = weather_data[7] >> 4;
    uint8_t aqi4 = weather_data[7] & 0x0F;

    // UV index (4 bits) and data flags (4 bits)
    uint8_t uv_index = weather_data[8] >> 4;
    uint8_t data_flags = weather_data[8] & 0x0F;

    // Extract data availability flags
    bool has_wind_gust = (data_flags & 0x4) != 0;
    bool has_wind_dir = (data_flags & 0x2) != 0;
    bool has_air_quality = (data_flags & 0x1) != 0;

    // Condition and experiential icons (uint4 each)
    forecast_hour->conditions_icon = weather_data[9] >> 4;
    forecast_hour->experiential_icon = weather_data[9] & 0x0F;

    // Get user preferences
    ClaySettings* settings = prefs_get_settings();

    // Format hour string (e.g., "12PM")
    if (hour == 0) {
        strcpy(forecast_hour->hour_string, "12AM");
    } else if (hour < 12) {
        snprintf(forecast_hour->hour_string, 5, "%dAM", hour);
    } else if (hour == 12) {
        strcpy(forecast_hour->hour_string, "12PM");
    } else {
        snprintf(forecast_hour->hour_string, 5, "%dPM", hour - 12);
    }

    // Convert temperature if needed
    if (strcmp(settings->temperature_units, "C") == 0) {
        temp = fahrenheit_to_celsius(temp);
        feels_like = fahrenheit_to_celsius(feels_like);
    }

    // Format conditions string (e.g., "72°\nMostly\nCloudy")
    snprintf(forecast_hour->conditions_string, MAX_STRING_LENGTH - 1,
             "%d°%s\n%s",
             temp,
             settings->temperature_units,
             get_weather_condition_string(forecast_hour->conditions_icon));
    forecast_hour->conditions_string[MAX_STRING_LENGTH - 1] = '\0';

    // Convert wind speed if needed
    if (strcmp(settings->velocity_units, "mph") == 0) {
        wind_speed = kph_to_mph(wind_speed);
        if (has_wind_gust) wind_gust = kph_to_mph(wind_gust);
    } else if (strcmp(settings->velocity_units, "m/s") == 0) {
        wind_speed = kph_to_mps(wind_speed);
        if (has_wind_gust) wind_gust = kph_to_mps(wind_gust);
    }

    // Convert visibility if needed
    if (strcmp(settings->distance_units, "mi") == 0) {
        visibility = km_to_miles(visibility);
    }



    // Format airflow string with appropriate units, including only available data
    char airflow_buffer[MAX_STRING_LENGTH];
    int written = 0;

    // Start with wind speed (always available)
    written += snprintf(airflow_buffer + written, MAX_STRING_LENGTH - written,
                       "%d%s", wind_speed, settings->velocity_units);

    // Add wind direction if available
    if (has_wind_dir) {
        written += snprintf(airflow_buffer + written, MAX_STRING_LENGTH - written,
                          " %s", get_wind_direction_string(wind_dir4));
        // Map 16 directions to 8 directions:
        // Cardinals (0,4,8,12) map to (0,2,4,6)
        // All others map to diagonals (1,3,5,7)
        //theres gotta be a better mathy moduly way to do this
        if (wind_dir4 == 0) {
            forecast_hour->wind_direction = 0;  // N
        } else if (wind_dir4 == 4) {
            forecast_hour->wind_direction = 2;  // E  
        } else if (wind_dir4 == 8) {
            forecast_hour->wind_direction = 4;  // S
        } else if (wind_dir4 == 12) {
            forecast_hour->wind_direction = 6;  // W
        } else if (wind_dir4 >= 1 && wind_dir4 <= 3) {
            forecast_hour->wind_direction = 1;  // NE quadrant
        } else if (wind_dir4 >= 5 && wind_dir4 <= 7) {
            forecast_hour->wind_direction = 3;  // SE quadrant
        } else if (wind_dir4 >= 9 && wind_dir4 <= 11) {
            forecast_hour->wind_direction = 5;  // SW quadrant
        } else { // wind_dir4 >= 13 && wind_dir4 <= 15
            forecast_hour->wind_direction = 7;  // NW quadrant
        }

        UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "wind vane direction: %d", settings->wind_vane_direction);

        if(settings->wind_vane_direction == 1) {
            forecast_hour->wind_direction = (forecast_hour->wind_direction + 4) % 8;
        }
    }

    // Add wind gust if available
    if (has_wind_gust) {
        written += snprintf(airflow_buffer + written, MAX_STRING_LENGTH - written,
                          "\n%d%s gusts", wind_gust, settings->velocity_units);
    }

    // Add pressure (always available)
    if (strcmp(settings->pressure_units, "in") == 0) {
        // For inHg, use the x100 function to get value multiplied by 100
        uint16_t pressure_x100 = mb_to_inHg_x100(pressure_mb);
        written += snprintf(airflow_buffer + written, MAX_STRING_LENGTH - written,
                           "\n%d.%02d%s", pressure_x100 / 100, pressure_x100 % 100, settings->pressure_units);
    } else {
        written += snprintf(airflow_buffer + written, MAX_STRING_LENGTH - written,
                           "\n%d%s", pressure_mb, settings->pressure_units);
    }

    strncpy(forecast_hour->airflow_string, airflow_buffer, MAX_STRING_LENGTH - 1);
    forecast_hour->airflow_string[MAX_STRING_LENGTH - 1] = '\0';

    // Format experiential string with appropriate units
    written = 0;
    char experiential_buffer[MAX_STRING_LENGTH];

    // Start with feels like temperature (always available)
    written += snprintf(experiential_buffer + written, MAX_STRING_LENGTH - written,
                       "Feels %d°%s", feels_like, settings->temperature_units);

    // Add UV index (and air quality if available)
    if (has_air_quality) {
        written += snprintf(experiential_buffer + written, MAX_STRING_LENGTH - written,
                          "\nUVI %d AQI %d", uv_index, aqi4 * 50);
    } else {
        written += snprintf(experiential_buffer + written, MAX_STRING_LENGTH - written,
                          "\nUVI %d", uv_index);
    }

    // Add visibility (always available)
    written += snprintf(experiential_buffer + written, MAX_STRING_LENGTH - written,
                       "\nVis. %d%s", visibility, settings->distance_units);

    strncpy(forecast_hour->experiential_string, experiential_buffer, MAX_STRING_LENGTH - 1);
    forecast_hour->experiential_string[MAX_STRING_LENGTH - 1] = '\0';
}

void unpack_precipitation(PrecipitationPackage weather_data, Precipitation* precipitation) {
    // First byte: precipitation type (uint8)
    uint8_t type = weather_data[0];

    precipitation->precipitation_type = type;

    // Next 6 bytes: 24 precipitation intensity values (2 bits each)
    // Each byte contains 4 intensity values
    for (int i = 0; i < 6; i++) {
        uint8_t byte = weather_data[i + 1];
        for (int j = 0; j < 4; j++) {
            int index = i * 4 + j;
            if (index < PRECIPITATION_INTERVALS) {
                // Extract 2 bits for each intensity value
                precipitation->precipitation_intensity[index] = (byte >> (j * 2)) & 0x03;
            }
        }
    }

    // Get the temperature from the first forecast hour
    char temp_line[MAX_STRING_LENGTH];
    strncpy(temp_line, forecast_hours[0].conditions_string, strchr(forecast_hours[0].conditions_string, '\n') - forecast_hours[0].conditions_string);
    temp_line[strchr(forecast_hours[0].conditions_string, '\n') - forecast_hours[0].conditions_string] = '\0';

    // Format precipitation string based on type and intensity
    if (precipitation->precipitation_intensity[0] == 0) {
        // No precipitation is happening now
        // Find first non-zero intensity
        int start = 0;
        while (start < PRECIPITATION_INTERVALS && precipitation->precipitation_intensity[start] == 0) {
            start++;
        }

        if (start < PRECIPITATION_INTERVALS) {
            // Precipitation is coming later
            int minutes_until = start * 5;
            snprintf(precipitation->precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nin %dm", temp_line, get_precipitation_string(type), minutes_until);
        } else {
            // No precipitation
            snprintf(precipitation->precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\nNo precipitation", temp_line);
        }
    } else {
        // Precipitation is happening now, find when it ends
        int end = 0;
        while (end < PRECIPITATION_INTERVALS && precipitation->precipitation_intensity[end] > 0) {
            end++;
        }
        end--; // Move back to last non-zero intensity

        int duration = (end + 1) * 5; // Duration in minutes
        if (duration > 60) {
            snprintf(precipitation->precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nfor 1h+", temp_line, get_precipitation_string(type));
        } else {
            snprintf(precipitation->precipitation_string, MAX_STRING_LENGTH - 1,
                    "%s\n%s\nfor %dm", temp_line, get_precipitation_string(type), duration);
        }
    }
    precipitation->precipitation_string[MAX_STRING_LENGTH - 1] = '\0';
}
