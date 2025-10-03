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

    uint8: hour of day
    int8: temperature degrees (f)
    int8: temperature (feels like) degrees (f)
    uint8: wind speed (kph)
    uint8: wind gust speed (kph)
    uint8: visibility (km)
    int8: pressure difference from 1000mb (-128 to 127)
    uint4: wind direction (0-15) in 22.5 degree increments
    uint4: air quality index (0-15) 0-500 in steps of 50
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

/*
    Special details can be provided to the current hour, such as precipitation

    uint8: precipitation type (0 for none)
    uint2 * [24]: precipitation intensity (on a scale of 0-3) in 5 minute intervals, for up to 2 hours

*/

void unpack_precipitation(PrecipitationPackage weather_data, Precipitation* precipitation);