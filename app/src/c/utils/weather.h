/*
    Weather Data Structures and Functions
    
    This file contains the data structures and utility functions for handling weather data
    in the app. It defines constants, types, and conversion functions used
    throughout the weather display system.

    The weather data is organized into hourly forecasts containing:
    - Basic conditions (temperature, conditions) (plus precipitation if it's happening)
    - Wind and pressure information
    - Experiential factors (feels like, UV, visibility)
*/

#pragma once

#include <pebble.h>


#define MAX_STRING_LENGTH 64


// Weather condition codes for their corresponding strings
#define WEATHER_CONDITION_CLEAR 0
#define WEATHER_CONDITION_CLOUDY 1
#define WEATHER_CONDITION_PARTLY_CLOUDY 2
#define WEATHER_CONDITION_RAIN 3
#define WEATHER_CONDITION_SNOW 4
#define WEATHER_CONDITION_THUNDER 5
#define WEATHER_CONDITION_MIXED 6
#define WEATHER_CONDITION_SEVERE 7
#define WEATHER_CONDITION_WINDY 8
#define WEATHER_CONDITION_FOGGY 9
#define WEATHER_CONDITION_HAIL 10
#define WEATHER_CONDITION_CLEAR_NIGHT 11
#define WEATHER_CONDITION_PARTLY_CLOUDY_NIGHT 12

// Number of weather conditions
#define NUM_WEATHER_CONDITIONS 13

// Precipitation types
#define NUM_PRECIPITATION_TYPES 7

#define PRECIPITATION_INTERVALS 24

// Custom type for the 10-byte hourly weather data package
typedef uint8_t* HourPackage;

//and the 7 byte precipitation package
typedef uint8_t* PrecipitationPackage;

// Forecast hour struct
//HourPackage is unpacked into this struct
//hour_string: "12PM"
//conditions_string: "72°\nMostly\nCloudy"
//airflow_string: "12mph SW\n18mph gusts\n1004mb"
//experiential_string: "Feels 64°\nUVI 2\nVis. 20mi"
typedef struct forecast_hour {
    uint8_t wind_speed;
    int8_t wind_direction;
    uint8_t wind_speed_icon;
    uint8_t conditions_icon;
    uint8_t experiential_icon;
    char hour_string[5];
    char conditions_string[MAX_STRING_LENGTH];
    char airflow_string[MAX_STRING_LENGTH];
    char experiential_string[MAX_STRING_LENGTH];
    //TODO: store small&large icons (resource IDs) for all data here instead of per page
} ForecastHour;

// Precipitation struct
//precipitation_string: "Rain\nfor 13m"
//precipitation_intensity: [3, 2, 1, ... 0, 0, 0]
typedef struct Precipitation{
    uint8_t precipitation_type;
    char precipitation_string[MAX_STRING_LENGTH];
    uint8_t precipitation_intensity[PRECIPITATION_INTERVALS];
} Precipitation;

// Array of forecast hours
extern ForecastHour forecast_hours[12];

//next hour precipitation
Precipitation precipitation;


// Function declarations
const char* get_weather_condition_string(int condition_code);
const char* get_wind_direction_string(int degrees);
const char* get_precipitation_string(int precipitation_code);
const char* get_uv_index_string(int uv_index);

uint8_t km_to_miles(uint8_t kilo);
int8_t fahrenheit_to_celsius(int8_t fahrenheit);
uint8_t kph_to_mph(uint8_t kph);
uint8_t kph_to_mps(uint8_t kph);
uint8_t mb_to_inHg(uint8_t mb);

// Convert wind direction in degrees to 8-direction code (0-7)
// 0: Right (0°)
// 1: Bottom-right (45°)
// 2: Down (90°)
// 3: Bottom-left (135°)
// 4: Left (180°)
// 5: Top-left (225°)
// 6: Up (270°)
// 7: Top-right (315°)
uint8_t get_wind_direction_code(uint16_t degrees);