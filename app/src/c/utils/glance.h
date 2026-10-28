/*
    App Glance Utility
    
    Generates App Glance slices summarizing precipitation for the watch launcher.
    
    Glance scenarios:
    - "No precipitation" (Icon: PRECIPITATION_NONE)
      When no precipitation is detected now or throughout the forecast period.
    - "Possible {rain|snow} at {hour}" (Icon: PRECIPITATION_EXPECTED)
      When no precipitation in package, but future forecast hour(s) have rain/snow.
      Followed by "No precipitation" (Icon: PRECIPITATION_NONE) once the rain block ends.
    - "{Rain|Snow} starting at {hh:mm}" (Icon: PRECIPITATION_EXPECTED)
      When precipitation package indicates rain/snow starting later in the 2-hour window.
      Followed by either "{Rain|Snow} for {mm}m" or "{Rain|Snow} 'til {h}" (Icon: PRECIPITATION_ACTIVE)
      after the starting minute, and then "No precipitation" (Icon: PRECIPITATION_NONE).
    - "{Rain|Snow} for {mm}m" (Icon: PRECIPITATION_ACTIVE)
      When precipitation is happening now, but the next forecast hour is dry.
      Followed by "No precipitation" (Icon: PRECIPITATION_NONE).
    - "{Rain|Snow} 'til {h}" (Icon: PRECIPITATION_ACTIVE)
      When precipitation is happening now and into subsequent forecast hours.
      {h} is the hour when the rain stops / dry hour begins.
      Followed by "No precipitation" (Icon: PRECIPITATION_NONE).

    Safe to call on all platforms; on Aplite (which does not support App Glance),
    it is a no-op.
*/

#pragma once

#include <pebble.h>

// Reloads the launcher app glance based on the current weather data.
void glance_update(void);
