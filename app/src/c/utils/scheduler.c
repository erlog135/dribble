#include "scheduler.h"
#include "utils_common.h"
#include "weather.h"
#include "prefs.h"
#include <pebble.h>

// Static variables to track scheduled wakeups
static WakeupId s_scheduled_wakeup_id = 0;
static bool s_scheduler_initialized = false;
static void (*s_custom_wakeup_handler)(uint32_t wakeup_id, int32_t cookie) = NULL;

// Forward declaration
static time_t prv_get_next_midnight_timestamp(void);

/**
 * @brief Get the timestamp for the next midnight
 * @return time_t timestamp for next midnight, or 0 if error
 */
static time_t prv_get_next_midnight_timestamp(void) {
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);

  if (!current_time) {
    UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Failed to get current time");
    return 0;
  }

  // Set time to midnight today
  struct tm midnight = *current_time;
  midnight.tm_hour = 0;
  midnight.tm_min = 0;
  midnight.tm_sec = 0;

  // If it's already past midnight, schedule for tomorrow
  if (now >= mktime(&midnight)) {
    midnight.tm_mday += 1;
  }

  time_t midnight_timestamp = mktime(&midnight);
  if (midnight_timestamp == (time_t)-1) {
    UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Failed to calculate midnight timestamp");
    return 0;
  }

  return midnight_timestamp;
}

/**
 * @brief Wakeup service handler
 * @param wakeup_id The ID of the wakeup that occurred
 * @param cookie The cookie value passed during scheduling
 */
static void prv_wakeup_handler(WakeupId wakeup_id, int32_t cookie) {
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "App woken up by scheduled wakeup (ID: %d, cookie: %d)",
           (int)wakeup_id, (int)cookie);

  // Handle different types of wakeups based on cookie value
  if (cookie == 0) {
    // Midnight wakeup - refresh weather data and reschedule next midnight
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Midnight wakeup - refreshing weather data");

    // Call custom handler if set, otherwise call default handler
    if (s_custom_wakeup_handler) {
      s_custom_wakeup_handler((uint32_t)wakeup_id, cookie);
    } else {
      scheduler_handle_wakeup((uint32_t)wakeup_id, cookie);
    }

    // Reschedule for next midnight
    SchedulerStatus status = scheduler_schedule_midnight_wakeup();
    if (status != SCHEDULER_SUCCESS) {
      UTIL_LOG(UTIL_LOG_LEVEL_WARNING, "Failed to reschedule next midnight wakeup: %d", status);
    }
  } else {
    // Precipitation wakeup - cookie contains precipitation type
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Precipitation wakeup - type: %d (%s)",
             (int)cookie, get_precipitation_string(cookie));

    // Call custom handler if set, otherwise call default handler
    if (s_custom_wakeup_handler) {
      s_custom_wakeup_handler((uint32_t)wakeup_id, cookie);
    } else {
      scheduler_handle_wakeup((uint32_t)wakeup_id, cookie);
    }

    // Note: Precipitation wakeups are one-time and don't reschedule themselves
  }
}

// Public function implementations

SchedulerStatus scheduler_schedule_midnight_wakeup(void) {
  if (!s_scheduler_initialized) {
    UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Scheduler not initialized");
    return SCHEDULER_ERROR_UNKNOWN;
  }

  // Check if self-refresh is enabled
  ClaySettings* settings = prefs_get_settings();
  if (!settings->self_refresh) {
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Self-refresh disabled, not scheduling midnight wakeup");
    // Cancel any existing midnight wakeup
    if (s_scheduled_wakeup_id > 0) {
      wakeup_cancel(s_scheduled_wakeup_id);
      s_scheduled_wakeup_id = 0;
      UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Cancelled existing midnight wakeup due to self-refresh disabled");
    }
    return SCHEDULER_SUCCESS;
  }

  time_t midnight_timestamp = prv_get_next_midnight_timestamp();
  if (midnight_timestamp == 0) {
    return SCHEDULER_ERROR_INVALID_TIME;
  }

  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Scheduling wakeup for next midnight: %s", ctime(&midnight_timestamp));

  // Use cookie = 0 to identify midnight wakeups
  return scheduler_schedule_wakeup_at_time(midnight_timestamp, 0);
}

void scheduler_handle_wakeup(uint32_t wakeup_id, int32_t cookie) {
  // Default implementation - can be overridden by app-specific logic
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Handling wakeup event (ID: %d, cookie: %d)",
           (int)wakeup_id, (int)cookie);

  // Add any app-specific wakeup handling here
  // For example: refresh weather data, update display, etc.
}

void scheduler_cancel_all_wakeups(void) {
  if (s_scheduled_wakeup_id > 0) {
    wakeup_cancel(s_scheduled_wakeup_id);
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Cancelled scheduled wakeup ID: %d", (int)s_scheduled_wakeup_id);
    s_scheduled_wakeup_id = 0;
  }
}

void scheduler_init(void) {
  if (s_scheduler_initialized) {
    UTIL_LOG(UTIL_LOG_LEVEL_WARNING, "Scheduler already initialized");
    return;
  }

  // Subscribe to wakeup events
  wakeup_service_subscribe(prv_wakeup_handler);

  // Check if app was launched due to a wakeup
  WakeupId wakeup_id = 0;
  int32_t cookie = 0;
  if (wakeup_get_launch_event(&wakeup_id, &cookie)) {
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "App launched due to wakeup (ID: %d, cookie: %d)",
             (int)wakeup_id, (int)cookie);
    prv_wakeup_handler(wakeup_id, cookie);
  }

  // Cancel any existing wakeups if self-refresh is disabled
  scheduler_cancel_wakeups_if_disabled();

  s_scheduler_initialized = true;
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Scheduler initialized");
}

void scheduler_deinit(void) {
  if (!s_scheduler_initialized) {
    return;
  }

  // Cancel any scheduled wakeups
  scheduler_cancel_all_wakeups();

  // Note: wakeup_service_unsubscribe() is not available in Pebble API
  // The wakeup service subscription remains active until app termination

  s_scheduler_initialized = false;
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Scheduler deinitialized");
}

void scheduler_set_wakeup_handler(void (*handler)(uint32_t wakeup_id, int32_t cookie)) {
  s_custom_wakeup_handler = handler;
  UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Custom wakeup handler set: %p", handler);
}

void scheduler_cancel_wakeups_if_disabled(void) {
  if (!s_scheduler_initialized) {
    return;
  }

  ClaySettings* settings = prefs_get_settings();
  if (!settings->self_refresh) {
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Self-refresh disabled, cancelling scheduled wakeups");

    // Cancel the main midnight wakeup if it exists
    if (s_scheduled_wakeup_id > 0) {
      wakeup_cancel(s_scheduled_wakeup_id);
      UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Cancelled midnight wakeup ID: %d", (int)s_scheduled_wakeup_id);
      s_scheduled_wakeup_id = 0;
    }

    // Note: Precipitation wakeups will expire naturally since we can't easily
    // enumerate all scheduled wakeups in Pebble API
  }
}

SchedulerStatus scheduler_schedule_wakeup_at_time(time_t target_time, int32_t cookie) {
  if (!s_scheduler_initialized) {
    UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Scheduler not initialized");
    return SCHEDULER_ERROR_UNKNOWN;
  }

  // Check if self-refresh is enabled
  ClaySettings* settings = prefs_get_settings();
  if (!settings->self_refresh) {
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Self-refresh disabled, not scheduling wakeup");
    return SCHEDULER_SUCCESS; // Return success to avoid error handling
  }

  // Schedule the wakeup (don't cancel existing ones for precipitation scheduling)
  WakeupId wakeup_id = wakeup_schedule(target_time, cookie, true);

  if (wakeup_id > 0) {
    // For precipitation wakeups (cookie > 0), don't update s_scheduled_wakeup_id
    // This allows multiple precipitation wakeups to be scheduled
    if (cookie == 0) {
      // Only update for midnight wakeups (cookie = 0)
      s_scheduled_wakeup_id = wakeup_id;
    }
    UTIL_LOG(UTIL_LOG_LEVEL_INFO, "Successfully scheduled wakeup for %s (cookie: %d)",
             ctime(&target_time), (int)cookie);
    return SCHEDULER_SUCCESS;
  }

  // Handle different error conditions
  switch (wakeup_id) {
    case E_INVALID_ARGUMENT:
      UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Invalid argument when scheduling wakeup");
      return SCHEDULER_ERROR_INVALID_TIME;

    case E_OUT_OF_MEMORY:
      UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Out of memory when scheduling wakeup");
      return SCHEDULER_ERROR_UNKNOWN;

    case E_INVALID_OPERATION:
      UTIL_LOG(UTIL_LOG_LEVEL_WARNING, "Invalid operation - too soon to schedule or max wakeups reached");
      return SCHEDULER_ERROR_TOO_SOON;

    default:
      UTIL_LOG(UTIL_LOG_LEVEL_ERROR, "Unknown error scheduling wakeup: %d", (int)wakeup_id);
      return SCHEDULER_ERROR_UNKNOWN;
  }
}
