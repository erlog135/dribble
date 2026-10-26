/*
    Persistent weather cache

    Stores the raw hourly and precipitation blobs received from PebbleKit JS
    along with the time they were received, so the app can show the last known
    weather immediately on launch. Raw blobs (rather than unpacked strings) are
    stored so that unit settings and 12h/24h style are re-applied on load.
*/

#pragma once

#include <pebble.h>

#define CACHE_HOURS_BLOB_SIZE 120
#define CACHE_PRECIP_BLOB_SIZE 7

// Persists the 120-byte hourly blob, stamped with the current time.
void cache_save_hours(const uint8_t* blob);

// Persists the 7-byte precipitation blob, stamped with the current time.
void cache_save_precip(const uint8_t* blob);

/*
    Loads cached weather into forecast_hours / precipitation.

    - Hours more than one hour before the current hour are dropped.
    - Precipitation is shifted forward by the number of whole 5 minute
      intervals elapsed since it was received (padding the tail with 0).
    - Sets forecast_hour_count and forecast_now_index.

    Returns the number of usable forecast hours (0 => nothing usable, the
    caller should fall back to fetching with the splash screen).
*/
uint8_t cache_load(void);

// Returns the age in seconds of the cached hourly data, or (uint32_t)-1 if no cache exists.
uint32_t cache_get_age_seconds(void);

// Returns true if cached hourly data exists and was fetched within max_age_seconds.
bool cache_is_fresh(uint32_t max_age_seconds);
