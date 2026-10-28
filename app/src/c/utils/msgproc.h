/*
    Message Processing Functions
    
    This file contains functions for unpacking weather data messages sent over from PebbleKit JS.
    The messages use a compact binary format to minimize data transfer while preserving
    precision of important weather metrics.

    Two message types are supported:
    1. Hourly forecast data (10 bytes per hour)
    2. Precipitation data (7 bytes for 2 hours of 5-min intervals)
*/

#pragma once

#include <pebble.h>
#include "weather.h"

/*

    managed to cram 12 fields into 10 bytes, so it can be sent easily at the expense of having to be unpacked
    Weather data is sent as one big 80 bit value, packaged in this format:

    uint8: relative hour offset (hours from current time)
    int8: temperature degrees (f)
    int8: temperature (feels like) degrees (f)
    uint8: wind speed (kph)
    uint8: wind gust speed (kph)
    uint8: visibility (km)
    int8: pressure difference from 1000mb (-128 to 127)
    uint4: wind direction (0-15) in 22.5 degree increments
    uint4: air quality index (1-5: 1=👍, 2=OK, 3=Meh, 4=👎, 5=💀)
    uint4: UV index (0-15)
    uint4: data flags (0000 -> empty, windgust, winddir, airquality)
    uint4: condition icon code
    uint4: experiential icon code

    Data flags indicate which optional fields are available:
    - Bit 3: Unused (reserved)
    - Bit 2: Wind gust available
    - Bit 1: Wind direction available
    - Bit 0: Air quality available
*/

void unpack_hour_package(HourPackage weather_data, ForecastHour* forecast_hour);

// Same as unpack_hour_package, but with the absolute hour of day (0-23) supplied
// by the caller (used when loading cached data).
void unpack_hour_package_at(HourPackage weather_data, ForecastHour* forecast_hour, int hour);

// Number of valid (non-padding) hour packages in a 120-byte blob.
uint8_t count_valid_hours(const uint8_t* data);

/*
    Unpacks all valid hour packages from a single 120-byte binary blob.
    Each hour package is 10 bytes, stored sequentially.
    Returns the number of valid hours unpacked.
    
    uint8[120]: 12 consecutive hour packages (10 bytes each)
*/
uint8_t unpack_all_hours(uint8_t* data, ForecastHour* forecast_hours_array);

/*
    Special details can be provided to the current hour, such as precipitation

    uint8: precipitation type (0 for none)
    uint2 * [24]: precipitation intensity (on a scale of 0-3) in 5 minute intervals, for up to 2 hours

*/

void unpack_precipitation(PrecipitationPackage weather_data, Precipitation* precipitation);

/*
    Formats precipitation->precipitation_string based on
    precipitation_intensity[] plus the supplied `temp_line` prefix.

    Shared by msgproc (live data) and demo (preset data).
*/
void format_precipitation_string(Precipitation* precipitation, const char* temp_line);

/*
    Rebuilds precipitation->precipitation_string using the temperature of the
    current-hour forecast entry (forecast_hours[forecast_now_index]).
*/
void refresh_precipitation_string(Precipitation* precipitation);

/*
    Sends REQUEST_DATA message to PebbleKit JS to fetch fresh weather data.
    Returns true if sent successfully, false otherwise.
*/
bool weather_request_data(void);