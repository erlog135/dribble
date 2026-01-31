#include "splash.h"
#include "../layout.h"
#include "../../utils/utils_common.h"
#include "../../utils/weather.h"
#include "../../utils/msgproc.h"
#include "../../utils/demo.h"
#include "../../utils/prefs.h"

// Window and UI elements
static Window* s_splash_window;
static Layer* image_layer;
static TextLayer* status_text_layer;

// Image and text state
static GDrawCommandImage* splash_image = NULL;
static char status_text[64] = "Loading...";

// Loading state
static bool loading_in_progress = false;
static uint8_t received_hours = 0;

// Callbacks
static SplashCompletionCallback completion_callback = NULL;

// Forward declarations
static void handle_data_response(DictionaryIterator *iter);

/**
 * @brief Draws the splash image if available
 */
static void image_layer_update_proc(Layer* layer, GContext* ctx) {
    if (splash_image) {
        GSize image_size = gdraw_command_image_get_bounds_size(splash_image);
        
        // Center the image using layout values
        GPoint origin;
        origin.x = layout.splash_image_center.x - image_size.w / 2;
        origin.y = layout.splash_image_center.y - image_size.h / 2;
        
        gdraw_command_image_draw(ctx, splash_image, origin);
    }
}



/**
 * @brief Handles incoming weather data responses
 */
static void handle_data_response(DictionaryIterator *iter) {
    // Check for JSReady signal from PebbleKit JS
    Tuple* js_ready_tuple = dict_find(iter, MESSAGE_KEY_JS_READY);
    if (js_ready_tuple) {
        UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Received JSReady signal, weather data will arrive automatically");
        splash_set_status_text("Retrieving...");
        return;
    }

    // Check for error responses
    Tuple* response_data_tuple = dict_find(iter, MESSAGE_KEY_RESPONSE_DATA);
    if (response_data_tuple) {
        int16_t response_data = response_data_tuple->value->int32;
        UTIL_LOG(APP_LOG_LEVEL_DEBUG, "Response data: %d", response_data);

        if (response_data == 2) {
            UTIL_LOG(APP_LOG_LEVEL_ERROR, "Location error - unable to get current location");
            splash_set_status_text("Location error");
            loading_in_progress = false;

            if (completion_callback) {
                completion_callback(false);
            }
        } else if (response_data == 1) {
            UTIL_LOG(APP_LOG_LEVEL_ERROR, "Weather data fetch failed");
            splash_set_status_text("Data error");
            loading_in_progress = false;

            if (completion_callback) {
                completion_callback(false);
            }
        }
        return; // Don't process other data in this message
    }

    // Handle all hourly data in a single 120-byte message
    Tuple* hour_data_tuple = dict_find(iter, MESSAGE_KEY_HOUR_DATA);
    if (hour_data_tuple) {
        // Unpack all 12 hours from the 120-byte binary blob
        unpack_all_hours((uint8_t*)hour_data_tuple->value->data, forecast_hours);
        received_hours = 12;
        
        splash_set_status_text("Loading precipitation...");
        UTIL_LOG(APP_LOG_LEVEL_DEBUG, "All hourly data received (120 bytes), waiting for precipitation data");
    }

    // Handle precipitation data
    Tuple* precipitation_package_tuple = dict_find(iter, MESSAGE_KEY_PRECIPITATION_PACKAGE);
    if (precipitation_package_tuple) {
        unpack_precipitation((PrecipitationPackage)precipitation_package_tuple->value->data, &precipitation);

        // Now we have all data (hourly + precipitation), complete the loading
        splash_set_status_text("Loaded!");
        loading_in_progress = false;

        // Call completion callback with success after a brief delay
        app_timer_register(500, (AppTimerCallback)completion_callback, (void*)true);
    }
}

/**
 * @brief Initialize demo data if in demo mode
 */
static void start_demo_data(void) {
    if (DEMO_MODE) {
        splash_set_status_text("Loading demo data...");
        demo_populate_forecast_hours();
        demo_populate_precipitation();

        // Simulate loading delay
        splash_set_status_text("Loaded!");
        loading_in_progress = false;

        // Call completion callback
        app_timer_register(1000, (AppTimerCallback)completion_callback, (void*)true);
    }
}

static void splash_window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    window_set_background_color(window, PBL_IF_COLOR_ELSE(GColorVividCerulean, GColorWhite));
    GRect bounds = layer_get_bounds(window_layer);

    // Initialize layout first
    layout_init(
        bounds.size.w,
        bounds.size.h,
        PBL_IF_ROUND_ELSE(true, false),
    #ifdef PBL_PLATFORM_EMERY
        true
    #else
        false
    #endif
    );

    // Create image layer using layout bounds for top third
    image_layer = layer_create(layout.splash_image_bounds);
    splash_image = gdraw_command_image_create_with_resource(RESOURCE_ID_RETRIEVAL);
    layer_set_update_proc(image_layer, image_layer_update_proc);
    layer_add_child(window_layer, image_layer);

    // Create status text layer using layout bounds
    status_text_layer = text_layer_create(layout.splash_text_bounds);
    text_layer_set_text(status_text_layer, status_text);
    text_layer_set_background_color(status_text_layer, GColorClear);
    text_layer_set_text_color(status_text_layer, GColorBlack);
    text_layer_set_font(status_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(status_text_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(status_text_layer));
}

static void splash_window_unload(Window *window) {
    // Clean up layers
    if (image_layer) {
        layer_destroy(image_layer);
        image_layer = NULL;
    }
    
    if (status_text_layer) {
        text_layer_destroy(status_text_layer);
        status_text_layer = NULL;
    }

    // Clean up image
    if (splash_image) {
        gdraw_command_image_destroy(splash_image);
        splash_image = NULL;
    }
}

// Public API implementation
Window* splash_window_create(void) {
    s_splash_window = window_create();
    window_set_window_handlers(s_splash_window, (WindowHandlers) {
        .load = splash_window_load,
        .unload = splash_window_unload,
    });

    return s_splash_window;
}

void splash_window_destroy(Window* window) {
    if (window && window == s_splash_window) {
        window_destroy(window);
        s_splash_window = NULL;
    }
}

void splash_set_status_text(const char* status_text_new) {
    strncpy(status_text, status_text_new, sizeof(status_text) - 1);
    status_text[sizeof(status_text) - 1] = '\0';
    
    if (status_text_layer) {
        text_layer_set_text(status_text_layer, status_text);
    }
}

void splash_set_image(GDrawCommandImage* image) {
    if (splash_image && splash_image != image) {
        gdraw_command_image_destroy(splash_image);
    }
    splash_image = image;
    
    if (image_layer) {
        layer_mark_dirty(image_layer);
    }
}

void splash_set_completion_callback(SplashCompletionCallback callback) {
    completion_callback = callback;
}

void splash_start_loading(void) {
    if (loading_in_progress) {
        return;
    }

    loading_in_progress = true;
    received_hours = 0;

    // In demo mode, load demo data immediately
    if (DEMO_MODE) {
        start_demo_data();
    } else {
        // In normal mode, PebbleKit JS will automatically send data on ready event
        splash_set_status_text("Connecting...");
    }
}

// Function to be called by main.c when inbox messages are received
void splash_handle_inbox_message(DictionaryIterator *iter) {
    if (loading_in_progress) {
        handle_data_response(iter);
    }
}
