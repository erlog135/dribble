#include "viewer.h"
#include "../layout.h"
#include "../resources.h"
#include "../pages/airflow.h"
#include "../pages/conditions.h"
#include "../pages/experiential.h"
#include "../animation/animation.h"
#include "../animation/text_animation.h"
#include "../animation/image_animation.h"
#include "../animation/transition.h"
#include "../animation/background_animation.h"
#include "../../utils/weather.h"
#include "../../utils/prefs.h"

// Conditional logging for viewer module
// Uncomment the line below to enable viewer debug logging
// #define VIEWER_LOGGING

#ifdef VIEWER_LOGGING
  #define VIEWER_LOG(level, fmt, ...) APP_LOG(level, fmt, ##__VA_ARGS__)
#else
  #define VIEWER_LOG(level, fmt, ...)
#endif

enum {
  VIEW_PAGE_CONDITIONS,
  VIEW_PAGE_AIRFLOW,
  VIEW_PAGE_EXPERIENTIAL,
};

// Window and UI elements
static Window* s_viewer_window;
static TextLayer* prev_time_layer;
static TextLayer* current_time_layer;
static TextLayer* next_time_layer;
static TextLayer* current_text_layer;

// Image references for the three positions
static GDrawCommandImage* prev_image;
static GDrawCommandImage* current_image;
static GDrawCommandImage* next_image;

// Layer for drawing the images
static Layer* images_layer;

// View state
static uint8_t hour_view = 0;
static uint8_t page_view = VIEW_PAGE_CONDITIONS;

// Animation state
static bool animating = false;
static bool transition_animating = false;
static bool background_animating = false;

// Data request callback
static ViewerDataRequestCallback data_request_callback = NULL;

// Forward declarations
static void update_view(uint8_t hour, uint8_t page);
static GColor get_background_color_for_forecast(uint8_t hour, uint8_t page);

// Helper function to check if animations are enabled
static bool animations_enabled(void) {
#ifdef PBL_PLATFORM_APLITE
    // Animations are not supported on Aplite platform
    return false;
#else
    ClaySettings* settings = prefs_get_settings();
    return settings->animate;
#endif
}

// Helper function to update only images and content text for animation, without updating time text
static void update_images_and_content_for_animation(uint8_t hour, uint8_t page) {
  if (hour > 11) {
    return;
  }

  if (page > 2) {
    return;
  }

  // Update only the content text (the animation system will handle this)
  const char* content_text = forecast_hours[hour].conditions_string;
  if(page == VIEW_PAGE_AIRFLOW) {
    content_text = forecast_hours[hour].airflow_string;
  } else if(page == VIEW_PAGE_EXPERIENTIAL) {
    content_text = forecast_hours[hour].experiential_string;
  }

  // For precipitation on conditions page at hour 0
  if(page == VIEW_PAGE_CONDITIONS && hour == 0 && precipitation.precipitation_type > 0) {
    content_text = precipitation.precipitation_string;
  }

  // Set the content text (but not time text)
  text_layer_set_text(current_text_layer, content_text);

  // Update page-specific image references
  if(page == VIEW_PAGE_CONDITIONS) {
    set_airflow_view(-1);
    set_experiential_view(-1);
    set_conditions_view(hour);
  } else if (page == VIEW_PAGE_AIRFLOW) {
    set_conditions_view(-1);
    set_experiential_view(-1);
    set_airflow_view(hour);
  } else if (page == VIEW_PAGE_EXPERIENTIAL) {
    set_conditions_view(-1);
    set_airflow_view(-1);
    set_experiential_view(hour);
  }

  // Mark the images layer as dirty to redraw the images
  layer_mark_dirty(images_layer);
}

// Animation completion callbacks
static void animation_complete_up(void) {
    animating = false;
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Up animation completed, current hour: %d", hour_view);
    // Update view after animation completes to ensure final state is correct
    // Only call update_view once, not from both text and image animation completions
    update_view(hour_view, page_view);
}

static void animation_complete_down(void) {
    animating = false;
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Down animation completed, current hour: %d", hour_view);
    // Update view after animation completes to ensure final state is correct
    // Only call update_view once, not from both text and image animation completions
    update_view(hour_view, page_view);
}

// Image animation completion callbacks (don't call update_view)
static void image_animation_complete_up(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Image up animation completed");
}
static void image_animation_complete_down(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Image down animation completed");
}

// Transition animation completion callback
static void transition_animation_complete(void) {
    transition_animating = false;
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation completed");
}

// Background animation completion callbacks
static void background_animation_complete_hour(void) {
    background_animating = false;
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Background hour animation completed");
}

static void background_animation_complete_page(void) {
    background_animating = false;
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Background page animation completed");
}

/**
 * @brief Draws the three images (prev, current, next) at their respective positions
 */
static void draw_page_images(GContext* ctx) {
#ifndef PBL_PLATFORM_APLITE
    // Don't draw images if the animation system is hiding them (only applies when animations enabled)
    // The animation system will handle drawing them progressively
    if (animations_enabled() && image_animation_are_images_hidden()) {
        return;
    }
#endif
    
    // Get image positions (animated or normal)
    GPoint prev_pos, current_pos, next_pos;
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
        transition_animation_get_image_positions(&prev_pos, &current_pos, &next_pos);
#endif
    } else {
        // Use normal layout positions when animations disabled
        prev_pos = layout.prev_icon_pos;
        current_pos = layout.current_icon_pos;
        next_pos = layout.next_icon_pos;
    }
    
    // Draw previous image if available
    if (prev_image) {
        // Check if this is the small axis image (precipitation case on conditions page, hour 1)
        if (page_view == VIEW_PAGE_CONDITIONS && hour_view == 1 && precipitation.precipitation_type > 0) {
            // Use special small axis position instead of normal prev position
            gdraw_command_image_draw(ctx, prev_image, layout.axis_small_pos);
        } else {
            gdraw_command_image_draw(ctx, prev_image, prev_pos);
        }
    }

    // Draw current image if available
    if (current_image) {
        // Check if this is the axis image (precipitation case on conditions page, hour 0)
        if (page_view == VIEW_PAGE_CONDITIONS && hour_view == 0 && precipitation.precipitation_type > 0) {
            // Use special axis position instead of normal current position
            gdraw_command_image_draw(ctx, current_image, layout.axis_large_pos);
        } else {
            gdraw_command_image_draw(ctx, current_image, current_pos);
        }
    }

    // Draw next image if available
    if (next_image) {
        gdraw_command_image_draw(ctx, next_image, next_pos);
    }
}

// Click handlers for navigation
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    if(animating || animation_system_is_any_active() || background_animating) {
      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Animation already active, ignoring up click");
      return;
    }
#endif
  }

  if(hour_view > 0) {
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
      // Start background animation for hour transition (up direction slides from top)
      background_animating = true;
      GColor animation_color = get_background_color_for_forecast(hour_view - 1, page_view);
      background_animation_start(BACKGROUND_ANIMATION_FROM_TOP, animation_color, background_animation_complete_hour);

      // Store current images for animation BEFORE updating view
      image_animation_store_current_images();

      // Update hour and set new images/content for animation (but not time text)
      hour_view--;
      update_images_and_content_for_animation(hour_view, page_view);

      // Start animation moving up
      animating = true;
      const char* time_text = forecast_hours[hour_view].hour_string;
      const char* content_text = text_layer_get_text(current_text_layer);

      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Starting up animation - hour: %d, time: %s", hour_view, time_text);
      text_animation_start(ANIMATION_DIRECTION_UP, hour_view, time_text, content_text, animation_complete_up);
      image_animation_start(ANIMATION_DIRECTION_UP, hour_view, page_view, image_animation_complete_up);
#endif
    } else {
      // Non-animated mode: directly update hour and view
      hour_view--;
      update_view(hour_view, page_view);

      // Simple background color change
      GColor new_color = get_background_color_for_forecast(hour_view, page_view);
      window_set_background_color(s_viewer_window, new_color);
    }
    return;
  }

  // Handle wrapping case (no animation)
  hour_view--;
  if(hour_view > 11) { // This check handles unsigned integer underflow when hour_view is 0
    hour_view = 11;
  }
  update_view(hour_view, page_view);

  // Update background color for wrap
  GColor new_color = get_background_color_for_forecast(hour_view, page_view);
  window_set_background_color(s_viewer_window, new_color);
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    if(transition_animating || transition_animation_is_active() || background_animating) {
      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Transition already active, ignoring select click");
      return;
    }
#endif
  }

  page_view++;
  if(page_view > 2) {
    page_view = 0;
  }

  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    // Start background animation for page transition (slide from right)
    background_animating = true;
    GColor animation_color = get_background_color_for_forecast(hour_view, page_view);
    background_animation_start(BACKGROUND_ANIMATION_FROM_RIGHT, animation_color, background_animation_complete_page);

    // Start transition animation
    transition_animating = true;
    transition_animation_start(transition_animation_complete);

    // Update the image animation system with the new page
    image_animation_set_current_page(page_view);
    update_view(hour_view, page_view);
#endif
  } else {
    // Non-animated mode: directly update view and background color
    update_view(hour_view, page_view);

    // Simple background color change
    GColor new_color = get_background_color_for_forecast(hour_view, page_view);
    window_set_background_color(s_viewer_window, new_color);
  }
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    if(animating || animation_system_is_any_active() || background_animating) {
      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Animation already active, ignoring down click");
      return;
    }
#endif
  }

  if(hour_view < 11) {
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
      // Start background animation for hour transition (down direction slides from bottom)
      background_animating = true;
      GColor animation_color = get_background_color_for_forecast(hour_view + 1, page_view);
      background_animation_start(BACKGROUND_ANIMATION_FROM_BOTTOM, animation_color, background_animation_complete_hour);

      // Store current images for animation BEFORE updating view
      image_animation_store_current_images();

      // Update hour and set new images/content for animation (but not time text)
      hour_view++;
      update_images_and_content_for_animation(hour_view, page_view);

      // Start animation moving down
      animating = true;
      const char* time_text = forecast_hours[hour_view].hour_string;
      const char* content_text = text_layer_get_text(current_text_layer);

      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Starting down animation - hour: %d, time: %s", hour_view, time_text);
      text_animation_start(ANIMATION_DIRECTION_DOWN, hour_view, time_text, content_text, animation_complete_down);
      image_animation_start(ANIMATION_DIRECTION_DOWN, hour_view, page_view, image_animation_complete_down);
#endif
    } else {
      // Non-animated mode: directly update hour and view
      hour_view++;
      update_view(hour_view, page_view);

      // Simple background color change
      GColor new_color = get_background_color_for_forecast(hour_view, page_view);
      window_set_background_color(s_viewer_window, new_color);
    }
    return;
  }

  // Handle wrapping case (no animation)
  hour_view = 0;
  update_view(hour_view, page_view);

  // Update background color for wrap
  GColor new_color = get_background_color_for_forecast(hour_view, page_view);
  window_set_background_color(s_viewer_window, new_color);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click_handler);
}

static void main_layer_update_proc(Layer* layer, GContext* ctx) {
  draw_page_images(ctx);
}

static void init_layers(Layer* window_layer) {
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Initializing layers");
  prev_time_layer = text_layer_create(layout.prev_time_bounds);
  text_layer_set_text(prev_time_layer, "9AM");
  text_layer_set_background_color(prev_time_layer, GColorClear);
  text_layer_set_font(prev_time_layer, fonts_get_system_font(layout.time_font_key));

  current_time_layer = text_layer_create(layout.current_time_bounds);
  text_layer_set_text(current_time_layer, "11AM");
  text_layer_set_background_color(current_time_layer, GColorClear);
  text_layer_set_font(current_time_layer, fonts_get_system_font(layout.time_font_key));

  current_text_layer = text_layer_create(layout.current_text_bounds);
  text_layer_set_text(current_text_layer, "12mph SW\n26mph gusts\n1004mb");
  text_layer_set_background_color(current_text_layer, GColorClear);
  text_layer_set_font(current_text_layer, fonts_get_system_font(layout.text_font_key));

  next_time_layer = text_layer_create(layout.next_time_bounds);
  text_layer_set_text(next_time_layer, "1PM");
  text_layer_set_background_color(next_time_layer, GColorClear);
  text_layer_set_font(next_time_layer, fonts_get_system_font(layout.time_font_key));
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Time layers initialized");
  // Register main layers for animation system (only if animations enabled)
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    text_animation_set_main_layers(current_time_layer, current_text_layer);
    text_animation_set_secondary_layers(prev_time_layer, next_time_layer);

    // Register layers for transition animation system
    transition_animation_set_layers(current_time_layer, current_text_layer, prev_time_layer, next_time_layer);
#endif
  }

  layer_add_child(window_layer, text_layer_get_layer(prev_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(current_time_layer));
  layer_add_child(window_layer, text_layer_get_layer(current_text_layer));
  layer_add_child(window_layer, text_layer_get_layer(next_time_layer));
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Time layers added to window layer");

  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Initializing conditions layers");
  Layer* conditions_layer = init_conditions_layers(window_layer, &prev_image, &current_image, &next_image);
  layer_add_child(window_layer, conditions_layer);
  
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Initializing airflow layers");
  Layer* air_layer = init_airflow_layers(window_layer, &prev_image, &current_image, &next_image);
  layer_add_child(window_layer, air_layer);
  
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Initializing experiential layers");
  Layer* exp_layer = init_experiential_layers(window_layer, &prev_image, &current_image, &next_image);
  layer_add_child(window_layer, exp_layer);
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Image layers initialized");

  // Create a separate layer for drawing images
  images_layer = layer_create(layer_get_bounds(window_layer));
  layer_set_update_proc(images_layer, main_layer_update_proc);
  layer_add_child(window_layer, images_layer);
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Images layer added to window layer");
  // Set image layers for animation system AFTER images_layer is created (only if animations enabled)
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    text_animation_set_image_layers(images_layer, &prev_image, &current_image, &next_image);

    // Set image layers for transition animation system
    transition_animation_set_image_layers(images_layer, &prev_image, &current_image, &next_image);

    // Initialize animation system
    animation_system_init();
    text_animation_init(window_layer);
    transition_animation_init(window_layer);
#endif
  }
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Layers initialized");
}

// Get background color based on forecast data and current page
static GColor get_background_color_for_forecast(uint8_t hour, uint8_t page) {
  // On black & white devices, always return white
  #ifdef PBL_BW
  return GColorWhite;
  #endif
  
  if (hour > 11) {
    return GColorWhite; // Default fallback
  }

  switch (page) {
    case VIEW_PAGE_CONDITIONS:
      // Use condition icon color, or precipitation color if applicable
      if (hour == 0 && precipitation.precipitation_type > 0) {
        // For precipitation, use condition color 4 (snow) if cold, condition color 3 (rain) if warm
        if (precipitation.precipitation_type >= 3 && precipitation.precipitation_type <= 5) {
          // Cold precipitation: snow (3), sleet (4), hail (5)
          return get_condition_color(4); // Snow color
        } else {
          // Warm precipitation: rain (2), mixed (6), generic (1)
          return get_condition_color(3); // Rain color
        }
      } else {
        return get_condition_color(forecast_hours[hour].conditions_icon);
      }
      
    case VIEW_PAGE_AIRFLOW:
      // Use airflow intensity based on wind speed resource ID
      // Extract speed level from resource ID: Slow=0, Med=1, Fast=2
      {
        uint32_t resource_id = forecast_hours[hour].wind_speed_resource_id;
        int speed_level = 0;  // Default to slow
        
        if (resource_id >= RESOURCE_ID_WIND_SPEED_MED_N && resource_id <= RESOURCE_ID_WIND_SPEED_MED_NW) {
          speed_level = 1;  // Medium
        } else if (resource_id >= RESOURCE_ID_WIND_SPEED_FAST_N && resource_id <= RESOURCE_ID_WIND_SPEED_FAST_NW) {
          speed_level = 2;  // Fast
        }
        
        return get_airflow_color(speed_level);
      }
      
    case VIEW_PAGE_EXPERIENTIAL:
      // Use experiential icon color
      return get_experiential_color(forecast_hours[hour].experiential_icon);
      
    default:
      return GColorWhite; // Default fallback
  }
}

//update the view for the given hour (index 0-11) and page enum
static void update_view(uint8_t hour, uint8_t page) {
  if (hour > 11) {
    return;
  }

  if (page > 2) {
    return;
  }

  // Handle text layers with rudimentary logic when animations are disabled
  if(animations_enabled()) {
    // Animated mode: show all time layers normally
    if(hour == 0) {
      text_layer_set_text(prev_time_layer, "");
    } else{
      text_layer_set_text(prev_time_layer, forecast_hours[hour-1].hour_string);
    }

    text_layer_set_text(current_time_layer, forecast_hours[hour].hour_string);

    if(hour == 11) {
      text_layer_set_text(next_time_layer, "");
    } else {
      text_layer_set_text(next_time_layer, forecast_hours[hour+1].hour_string);
    }
  } else {
    // Non-animated mode: rudimentary text handling - selectively hide elements at first/last hours
    if(hour == 0) {
      // At first hour, hide prev time layer
      text_layer_set_text(prev_time_layer, "");
      text_layer_set_text(current_time_layer, forecast_hours[hour].hour_string);
      text_layer_set_text(next_time_layer, forecast_hours[hour+1].hour_string);
    } else if(hour == 11) {
      // At last hour, hide next time layer
      text_layer_set_text(prev_time_layer, forecast_hours[hour-1].hour_string);
      text_layer_set_text(current_time_layer, forecast_hours[hour].hour_string);
      text_layer_set_text(next_time_layer, "");
    } else {
      // Show all time layers normally
      text_layer_set_text(prev_time_layer, forecast_hours[hour-1].hour_string);
      text_layer_set_text(current_time_layer, forecast_hours[hour].hour_string);
      text_layer_set_text(next_time_layer, forecast_hours[hour+1].hour_string);
    }
  }

  if(page == VIEW_PAGE_CONDITIONS) {
    if(hour == 0 && precipitation.precipitation_type > 0) {
      text_layer_set_text(current_text_layer, precipitation.precipitation_string);
    } else {
      text_layer_set_text(current_text_layer, forecast_hours[hour].conditions_string);
    }
    set_airflow_view(-1);
    set_experiential_view(-1);
    set_conditions_view(hour);
  } else if (page == VIEW_PAGE_AIRFLOW) {
    text_layer_set_text(current_text_layer, forecast_hours[hour].airflow_string);
    set_conditions_view(-1);
    set_experiential_view(-1);
    set_airflow_view(hour);
  } else if (page == VIEW_PAGE_EXPERIENTIAL) {
    text_layer_set_text(current_text_layer, forecast_hours[hour].experiential_string);
    set_conditions_view(-1);
    set_airflow_view(-1);
    set_experiential_view(hour);
  }

  // Mark the images layer as dirty to redraw the images
  layer_mark_dirty(images_layer);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  
  // Set initial background color based on current forecast data
  GColor initial_color = get_background_color_for_forecast(hour_view, page_view);
  window_set_background_color(window, initial_color);

  // Initialize background animation first so it draws behind other layers (only if animations enabled)
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    background_animation_init(window_layer, window);
#endif
  }

  init_layers(window_layer);
}

static void prv_window_unload(Window *window) {
  // Clean up animation systems (only if animations were enabled)
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    background_animation_deinit();
    text_animation_deinit();
    transition_animation_deinit();
    animation_system_deinit();
#endif
  }
  
  // Clean up any allocated images
  // Note: prev_image/current_image/next_image are owned by page modules.
  // They are deinitialized in their respective deinit functions (e.g., deinit_conditions_layers()).
  // Do not destroy them here to avoid double-free when the app exits.
  prev_image = NULL;
  current_image = NULL;
  next_image = NULL;

  // Clean up the images layer
  if (images_layer) {
    layer_destroy(images_layer);
    images_layer = NULL;
  }

  // Clean up text layers
  if (prev_time_layer) {
    text_layer_destroy(prev_time_layer);
    prev_time_layer = NULL;
  }
  if (current_time_layer) {
    text_layer_destroy(current_time_layer);
    current_time_layer = NULL;
  }
  if (current_text_layer) {
    text_layer_destroy(current_text_layer);
    current_text_layer = NULL;
  }
  if (next_time_layer) {
    text_layer_destroy(next_time_layer);
    next_time_layer = NULL;
  }

  deinit_conditions_layers();
  deinit_airflow_layers();
  deinit_experiential_layers();
}

// Public API implementation
Window* viewer_window_create(void) {
  // Initialize image references to NULL
  prev_image = NULL;
  current_image = NULL;
  next_image = NULL;
  
  // Initialize animation subsystems (only if animations enabled)
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    text_animation_init_system();
    transition_animation_init_system();
    background_animation_init_system();
#endif
  }

  s_viewer_window = window_create();
  window_set_click_config_provider(s_viewer_window, prv_click_config_provider);
  window_set_window_handlers(s_viewer_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });

  return s_viewer_window;
}

void viewer_window_destroy(Window* window) {
  if (window && window == s_viewer_window) {
    // Clean up animation subsystems (only if animations were enabled)
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
      background_animation_deinit_system();
      text_animation_deinit_system();
      transition_animation_deinit_system();
#endif
    }

    window_destroy(window);
    s_viewer_window = NULL;
  }
}

void viewer_update_view(uint8_t hour, uint8_t page) {
  hour_view = hour;
  page_view = page;
  update_view(hour, page);
}

uint8_t viewer_get_current_hour(void) {
  return hour_view;
}

uint8_t viewer_get_current_page(void) {
  return page_view;
}

void viewer_set_current_view(uint8_t hour, uint8_t page) {
  hour_view = hour;
  page_view = page;
  update_view(hour, page);
}

void viewer_set_data_request_callback(ViewerDataRequestCallback callback) {
  data_request_callback = callback;
}
