#include "cache.h"
#include "utils_common.h"
#include "weather.h"
#include "msgproc.h"
#include "demo.h"
#include <string.h>
#include <time.h>

// Persist keys (prefs uses key 1)
#define CACHE_KEY_HOURS 2
#define CACHE_KEY_PRECIP 3

#define PRECIP_INTERVAL_SECONDS 300

typedef struct {
    uint32_t fetched_at;
    uint8_t blob[CACHE_HOURS_BLOB_SIZE];
} CachedHours;

typedef struct {
    uint32_t received_at;
    uint8_t blob[CACHE_PRECIP_BLOB_SIZE];
} CachedPrecip;

// Start (in time_t terms) of the local hour containing t
static time_t prv_hour_start(time_t t) {
    struct tm* lt = localtime(&t);
    if (!lt) {
        return t - (t % 3600);
    }
    return t - lt->tm_min * 60 - lt->tm_sec;
}

static int prv_hour_of_day(time_t t) {
    struct tm* lt = localtime(&t);
    return lt ? lt->tm_hour : 0;
}

static time_t s_cached_fetched_at = 0;

void cache_save_hours(const uint8_t* blob) {
    CachedHours cached;
    cached.fetched_at = (uint32_t)time(NULL);
    s_cached_fetched_at = (time_t)cached.fetched_at;
    memcpy(cached.blob, blob, CACHE_HOURS_BLOB_SIZE);
    persist_write_data(CACHE_KEY_HOURS, &cached, sizeof(cached));
}

void cache_save_precip(const uint8_t* blob) {
    CachedPrecip cached;
    cached.received_at = (uint32_t)time(NULL);
    memcpy(cached.blob, blob, CACHE_PRECIP_BLOB_SIZE);
    persist_write_data(CACHE_KEY_PRECIP, &cached, sizeof(cached));
}

static void prv_load_precip(time_t now) {
    CachedPrecip cached;
    bool have = persist_read_data(CACHE_KEY_PRECIP, &cached, sizeof(cached)) == (int)sizeof(cached)
                && (time_t)cached.received_at <= now;

    if (!have) {
        memset(&precipitation, 0, sizeof(precipitation));
        refresh_precipitation_string(&precipitation);
        return;
    }

    unpack_precipitation(cached.blob, &precipitation);

    // Advance the graph to the latest 5 minute interval
    uint32_t shift = (uint32_t)(now - (time_t)cached.received_at) / PRECIP_INTERVAL_SECONDS;
    uint8_t shifted[PRECIPITATION_INTERVALS];
    bool any = false;
    for (int i = 0; i < PRECIPITATION_INTERVALS; i++) {
        uint32_t src = i + shift;
        shifted[i] = (src < PRECIPITATION_INTERVALS) ? precipitation.precipitation_intensity[src] : 0;
        if (shifted[i] > 0) {
            any = true;
        }
    }
    memcpy(precipitation.precipitation_intensity, shifted, sizeof(shifted));
    if (!any) {
        precipitation.precipitation_type = 0;
    }
    refresh_precipitation_string(&precipitation);
}

uint8_t cache_load(void) {
    CachedHours cached;
    if (persist_read_data(CACHE_KEY_HOURS, &cached, sizeof(cached)) != (int)sizeof(cached)) {
        return 0;
    }
    s_cached_fetched_at = (time_t)cached.fetched_at;

    time_t now = time(NULL);
    if ((time_t)cached.fetched_at > now) {
        // Clock went backwards; can't trust the cache
        return 0;
    }

    time_t base = prv_hour_start((time_t)cached.fetched_at);
    time_t now_start = prv_hour_start(now);
    time_t cutoff = now_start - 3600;

    uint8_t valid = count_valid_hours(cached.blob);
    uint8_t kept = 0;
    uint8_t now_index = 0;

    for (int i = 0; i < valid; i++) {
        time_t abs_time = base + (time_t)cached.blob[i * 10] * 3600;
        if (abs_time < cutoff) {
            continue;
        }
        unpack_hour_package_at(&cached.blob[i * 10], &forecast_hours[kept], prv_hour_of_day(abs_time));
        // The current-hour entry is the latest one that is not in the future
        if (abs_time <= now_start) {
            now_index = kept;
        }
        kept++;
    }

    if (kept == 0) {
        UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Cached weather is too old, ignoring");
        return 0;
    }

    forecast_hour_count = kept;
    forecast_now_index = now_index;

    prv_load_precip(now);

    UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Loaded %d cached hours (now index %d)", kept, now_index);
    return kept;
}

uint32_t cache_get_age_seconds(void) {
    CachedHours cached;
    if (persist_read_data(CACHE_KEY_HOURS, &cached, sizeof(cached)) != (int)sizeof(cached)) {
        return (uint32_t)-1;
    }
    time_t now = time(NULL);
    if ((time_t)cached.fetched_at > now) {
        return (uint32_t)-1;
    }
    return (uint32_t)(now - (time_t)cached.fetched_at);
}

bool cache_is_fresh(uint32_t max_age_seconds) {
    uint32_t age = cache_get_age_seconds();
    if (age == (uint32_t)-1) {
        return false;
    }
    return age <= max_age_seconds;
}

time_t cache_get_fetched_at(void) {
    if (DEMO_MODE) {
        return time(NULL);
    }
    if (s_cached_fetched_at != 0) {
        return s_cached_fetched_at;
    }
    CachedHours cached;
    if (persist_read_data(CACHE_KEY_HOURS, &cached, sizeof(cached)) == (int)sizeof(cached)) {
        s_cached_fetched_at = (time_t)cached.fetched_at;
        return s_cached_fetched_at;
    }
    return 0;
}

