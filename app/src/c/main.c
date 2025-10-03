#include <pebble.h>
#include "utils/utils_common.h"
#include "utils/weather.h"
#include "utils/msgproc.h"
#include "utils/prefs.h"
#include "utils/demo.h"
#include "utils/scheduler.h"
#include "gfx/windows/viewer.h"
#include "gfx/windows/splash.h"

#define APP_MESSAGE_INBOX_SIZE 1024
#define MAX_STRING_LENGTH 64

static Window* s_splash_window;
static Window* s_viewer_window;

uint8_t received_hours = 0;



// Forward declarations
static void handle_data_request(void);
static void handle_midnight_wakeup(uint32_t wakeup_id, int32_t cookie);

// Splash completion handler
static void splash_completion_handler(bool success) {
  if (success) {
    UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "Splash loading successful, transitioning to viewer");
    
    // Create and show the viewer window
    s_viewer_window = viewer_window_create();
    viewer_set_data_request_callback(handle_data_request);
    
    // Push viewer window and pop splash
    window_stack_push(s_viewer_window, true);
    
    // Update viewer with initial data
    viewer_update_view(0, 0);  // Start at hour 0, conditions page

    // Schedule midnight wakeup now that app is ready
    SchedulerStatus schedule_status = scheduler_schedule_midnight_wakeup();
    if (schedule_status == SCHEDULER_SUCCESS) {
      UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Successfully scheduled midnight wakeup");
    } else {
      UTIL_LOG(UTIL_LOG_LEVEL_WARNING, "Failed to schedule midnight wakeup: %d", schedule_status);
    }

    // Remove splash window after transition
    app_timer_register(500, (AppTimerCallback)window_stack_remove, s_splash_window);
  } else {
    UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Splash loading failed");
    // Splash window stays visible with error message
    // User can try again by restarting the app
  }
}

// Midnight wakeup handler
static void handle_midnight_wakeup(uint32_t wakeup_id, int32_t cookie) {
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Midnight wakeup occurred, refreshing weather data");

  // Reset received hours counter for fresh data
  received_hours = 0;

  // Trigger weather data refresh by calling the data request handler
  // This will fetch new weather data and update the display
  if (s_viewer_window) {
    handle_data_request();
  }
}

// Data request handler for viewer
static void handle_data_request(void) {
  if(DEMO_MODE) {
    demo_populate_forecast_hours();
    demo_populate_precipitation();
    viewer_update_view(viewer_get_current_hour(), viewer_get_current_page());
    return;
  }

  // In the new flow, we don't send requests - data comes automatically from PebbleKit JS
  // Just update the view with current data
  viewer_update_view(viewer_get_current_hour(), viewer_get_current_page());
}







static void hour_response_callback(DictionaryIterator *iter, void *context) {
  // Process individual hour packages as they arrive sequentially
  for (int i = 0; i < 12; i++) {
    Tuple* hour_package_tuple = dict_find(iter, MESSAGE_KEY_HOUR_PACKAGE + i);
    if (hour_package_tuple) {
      unpack_hour_package((HourPackage)hour_package_tuple->value->data, &forecast_hours[i]);
      received_hours++;
      UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "Received hour %d data (total: %d/12)", i, received_hours);
      
      // Update view when we have complete data
      if (received_hours >= 12) {
        UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "All hourly data received, updating view");
        viewer_update_view(viewer_get_current_hour(), viewer_get_current_page());
      }
    }
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
    UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "Received JSReady signal from PebbleKit JS");
    return;
  }

  // Check for error responses
  Tuple* response_data_tuple = dict_find(iter, MESSAGE_KEY_RESPONSE_DATA);
  if (response_data_tuple) {
    int16_t response_data = response_data_tuple->value->int32;
    UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "Response data: %d", response_data);

    if(response_data == 2) {
      UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Location error - unable to get current location");
      // TODO: Show location error screen or notification
    } else if(response_data == 1) {
      UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Weather data fetch failed");
      // TODO: Show error screen or something
    }
    return; // Don't process other data in this message
  }

  // Process hourly data
  hour_response_callback(iter, context);

  // Handle precipitation data
  Tuple* precipitation_package_tuple = dict_find(iter, MESSAGE_KEY_PRECIPITATION_PACKAGE);
  if(precipitation_package_tuple) {
    unpack_precipitation((PrecipitationPackage)precipitation_package_tuple->value->data, &precipitation);
    viewer_update_view(viewer_get_current_hour(), viewer_get_current_page());
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
    settings->refresh_interval = refresh_interval_tuple->value->int32;
  }

  bool self_refresh_changed = false;
  if(self_refresh_tuple) {
    bool old_self_refresh = settings->self_refresh;
    settings->self_refresh = self_refresh_tuple->value->int16 != 0;
    self_refresh_changed = (old_self_refresh != settings->self_refresh);
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

  // If self-refresh setting changed, update wakeup scheduling accordingly
  if (self_refresh_changed) {
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Self-refresh setting changed, updating wakeup scheduling");
    scheduler_cancel_wakeups_if_disabled();
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  // A message was received, but had to be dropped
  UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Message dropped. Reason: %d", (int)reason);
}

static void outbox_sent_callback(DictionaryIterator *iter, void *context) {
  // The message just sent has been successfully delivered

}

static void outbox_failed_callback(DictionaryIterator *iter,
                                      AppMessageResult reason, void *context) {
  // The message just sent failed to be delivered
  UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Message send failed. Reason: %d", (int)reason);
}














static void prv_init(void) {
  prefs_load();

  // Initialize the scheduler service
  scheduler_init();
  scheduler_set_wakeup_handler(handle_midnight_wakeup);

  // Create and show the splash window first
  s_splash_window = splash_window_create();
  splash_set_completion_callback(splash_completion_handler);
  splash_set_status_text("Starting up...");
  
  const bool animated = true;
  window_stack_push(s_splash_window, animated);

  // Register to be notified about inbox received events
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  app_message_open(APP_MESSAGE_INBOX_SIZE_MINIMUM, APP_MESSAGE_INBOX_SIZE);

  UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "inbox size: %lu", (uint32_t)app_message_inbox_size_maximum());
  
  // Start loading weather data
  splash_start_loading();
}

static void prv_deinit(void) {
  // Clean up both windows
  if (s_viewer_window) {
    viewer_window_destroy(s_viewer_window);
  }
  if (s_splash_window) {
    splash_window_destroy(s_splash_window);
  }

  // Deinitialize the scheduler service
  scheduler_deinit();
}

int main(void) {
  prv_init();

  UTIL_LOG(UTIL_LOG_LEVEL_DEBUG, "Done initializing, pushed splash window: %p", s_splash_window);

  app_event_loop();
  prv_deinit();
}
