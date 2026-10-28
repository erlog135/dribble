#include "glance.h"
#include "weather.h"
#include "prefs.h"
#include "utils_common.h"
#include <string.h>
#include <time.h>

#ifndef PBL_PLATFORM_APLITE

static bool prv_is_snow_condition(uint8_t code) {
    return (code == WEATHER_CONDITION_SNOW ||
            code == WEATHER_CONDITION_MIXED ||
            code == WEATHER_CONDITION_HAIL);
}

static bool prv_is_rain_condition(uint8_t code) {
    return (code == WEATHER_CONDITION_RAIN ||
            code == WEATHER_CONDITION_THUNDER);
}

static bool prv_is_precip_condition(uint8_t code) {
    return prv_is_rain_condition(code) || prv_is_snow_condition(code);
}

static bool prv_get_package_precip_range(int *out_start_idx, int *out_end_idx) {
    if (precipitation.precipitation_type == 0) {
        return false;
    }
    int start = 0;
    while (start < PRECIPITATION_INTERVALS && precipitation.precipitation_intensity[start] == 0) {
        start++;
    }
    if (start >= PRECIPITATION_INTERVALS) {
        return false;
    }
    int end = start;
    while (end < PRECIPITATION_INTERVALS && precipitation.precipitation_intensity[end] > 0) {
        end++;
    }
    if (out_start_idx) *out_start_idx = start;
    if (out_end_idx) *out_end_idx = end;
    return true;
}

static void prv_format_clock_time(time_t t, char* buffer, size_t buf_len) {
    struct tm* lt = localtime(&t);
    if (!lt) {
        snprintf(buffer, buf_len, "--:--");
        return;
    }
    if (clock_is_24h_style()) {
        snprintf(buffer, buf_len, "%02d:%02d", lt->tm_hour, lt->tm_min);
    } else {
        int hour = lt->tm_hour;
        const char* ampm = (hour >= 12) ? "PM" : "AM";
        hour = hour % 12;
        if (hour == 0) hour = 12;
        snprintf(buffer, buf_len, "%d:%02d%s", hour, lt->tm_min, ampm);
    }
}

static void prv_format_hour_for_time(time_t t, char* buffer, size_t buf_len) {
    struct tm* lt = localtime(&t);
    int hour = lt ? lt->tm_hour : 0;
    if (clock_is_24h_style()) {
        snprintf(buffer, buf_len, "%02d:00", hour);
    } else if (hour == 0) {
        snprintf(buffer, buf_len, "12AM");
    } else if (hour < 12) {
        snprintf(buffer, buf_len, "%dAM", hour);
    } else if (hour == 12) {
        snprintf(buffer, buf_len, "12PM");
    } else {
        snprintf(buffer, buf_len, "%dPM", hour - 12);
    }
}

static bool prv_add_slice(AppGlanceReloadSession *session, size_t limit, size_t *added,
                          const char *subtitle, uint32_t icon, time_t expiration) {
    if (*added >= limit) {
        return false;
    }
    AppGlanceSlice slice = {
        .expiration_time = expiration,
        .layout.icon = icon,
        .layout.subtitle_template_string = subtitle,
    };
    AppGlanceResult result = app_glance_add_slice(session, slice);
    if (result == APP_GLANCE_RESULT_SUCCESS) {
        (*added)++;
        UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Added glance slice: '%s', exp: %ld", subtitle, (long)expiration);
        return true;
    } else {
        UTIL_LOG(APP_LOG_LEVEL_ERROR, "app_glance_add_slice failed: %d", (int)result);
        return false;
    }
}

static void prv_glance_reload_callback(AppGlanceReloadSession *session, size_t limit, void *context) {
    if (limit < 1 || forecast_hour_count == 0) {
        return;
    }

    size_t added = 0;
    time_t now = time(NULL);

    ClaySettings* settings = prefs_get_settings();
    int interval_hours = (settings->display_interval > 0) ? settings->display_interval : 2;

    struct tm* lt = localtime(&now);
    time_t cur_hour_start = lt ? (now - lt->tm_min * 60 - lt->tm_sec) : (now - (now % 3600));

    uint8_t now_idx = forecast_now_index;
    if (now_idx >= forecast_hour_count) {
        now_idx = 0;
    }
    uint8_t total_hours = forecast_hour_count;

    char slice0_text[MAX_STRING_LENGTH];
    char slice1_text[MAX_STRING_LENGTH];
    char slice2_text[MAX_STRING_LENGTH];

    int precip_start = 0;
    int precip_end = 0;
    bool has_package_precip = prv_get_package_precip_range(&precip_start, &precip_end);

    if (!has_package_precip) {
        // No active or upcoming precipitation in the precipitation package
        // Check if any forecast hour from now_idx onwards has rain or snow
        int first_precip_idx = -1;
        for (uint8_t i = now_idx; i < total_hours; i++) {
            if (prv_is_precip_condition(forecast_hours[i].conditions_icon)) {
                first_precip_idx = i;
                break;
            }
        }

        if (first_precip_idx == -1) {
            // Case 1: No precipitation detected anywhere in the forecast period
            snprintf(slice0_text, sizeof(slice0_text), "No precipitation");
            prv_add_slice(session, limit, &added, slice0_text,
                          PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
            return;
        }

        // Case 2: Possible {rain|snow} at {hour}
        bool is_snow = prv_is_snow_condition(forecast_hours[first_precip_idx].conditions_icon);
        const char* precip_word = is_snow ? "snow" : "rain";

        const char* hour_label = forecast_hours[first_precip_idx].hour_string;
        snprintf(slice0_text, sizeof(slice0_text), "Possible %s at %s", precip_word, hour_label);

        // Find when the contiguous rain block ends
        uint8_t stop_idx = first_precip_idx + 1;
        while (stop_idx < total_hours && prv_is_precip_condition(forecast_hours[stop_idx].conditions_icon)) {
            stop_idx++;
        }

        time_t rain_block_end = cur_hour_start + (time_t)(stop_idx - now_idx) * interval_hours * 3600;
        time_t slice0_exp = (rain_block_end > now) ? rain_block_end : (now + 60);

        prv_add_slice(session, limit, &added, slice0_text,
                      PUBLISHED_ID_PRECIPITATION_EXPECTED, slice0_exp);

        // If there is a non-rainy or snowy hour further in the forecast, add "No precipitation"
        if (stop_idx < total_hours) {
            snprintf(slice1_text, sizeof(slice1_text), "No precipitation");
            prv_add_slice(session, limit, &added, slice1_text,
                          PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
        }
    } else {
        // Precipitation detected in the precipitation package
        bool is_snow = (precipitation.precipitation_type == 3 || precipitation.precipitation_type == 4);
        const char* precip_word = is_snow ? "Snow" : "Rain";

        // Check if the next hour in forecast is rain or snow
        bool next_hour_is_precip = false;
        if (now_idx + 1 < total_hours) {
            next_hour_is_precip = prv_is_precip_condition(forecast_hours[now_idx + 1].conditions_icon);
        }

        // Compute stopping hour information if precipitation continues into next hours
        uint8_t stop_idx = now_idx + 1;
        while (stop_idx < total_hours && prv_is_precip_condition(forecast_hours[stop_idx].conditions_icon)) {
            stop_idx++;
        }

        char stop_hour_str[8];
        if (stop_idx < total_hours) {
            strncpy(stop_hour_str, forecast_hours[stop_idx].hour_string, sizeof(stop_hour_str) - 1);
            stop_hour_str[sizeof(stop_hour_str) - 1] = '\0';
        } else {
            time_t stop_time = cur_hour_start + (time_t)(stop_idx - now_idx) * interval_hours * 3600;
            prv_format_hour_for_time(stop_time, stop_hour_str, sizeof(stop_hour_str));
        }

        time_t rain_end_time = cur_hour_start + (time_t)(stop_idx - now_idx) * interval_hours * 3600;
        if (rain_end_time <= now) {
            rain_end_time = now + 60;
        }

        if (precip_start == 0) {
            // Precipitation is active right now
            if (!next_hour_is_precip) {
                // Case 3: {Rain|Snow} for {mm}m
                int mm = precip_end * 5;
                if (mm <= 0) mm = 5;
                snprintf(slice0_text, sizeof(slice0_text), "%s for %dm", precip_word, mm);

                time_t slice0_exp = now + mm * 60;
                prv_add_slice(session, limit, &added, slice0_text,
                              PUBLISHED_ID_PRECIPITATION_ACTIVE, slice0_exp);

                // Follow-up slice for "No precipitation"
                snprintf(slice1_text, sizeof(slice1_text), "No precipitation");
                prv_add_slice(session, limit, &added, slice1_text,
                              PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
            } else {
                // Case 4: {Rain|Snow} 'til {h}
                snprintf(slice0_text, sizeof(slice0_text), "%s 'til %s", precip_word, stop_hour_str);

                prv_add_slice(session, limit, &added, slice0_text,
                              PUBLISHED_ID_PRECIPITATION_ACTIVE, rain_end_time);

                // If there is a non-rainy or snowy hour further in the forecast, add "No precipitation"
                if (stop_idx < total_hours) {
                    snprintf(slice1_text, sizeof(slice1_text), "No precipitation");
                    prv_add_slice(session, limit, &added, slice1_text,
                                  PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
                }
            }
        } else {
            // Precipitation starts later in the package window
            time_t start_time = now + (time_t)(precip_start * 5 * 60);
            char start_time_str[10];
            prv_format_clock_time(start_time, start_time_str, sizeof(start_time_str));

            // Initial slice before precipitation begins
            snprintf(slice0_text, sizeof(slice0_text), "%s starting at %s", precip_word, start_time_str);
            time_t slice0_exp = (start_time > now) ? start_time : (now + 60);

            prv_add_slice(session, limit, &added, slice0_text,
                          PUBLISHED_ID_PRECIPITATION_EXPECTED, slice0_exp);

            // Followed by stopping glance after starting minute
            if (!next_hour_is_precip) {
                // Precise precipitation stopping glance: {Rain|Snow} for {mm}m
                int mm = (precip_end - precip_start) * 5;
                if (mm <= 0) mm = 5;
                snprintf(slice1_text, sizeof(slice1_text), "%s for %dm", precip_word, mm);

                time_t slice1_exp = start_time + (time_t)(mm * 60);
                prv_add_slice(session, limit, &added, slice1_text,
                              PUBLISHED_ID_PRECIPITATION_ACTIVE, slice1_exp);

                // Follow-up slice for "No precipitation"
                snprintf(slice2_text, sizeof(slice2_text), "No precipitation");
                prv_add_slice(session, limit, &added, slice2_text,
                              PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
            } else {
                // General forecast stopping glance: {Rain|Snow} 'til {h}
                snprintf(slice1_text, sizeof(slice1_text), "%s 'til %s", precip_word, stop_hour_str);

                prv_add_slice(session, limit, &added, slice1_text,
                              PUBLISHED_ID_PRECIPITATION_ACTIVE, rain_end_time);

                // If there is a non-rainy or snowy hour further in the forecast, add "No precipitation"
                if (stop_idx < total_hours) {
                    snprintf(slice2_text, sizeof(slice2_text), "No precipitation");
                    prv_add_slice(session, limit, &added, slice2_text,
                                  PUBLISHED_ID_PRECIPITATION_NONE, APP_GLANCE_SLICE_NO_EXPIRATION);
                }
            }
        }
    }
}

#endif // !PBL_PLATFORM_APLITE

void glance_update(void) {
#ifndef PBL_PLATFORM_APLITE
    app_glance_reload(prv_glance_reload_callback, NULL);
#endif
}
