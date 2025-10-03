#pragma once

#include <pebble.h>

// Scheduler constants
#define SCHEDULER_RETRY_INCREMENT_MINUTES 5
#define SCHEDULER_MAX_RETRY_ATTEMPTS 12  // 5 minutes * 12 = 1 hour max retry window

// Scheduler status codes
typedef enum {
  SCHEDULER_SUCCESS = 0,
  SCHEDULER_ERROR_INVALID_TIME = -1,
  SCHEDULER_ERROR_TOO_SOON = -2,
  SCHEDULER_ERROR_MAX_WAKEUPS = -3,
  SCHEDULER_ERROR_UNKNOWN = -4
} SchedulerStatus;

// Function declarations

/**
 * @brief Schedule the app to wake up at midnight every day
 * @return Status code indicating success or failure
 */
SchedulerStatus scheduler_schedule_midnight_wakeup(void);

/**
 * @brief Handle wakeup event when app is launched due to scheduled wakeup
 * @param wakeup_id The ID of the wakeup that triggered this launch
 * @param cookie The cookie value passed when scheduling
 */
void scheduler_handle_wakeup(uint32_t wakeup_id, int32_t cookie);

/**
 * @brief Cancel any scheduled wakeups
 */
void scheduler_cancel_all_wakeups(void);

/**
 * @brief Initialize the scheduler service
 * Should be called during app initialization
 */
void scheduler_init(void);

/**
 * @brief Deinitialize the scheduler service
 * Should be called during app cleanup
 */
void scheduler_deinit(void);

/**
 * @brief Set a custom wakeup handler function
 * @param handler Function pointer to custom wakeup handler
 * If not set, the default handler will be used
 */
void scheduler_set_wakeup_handler(void (*handler)(uint32_t wakeup_id, int32_t cookie));

/**
 * @brief Schedule a wakeup at a specific time
 * @param target_time The target timestamp to wake up
 * @param cookie A value passed to the wakeup handler
 * @return Status code indicating success or failure
 */
SchedulerStatus scheduler_schedule_wakeup_at_time(time_t target_time, int32_t cookie);

/**
 * @brief Cancel all scheduled wakeups if self-refresh is disabled
 * Should be called when settings change or during initialization
 */
void scheduler_cancel_wakeups_if_disabled(void);
