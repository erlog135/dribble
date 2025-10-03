#pragma once

#include <pebble.h>

// Conditional logging for utils
// Uncomment the line below to enable utils debug logging
// #define UTILS_LOGGING

#ifdef UTILS_LOGGING
  #define UTIL_LOG(level, fmt, ...) APP_LOG(level, fmt, ##__VA_ARGS__)
#else
  #define UTIL_LOG(level, fmt, ...)
#endif
