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

#define FIN_IMAGE_TOP_OFFSET 15

enum {
  VIEW_PAGE_CONDITIONS,
  VIEW_PAGE_AIRFLOW,
  VIEW_PAGE_EXPERIENTIAL,
};

#define VIEW_PAGE_NONE 0xFF

// Window and UI elements
static Window* s_viewer_window;
static TextLayer* prev_time_layer;
static TextLayer* current_time_layer;
static TextLayer* next_time_layer;
static TextLayer* current_text_layer;

// Hour-boundary overlays
static StatusBarLayer* s_status_bar;   // Visible only on hour 0
static Layer* s_fin_layer;             // Visible only on hour 11
static GDrawCommandImage* s_fin_image;

// Image references for the three positions. Each active page populates these
// via set_<page>_view(); the viewer then draws them through images_layer.
static GDrawCommandImage* prev_image;
static GDrawCommandImage* current_image;
static GDrawCommandImage* next_image;

// Layer for drawing the images
static Layer* images_layer;

// View state
static uint8_t hour_view = 0;
static uint8_t page_view = VIEW_PAGE_CONDITIONS;
static uint8_t active_page_view = VIEW_PAGE_NONE;  // Tracks which page's set_*_view(hour) is currently active.

// Forward declarations
static void update_view(uint8_t hour, uint8_t page);
static void apply_page_content(uint8_t hour, uint8_t page);
static GColor get_background_color_for_forecast(uint8_t hour, uint8_t page);
static void draw_page_images(Layer* layer, GContext* ctx);

// Helper function to check if animations are enabled
static bool animations_enabled(void) {
#ifdef PBL_PLATFORM_APLITE
    return false;
#else
    ClaySettings* settings = prefs_get_settings();
    return settings->animate;
#endif
}

// Returns the resting draw position for an image occupying the given slot,
// accounting for the precipitation-axis special-cases. Image identity is the
// source of truth, avoiding a duplicated (page, hour, precipitation) check.
static GPoint resolve_image_pos(GDrawCommandImage* img, GPoint default_pos) {
    if (img && img == conditions_get_axis_small_image()) {
        return LAYOUT_AXIS_SM_POS;
    }
    if (img && img == conditions_get_axis_large_image()) {
        return LAYOUT_AXIS_LG_POS;
    }
    return default_pos;
}

// Deactivate whichever page is currently active and clear image slots. After
// this call, a new active page can populate slots without seeing stale
// pointers from the previous page.
static void deactivate_current_page(void) {
    switch (active_page_view) {
        case VIEW_PAGE_CONDITIONS:   set_conditions_view(-1);   break;
        case VIEW_PAGE_AIRFLOW:      set_airflow_view(-1);      break;
        case VIEW_PAGE_EXPERIENTIAL: set_experiential_view(-1); break;
        default: break;
    }
    prev_image = NULL;
    current_image = NULL;
    next_image = NULL;
    active_page_view = VIEW_PAGE_NONE;
}

// Applies the page-specific content: selects content text, activates the page
// for the requested hour, and populates the shared image slots. Used by both
// update_view (static swap) and update_images_and_content_for_animation
// (mid-flight swap that should skip time-layer updates).
static void apply_page_content(uint8_t hour, uint8_t page) {
    const char* content_text = "";
    switch (page) {
        case VIEW_PAGE_CONDITIONS:
            content_text = (hour == 0 && precipitation.precipitation_type > 0)
                ? precipitation.precipitation_string
                : forecast_hours[hour].conditions_string;
            break;
        case VIEW_PAGE_AIRFLOW:
            content_text = forecast_hours[hour].airflow_string;
            break;
        case VIEW_PAGE_EXPERIENTIAL:
            content_text = forecast_hours[hour].experiential_string;
            break;
        default: break;
    }
    text_layer_set_text(current_text_layer, content_text);

    // Only touch other pages when actually switching pages. Re-activating the
    // same page (hour change) just has it repopulate its slots.
    if (active_page_view != page) {
        deactivate_current_page();
    }

    switch (page) {
        case VIEW_PAGE_CONDITIONS:   set_conditions_view(hour);   break;
        case VIEW_PAGE_AIRFLOW:      set_airflow_view(hour);      break;
        case VIEW_PAGE_EXPERIENTIAL: set_experiential_view(hour); break;
        default: break;
    }
    active_page_view = page;

    layer_mark_dirty(images_layer);
}

// Helper function to update only images and content text for animation,
// without touching the time text (the animation system owns that).
static void update_images_and_content_for_animation(uint8_t hour, uint8_t page) {
    if (hour > 11 || page > 2) {
        return;
    }
    apply_page_content(hour, page);
}

// Animation completion callbacks
static void animation_complete_up(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Up animation completed, current hour: %d", hour_view);
    update_view(hour_view, page_view);
}

static void animation_complete_down(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Down animation completed, current hour: %d", hour_view);
    update_view(hour_view, page_view);
}

// Image animation completion callbacks (don't call update_view).
static void image_animation_complete_up(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Image up animation completed");
}
static void image_animation_complete_down(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Image down animation completed");
}

// Transition / background completion callbacks. The animation system tracks its
// own state; these just log so that `animation_is_busy()` is the single source
// of truth for gating input.
static void transition_animation_complete(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Transition animation completed");
}
static void background_animation_complete_hour(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Background hour animation completed");
}
static void background_animation_complete_page(void) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Background page animation completed");
}

static void draw_fin_image(Layer* layer, GContext* ctx) {
    if (s_fin_image) {
        gdraw_command_image_draw(ctx, s_fin_image, GPoint(0, 0));
    }
}

/**
 * @brief Draws the three images (prev, current, next) at their respective positions.
 */
static void draw_page_images(Layer* layer, GContext* ctx) {
    const bool anim = animations_enabled();

#ifndef PBL_PLATFORM_APLITE
    // Don't draw static images while the image animation system is progressively
    // revealing them; it composites them itself during that window.
    if (anim && image_animation_are_images_hidden()) {
        return;
    }
#endif

    GPoint prev_pos, current_pos, next_pos;
    if (anim) {
#ifndef PBL_PLATFORM_APLITE
        transition_animation_get_image_positions(&prev_pos, &current_pos, &next_pos);
#else
        prev_pos = LAYOUT_PREV_ICON_POS;
        current_pos = LAYOUT_CUR_ICON_POS;
        next_pos = LAYOUT_NEXT_ICON_POS;
#endif
    } else {
        prev_pos = LAYOUT_PREV_ICON_POS;
        current_pos = LAYOUT_CUR_ICON_POS;
        next_pos = LAYOUT_NEXT_ICON_POS;
    }

    if (prev_image) {
        gdraw_command_image_draw(ctx, prev_image, resolve_image_pos(prev_image, prev_pos));
    }
    if (current_image) {
        gdraw_command_image_draw(ctx, current_image, resolve_image_pos(current_image, current_pos));
    }
    if (next_image) {
        gdraw_command_image_draw(ctx, next_image, resolve_image_pos(next_image, next_pos));
    }
}

// Click handlers for navigation
static void prv_up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Repeating presses: fast scroll without animation and without wrapping.
  if (click_recognizer_is_repeating(recognizer)) {
    if (animations_enabled() && animation_is_busy()) {
      return;
    }
    if (hour_view > 0) {
      hour_view--;
      update_view(hour_view, page_view);
      window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
    }
    return;
  }

  if (animations_enabled() && animation_is_busy()) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Animation busy, ignoring up click");
    return;
  }

  if(hour_view > 0) {
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
      // Hide any overlay that won't be visible at the destination hour before
      // animations start so it doesn't linger through the transition.
      if (hour_view == 11 && s_fin_layer) {
        layer_set_hidden(s_fin_layer, true);
      }

      // Hour transition: background slides from top, images/text cross-fade.
      GColor animation_color = get_background_color_for_forecast(hour_view - 1, page_view);
      background_animation_start(BACKGROUND_ANIMATION_FROM_TOP, animation_color, background_animation_complete_hour);

      image_animation_store_current_images();

      hour_view--;
      update_images_and_content_for_animation(hour_view, page_view);

      const char* time_text = forecast_hours[hour_view].hour_string;
      const char* content_text = text_layer_get_text(current_text_layer);

      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Starting up animation - hour: %d, time: %s", hour_view, time_text);
      text_animation_start(ANIMATION_DIRECTION_UP, hour_view, time_text, content_text, animation_complete_up);
      image_animation_start(ANIMATION_DIRECTION_UP, hour_view, page_view, image_animation_complete_up);
#endif
    } else {
      hour_view--;
      update_view(hour_view, page_view);
      window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
    }
    return;
  }

  // Wrap: UP at hour 0 jumps straight to hour 11 without animation. The jump is
  // intentionally un-animated because scrolling 11 hours in the "up" direction
  // would be disorienting; prefer a snap.
  hour_view = 11;
  update_view(hour_view, page_view);
  window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
}

static void prv_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (animations_enabled() && animation_is_busy()) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Animation busy, ignoring select click");
    return;
  }

  page_view++;
  if(page_view > 2) {
    page_view = 0;
  }

  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    // Page transition: background slides from the right, images animate out/in.
    GColor animation_color = get_background_color_for_forecast(hour_view, page_view);
    background_animation_start(BACKGROUND_ANIMATION_FROM_RIGHT, animation_color, background_animation_complete_page);

    transition_animation_start(transition_animation_complete);

    image_animation_set_current_page(page_view);
    update_view(hour_view, page_view);
#endif
  } else {
    update_view(hour_view, page_view);
    window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
  }
}

static void prv_down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Repeating presses: fast scroll without animation and without wrapping.
  if (click_recognizer_is_repeating(recognizer)) {
    if (animations_enabled() && animation_is_busy()) {
      return;
    }
    if (hour_view < 11) {
      hour_view++;
      update_view(hour_view, page_view);
      window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
    }
    return;
  }

  if (animations_enabled() && animation_is_busy()) {
    VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Animation busy, ignoring down click");
    return;
  }

  if(hour_view < 11) {
    if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
      // Hide any overlay that won't be visible at the destination hour before
      // animations start so it doesn't linger through the transition.
      if (hour_view == 0 && s_status_bar) {
        layer_set_hidden(status_bar_layer_get_layer(s_status_bar), true);
      }

      GColor animation_color = get_background_color_for_forecast(hour_view + 1, page_view);
      background_animation_start(BACKGROUND_ANIMATION_FROM_BOTTOM, animation_color, background_animation_complete_hour);

      image_animation_store_current_images();

      hour_view++;
      update_images_and_content_for_animation(hour_view, page_view);

      const char* time_text = forecast_hours[hour_view].hour_string;
      const char* content_text = text_layer_get_text(current_text_layer);

      VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Starting down animation - hour: %d, time: %s", hour_view, time_text);
      text_animation_start(ANIMATION_DIRECTION_DOWN, hour_view, time_text, content_text, animation_complete_down);
      image_animation_start(ANIMATION_DIRECTION_DOWN, hour_view, page_view, image_animation_complete_down);
#endif
    } else {
      hour_view++;
      update_view(hour_view, page_view);
      window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
    }
    return;
  }

  // Wrap: DOWN at hour 11 jumps to hour 0 without animation (see up-wrap comment).
  hour_view = 0;
  update_view(hour_view, page_view);
  window_set_background_color(s_viewer_window, get_background_color_for_forecast(hour_view, page_view));
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
  // Up/down use repeating subscriptions so a held button fast-scrolls without
  // animation after the first animated step. repeat_interval_ms of 150 feels
  // responsive without being too rapid across only 12 hours.
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, prv_up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, prv_down_click_handler);
}

static void init_layers(Layer* window_layer) {
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Initializing layers");
  prev_time_layer = text_layer_create(LAYOUT_PREV_TIME_BOUNDS);
  text_layer_set_text(prev_time_layer, "9AM");
  text_layer_set_background_color(prev_time_layer, GColorClear);
  text_layer_set_font(prev_time_layer, fonts_get_system_font(LAYOUT_TIME_FONT));

  current_time_layer = text_layer_create(LAYOUT_CUR_TIME_BOUNDS);
  text_layer_set_text(current_time_layer, "11AM");
  text_layer_set_background_color(current_time_layer, GColorClear);
  text_layer_set_font(current_time_layer, fonts_get_system_font(LAYOUT_TIME_FONT));

  current_text_layer = text_layer_create(LAYOUT_CUR_TEXT_BOUNDS);
  text_layer_set_text(current_text_layer, "12mph SW\n26mph gusts\n1004mb");
  text_layer_set_background_color(current_text_layer, GColorClear);
  text_layer_set_font(current_text_layer, fonts_get_system_font(LAYOUT_TEXT_FONT));

  next_time_layer = text_layer_create(LAYOUT_NEXT_TIME_BOUNDS);
  text_layer_set_text(next_time_layer, "1PM");
  text_layer_set_background_color(next_time_layer, GColorClear);
  text_layer_set_font(next_time_layer, fonts_get_system_font(LAYOUT_TIME_FONT));
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Time layers initialized");
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    text_animation_set_main_layers(current_time_layer, current_text_layer);
    text_animation_set_secondary_layers(prev_time_layer, next_time_layer);
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

  images_layer = layer_create(layer_get_bounds(window_layer));
  layer_set_update_proc(images_layer, draw_page_images);
  layer_add_child(window_layer, images_layer);
  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Images layer added to window layer");

  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    text_animation_set_image_layers(images_layer, &prev_image, &current_image, &next_image);
    transition_animation_set_image_layer(images_layer);

    animation_system_init();
    text_animation_init(window_layer);
    transition_animation_init(window_layer);
#endif
  }

  // Status bar overlay – shown only on hour 0
  s_status_bar = status_bar_layer_create();
  status_bar_layer_set_colors(s_status_bar, GColorClear, GColorBlack);
  layer_set_hidden(status_bar_layer_get_layer(s_status_bar), hour_view != 0);
  layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

  // Fin banner overlay – shown only on hour 11. Bottom edge flush with text
  // (same LAYOUT_PAD_B margin used by LAYOUT_NEXT_TIME_BOUNDS).
  s_fin_image = gdraw_command_image_create_with_resource(RESOURCE_ID_FIN_50PX);
  if (s_fin_image) {
    GSize fin_size = gdraw_command_image_get_bounds_size(s_fin_image);
    int16_t fin_x = (LAYOUT_W - fin_size.w) / 2;
    int16_t fin_y = LAYOUT_H - LAYOUT_PAD_B - fin_size.h + FIN_IMAGE_TOP_OFFSET;
    s_fin_layer = layer_create(GRect(fin_x, fin_y, fin_size.w, fin_size.h));
    layer_set_update_proc(s_fin_layer, draw_fin_image);
    layer_set_hidden(s_fin_layer, hour_view != 11);
    layer_add_child(window_layer, s_fin_layer);
  }

  VIEWER_LOG(APP_LOG_LEVEL_DEBUG, "Layers initialized");
}

// Get background color based on forecast data and current page
static GColor get_background_color_for_forecast(uint8_t hour, uint8_t page) {
  #ifdef PBL_BW
  return GColorWhite;
  #endif

  if (hour > 11) {
    return GColorWhite;
  }

  switch (page) {
    case VIEW_PAGE_CONDITIONS:
      if (hour == 0 && precipitation.precipitation_type > 0) {
        // Cold precipitation types (snow=3, sleet=4, hail=5) use the snow color;
        // warm types use the rain color.
        if (precipitation.precipitation_type >= 3 && precipitation.precipitation_type <= 5) {
          return get_condition_color(4);
        }
        return get_condition_color(3);
      }
      return get_condition_color(forecast_hours[hour].conditions_icon);

    case VIEW_PAGE_AIRFLOW: {
      uint32_t resource_id = forecast_hours[hour].wind_speed_resource_id;
      int speed_level = 0;
      if (resource_id >= RESOURCE_ID_WIND_SPEED_MED_N && resource_id <= RESOURCE_ID_WIND_SPEED_MED_NW) {
        speed_level = 1;
      } else if (resource_id >= RESOURCE_ID_WIND_SPEED_FAST_N && resource_id <= RESOURCE_ID_WIND_SPEED_FAST_NW) {
        speed_level = 2;
      }
      return get_airflow_color(speed_level);
    }

    case VIEW_PAGE_EXPERIENTIAL:
      return get_experiential_color(forecast_hours[hour].experiential_icon);

    default:
      return GColorWhite;
  }
}

// Updates the view for the given hour (0-11) and page.
static void update_view(uint8_t hour, uint8_t page) {
  if (hour > 11 || page > 2) {
    return;
  }

  // Time layers: identical behaviour for animated and non-animated modes.
  // Prev is blanked at the first hour, next is blanked at the last hour.
  text_layer_set_text(prev_time_layer, (hour == 0)  ? "" : forecast_hours[hour - 1].hour_string);
  text_layer_set_text(current_time_layer, forecast_hours[hour].hour_string);
  text_layer_set_text(next_time_layer, (hour == 11) ? "" : forecast_hours[hour + 1].hour_string);

  if (s_status_bar) {
    layer_set_hidden(status_bar_layer_get_layer(s_status_bar), hour != 0);
  }
  if (s_fin_layer) {
    layer_set_hidden(s_fin_layer, hour != 11);
  }

  apply_page_content(hour, page);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  GColor initial_color = get_background_color_for_forecast(hour_view, page_view);
  window_set_background_color(window, initial_color);

  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    background_animation_init(window_layer, window);
#endif
  }

  init_layers(window_layer);
}

static void prv_window_unload(Window *window) {
  if(animations_enabled()) {
#ifndef PBL_PLATFORM_APLITE
    background_animation_deinit();
    text_animation_deinit();
    transition_animation_deinit();
    animation_system_deinit();
#endif
  }

  // Image pointers alias into per-page image arrays owned by the page modules.
  // The arrays are freed by each page's deinit_*_layers(); just null the shared
  // slots here so nothing dangles.
  prev_image = NULL;
  current_image = NULL;
  next_image = NULL;
  active_page_view = VIEW_PAGE_NONE;

  if (images_layer) {
    layer_destroy(images_layer);
    images_layer = NULL;
  }

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

  if (s_status_bar) {
    status_bar_layer_destroy(s_status_bar);
    s_status_bar = NULL;
  }
  if (s_fin_layer) {
    layer_destroy(s_fin_layer);
    s_fin_layer = NULL;
  }
  if (s_fin_image) {
    gdraw_command_image_destroy(s_fin_image);
    s_fin_image = NULL;
  }
}

// Public API implementation
Window* viewer_window_create(void) {
  prev_image = NULL;
  current_image = NULL;
  next_image = NULL;
  active_page_view = VIEW_PAGE_NONE;

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
