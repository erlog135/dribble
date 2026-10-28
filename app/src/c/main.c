#include <pebble.h>
#include "utils/utils_common.h"
#include "utils/weather.h"
#include "utils/msgproc.h"
#include "utils/prefs.h"
#include "utils/cache.h"
#include "utils/demo.h"
#include "gfx/windows/viewer.h"
#include "gfx/windows/splash.h"
#include "utils/glance.h"

#define APP_MESSAGE_INBOX_SIZE 512
#define MAX_STRING_LENGTH 40

#define RETRIEVING_STATUS_TIMEOUT_MS 30000
#define ERROR_STATUS_DURATION_MS 4000
#define RELOAD_RETRY_MS 400
#define RELOAD_MAX_RETRIES 10

static Window* s_splash_window;
static Window* s_viewer_window;

// Refresh state while the viewer is showing (possibly cached) data
static bool s_refresh_pending = false;          // hour data received, awaiting precipitation
static char s_prev_hour_string[6] = "";         // hour being viewed before the refresh
static uint8_t s_prev_page = 0;
static AppTimer* s_status_timer = NULL;
static AppTimer* s_reload_timer = NULL;
static uint8_t s_reload_hour = 0;

static bool s_js_ready = false;
static bool s_data_fetch_needed = false;
static AppTimer* s_request_timer = NULL;

static void request_timer_callback(void* context);

static void trigger_weather_request(void) {
  if (s_request_timer) {
    app_timer_cancel(s_request_timer);
    s_request_timer = NULL;
  }
  if (!weather_request_data()) {
    s_request_timer = app_timer_register(1000, request_timer_callback, NULL);
  }
}

static void request_timer_callback(void* context) {
  s_request_timer = NULL;
  if (s_data_fetch_needed && s_js_ready) {
    trigger_weather_request();
  }
}

static void status_clear_callback(void* context) {
  s_status_timer = NULL;
  viewer_set_status(NULL);
}

// Shows `text` under the status bar and clears it after `duration_ms`
static void show_status(const char* text, uint32_t duration_ms) {
  viewer_set_status(text);
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }
  s_status_timer = app_timer_register(duration_ms, status_clear_callback, NULL);
}

static void reload_viewer_callback(void* context) {
  s_reload_timer = NULL;
  int attempt = (int)(intptr_t)context;
  if (!viewer_reload(s_reload_hour, s_prev_page) && attempt < RELOAD_MAX_RETRIES) {
    s_reload_timer = app_timer_register(RELOAD_RETRY_MS, reload_viewer_callback, (void*)(intptr_t)(attempt + 1));
  }
}

// New hour + precipitation data has fully arrived while the viewer is open
static void finish_viewer_refresh(void) {
  s_refresh_pending = false;

  // Stay on the same hour if it still exists, otherwise fall back to the current hour
  uint8_t target = forecast_now_index;
  for (uint8_t i = 0; i < forecast_hour_count; i++) {
    if (strcmp(forecast_hours[i].hour_string, s_prev_hour_string) == 0) {
      target = i;
      break;
    }
  }
  s_reload_hour = target;

  vibes_short_pulse();
  viewer_set_status(NULL);
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
    s_status_timer = NULL;
  }

  if (s_reload_timer) {
    app_timer_cancel(s_reload_timer);
    s_reload_timer = NULL;
  }
  reload_viewer_callback((void*)0);
}

// Splash completion handler
static void splash_completion_handler(bool success) {
  if (success) {
    UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Splash loading successful, transitioning to viewer");

    s_viewer_window = viewer_window_create();
    window_stack_push(s_viewer_window, true);
    vibes_short_pulse();

    viewer_update_view(0, 0);  // Start at hour 0, conditions page

    // Remove splash window after transition
    app_timer_register(500, (AppTimerCallback)window_stack_remove, s_splash_window);
  } else {
    UTIL_LOG(APP_LOG_LEVEL_ERROR, "Splash loading failed");
    // Splash window stays visible with error message; user restarts the app to retry
  }
}

static void inbox_received_callback(DictionaryIterator *iter, void *context) {
  
  // If splash window is active, let it handle the message
  if (s_splash_window && window_stack_contains_window(s_splash_window)) {
    splash_handle_inbox_message(iter);
    return;
  }
  
  // Check for JSReady signal from PebbleKit JS
  Tuple* js_ready_tuple = dict_find(iter, MESSAGE_KEY_JS_READY);
  if (js_ready_tuple) {
    UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Received JSReady signal from PebbleKit JS");
    s_js_ready = true;
    if (s_data_fetch_needed) {
      show_status("Retrieving...", RETRIEVING_STATUS_TIMEOUT_MS);
      trigger_weather_request();
    }
    return;
  }

  // Check for error responses
  Tuple* response_data_tuple = dict_find(iter, MESSAGE_KEY_RESPONSE_DATA);
  if (response_data_tuple) {
    int16_t response_data = response_data_tuple->value->int32;
    UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Response data: %d", response_data);

    if(response_data == 2) {
      UTIL_LOG(APP_LOG_LEVEL_ERROR, "Location error - unable to get current location");
      show_status("Location error", ERROR_STATUS_DURATION_MS);
    } else if(response_data == 1) {
      UTIL_LOG(APP_LOG_LEVEL_ERROR, "Weather data fetch failed");
      show_status("Data error", ERROR_STATUS_DURATION_MS);
    }
    return; // Don't process other data in this message
  }

  // Handle hourly data (refresh while the viewer is open)
  Tuple* hour_data_tuple = dict_find(iter, MESSAGE_KEY_HOUR_DATA);
  if (hour_data_tuple) {
    // Remember what the user was looking at, to restore it after the refresh
    uint8_t cur = viewer_get_current_hour();
    strncpy(s_prev_hour_string, forecast_hours[cur].hour_string, sizeof(s_prev_hour_string) - 1);
    s_prev_hour_string[sizeof(s_prev_hour_string) - 1] = '\0';
    s_prev_page = viewer_get_current_page();

    forecast_hour_count = unpack_all_hours((uint8_t*)hour_data_tuple->value->data, forecast_hours);
    forecast_now_index = 0;
    cache_save_hours((uint8_t*)hour_data_tuple->value->data);
    s_refresh_pending = true;
    s_data_fetch_needed = false;
    if (s_request_timer) {
      app_timer_cancel(s_request_timer);
      s_request_timer = NULL;
    }
  }

  // Handle precipitation data
  Tuple* precipitation_package_tuple = dict_find(iter, MESSAGE_KEY_PRECIPITATION_PACKAGE);
  if(precipitation_package_tuple) {
    unpack_precipitation((PrecipitationPackage)precipitation_package_tuple->value->data, &precipitation);
    cache_save_precip((uint8_t*)precipitation_package_tuple->value->data);
    if (s_refresh_pending) {
      finish_viewer_refresh();
    } else {
      viewer_update_view(viewer_get_current_hour(), viewer_get_current_page());
    }
    glance_update();
  }

  // ************************SETTINGS***********************

  Tuple* temperature_units_tuple = dict_find(iter, MESSAGE_KEY_CFG_TEMPERATURE_UNITS);
  Tuple* velocity_units_tuple = dict_find(iter, MESSAGE_KEY_CFG_VELOCITY_UNITS);
  Tuple* distance_units_tuple = dict_find(iter, MESSAGE_KEY_CFG_DISTANCE_UNITS);
  Tuple* pressure_units_tuple = dict_find(iter, MESSAGE_KEY_CFG_PRESSURE_UNITS);
  Tuple* refresh_interval_tuple = dict_find(iter, MESSAGE_KEY_CFG_REFRESH_INTERVAL);
  Tuple* self_refresh_tuple = dict_find(iter, MESSAGE_KEY_CFG_SELF_REFRESH);
  Tuple* display_interval_tuple = dict_find(iter, MESSAGE_KEY_CFG_DISPLAY_INTERVAL);
  Tuple* animate_tuple = dict_find(iter, MESSAGE_KEY_CFG_ANIMATE);
  Tuple* wind_vane_direction_tuple = dict_find(iter, MESSAGE_KEY_CFG_WIND_VANE_DIRECTION);

  ClaySettings* settings = prefs_get_settings();

  if(temperature_units_tuple) {
    strcpy(settings->temperature_units, temperature_units_tuple->value->cstring);
  }

  if(velocity_units_tuple) {
    strcpy(settings->velocity_units, velocity_units_tuple->value->cstring);
  }

  if(distance_units_tuple) {
    strcpy(settings->distance_units, distance_units_tuple->value->cstring);
  }

  if(pressure_units_tuple) {
    strcpy(settings->pressure_units, pressure_units_tuple->value->cstring);
  }

  if(refresh_interval_tuple) {
    int32_t v = (refresh_interval_tuple->type == TUPLE_CSTRING)
        ? atoi(refresh_interval_tuple->value->cstring)
        : refresh_interval_tuple->value->int32;
    settings->refresh_interval = (v > 0 && v <= 60) ? v : 30;
  }

  if(self_refresh_tuple) {
    settings->self_refresh = self_refresh_tuple->value->int16 != 0;
  }

  if(display_interval_tuple) {
    settings->display_interval = display_interval_tuple->value->int32;
  }

  if(animate_tuple) {
    settings->animate = animate_tuple->value->int16 != 0;
  }

  if(wind_vane_direction_tuple) {
    // Handle both string and integer values for backward compatibility
    if(wind_vane_direction_tuple->type == TUPLE_CSTRING) {
      settings->wind_vane_direction = atoi(wind_vane_direction_tuple->value->cstring);
    } else {
      settings->wind_vane_direction = wind_vane_direction_tuple->value->int16;
    }
  }


  prefs_save();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  // A message was received, but had to be dropped
  UTIL_LOG(APP_LOG_LEVEL_ERROR, "Message dropped. Reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iter, void *context) {
  // The message just sent has been successfully delivered

}

static void outbox_failed_callback(DictionaryIterator *iter,
                                      AppMessageResult reason, void *context) {
  // The message just sent failed to be delivered
  UTIL_LOG(APP_LOG_LEVEL_ERROR, "Message send failed. Reason: %d", (int)reason);
  if (s_data_fetch_needed && !s_request_timer) {
    s_request_timer = app_timer_register(2000, request_timer_callback, NULL);
  }
}


static void prv_init(void) {
  prefs_load();

  // Register to be notified about inbox received events
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_INBOX_SIZE);

  UTIL_LOG(APP_LOG_LEVEL_DEBUG, "inbox size: %lu", (uint32_t)app_message_inbox_size_maximum());

  // If we have usable cached weather, go straight to the viewer.
  if (!DEMO_MODE && cache_load() > 0) {
    UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Showing cached weather");
    s_viewer_window = viewer_window_create();
    window_stack_push(s_viewer_window, true);
    viewer_update_view(forecast_now_index, 0);
    glance_update();

    ClaySettings* settings = prefs_get_settings();
    uint32_t max_age = (settings->refresh_interval > 0 ? settings->refresh_interval : 30) * 60;
    if (cache_is_fresh(max_age)) {
      UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Cache is fresh within refresh_interval (%ld min), not requesting new data",
               (long)(settings->refresh_interval));
      s_data_fetch_needed = false;
    } else {
      UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Cache is older than refresh_interval, requesting fresh data");
      s_data_fetch_needed = true;
      show_status("Retrieving...", RETRIEVING_STATUS_TIMEOUT_MS);
      if (s_js_ready) {
        trigger_weather_request();
      }
    }
    return;
  }

  // No usable cache: show the splash window while data is fetched
  s_data_fetch_needed = true;
  s_splash_window = splash_window_create();
  splash_set_completion_callback(splash_completion_handler);
  splash_set_status_text("Starting up...");

  const bool animated = true;
  window_stack_push(s_splash_window, animated);

  // Start loading weather data
  splash_start_loading();
}

static void prv_deinit(void) {
  glance_update();

  if (s_request_timer) {
    app_timer_cancel(s_request_timer);
    s_request_timer = NULL;
  }
  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
    s_status_timer = NULL;
  }
  if (s_reload_timer) {
    app_timer_cancel(s_reload_timer);
    s_reload_timer = NULL;
  }

  // Clean up both windows
  if (s_viewer_window) {
    viewer_window_destroy(s_viewer_window);
  }
  if (s_splash_window) {
    splash_window_destroy(s_splash_window);
  }
}

int main(void) {
  prv_init();

  UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed splash window: %p", s_splash_window);

  app_event_loop();
  prv_deinit();
}
