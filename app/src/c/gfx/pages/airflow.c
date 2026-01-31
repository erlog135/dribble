#include "airflow.h"
#include "../layout.h"
#include "../resources.h"
#include "../../utils/weather.h"

uint32_t frame_ms = 33;

AppTimer* frame_timer;
AppTimer* timeout_timer;

#define HIGH_WIND_SPEED 75
#define ANEMOMETER_TIMEOUT_MS (60 * 1000)  // 1 minute in milliseconds

//182 is a single degree of rotation, times rotations per frame
#define ANEMOMETER_SPEED_MIN 182*6
#define ANEMOMETER_SPEED_MAX 182*30

int32_t anemometer_speed = ANEMOMETER_SPEED_MIN; 

static Layer* airflow_layer;
static int32_t current_angle = 0;
static bool is_active = false;
static bool was_active = false;  // Track if we were active before, to know if we own the images
static uint8_t selected_hour = 0;

// Image references passed from main.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Anemometer configuration
static int radius = 30;        // Size of the anemometer
static int cup_size = 15;      // Size of the cups ( ͡° ͜ʖ ͡°)

// Wind vane and wind speed configuration
static GDrawCommandImage** wind_vane_images;      // Array of 8 directional wind vanes
static GDrawCommandImage** wind_speed_images;     // Array of 24 wind speed images (3 speeds × 8 directions)

static void frame_update();
static void update_icons();

// Helper function to get wind speed image index from resource ID
static int get_wind_speed_image_index(uint32_t resource_id) {
    if (resource_id == 0) return -1;
    
    // Calculate index from resource ID
    if (resource_id >= RESOURCE_ID_WIND_SPEED_SLOW_N && resource_id <= RESOURCE_ID_WIND_SPEED_SLOW_NW) {
        return resource_id - RESOURCE_ID_WIND_SPEED_SLOW_N;
    } else if (resource_id >= RESOURCE_ID_WIND_SPEED_MED_N && resource_id <= RESOURCE_ID_WIND_SPEED_MED_NW) {
        return 8 + (resource_id - RESOURCE_ID_WIND_SPEED_MED_N);
    } else if (resource_id >= RESOURCE_ID_WIND_SPEED_FAST_N && resource_id <= RESOURCE_ID_WIND_SPEED_FAST_NW) {
        return 16 + (resource_id - RESOURCE_ID_WIND_SPEED_FAST_N);
    }
    
    return -1;
}

//timeout functions for if view doesn't change for a while
//stops the animation, hopefully saving battery
static void timeout_callback();
static void reset_timeout();

void set_airflow_view(int hour) {
    was_active = is_active;  // Remember previous state
    is_active = (hour >= 0);
    selected_hour = hour;

    //update icons
    update_icons();
    
    // Update image references
    if (is_active) {
        // Get the wind speed icon for previous hour if available
        if (hour > 0) {
            uint32_t prev_resource_id = forecast_hours[hour-1].wind_speed_resource_id;
            int prev_index = get_wind_speed_image_index(prev_resource_id);
            if (prev_index >= 0 && wind_speed_images[prev_index]) {
                *prev_image_ref = wind_speed_images[prev_index];
            } else {
                *prev_image_ref = NULL;
            }
        } else {
            // Destroy existing image before setting to NULL
            if (was_active && *prev_image_ref) {
                // Don't destroy - it's from our array
            }
            *prev_image_ref = NULL;
        }

        // Current image is the wind vane for this hour's direction
        int8_t vane_dir = forecast_hours[hour].wind_direction;
        if (vane_dir >= 0 && vane_dir < 8 && wind_vane_images[vane_dir]) {
            *current_image_ref = wind_vane_images[vane_dir];
        } else {
            *current_image_ref = NULL;
        }
        
        // Get the wind speed icon for next hour if available
        if (hour < 11) {
            uint32_t next_resource_id = forecast_hours[hour+1].wind_speed_resource_id;
            int next_index = get_wind_speed_image_index(next_resource_id);
            if (next_index >= 0 && wind_speed_images[next_index]) {
                *next_image_ref = wind_speed_images[next_index];
            } else {
                *next_image_ref = NULL;
            }
        } else {
            // Destroy existing image before setting to NULL
            if (was_active && *next_image_ref) {
                // Don't destroy - it's from our array
            }
            *next_image_ref = NULL;
        }
    } else {
        // Clear all image references when inactive
        // Images are owned by the init arrays, not by individual pages
        // So we just set pointers to NULL without destroying
        *prev_image_ref = NULL;
        *current_image_ref = NULL;
        *next_image_ref = NULL;
    }
    
    // If becoming active, start the frame timer and timeout
    if (is_active && !frame_timer) {
        frame_timer = app_timer_register(frame_ms, (AppTimerCallback) frame_update, NULL);
        reset_timeout();  // Start the timeout timer
    }
    // If becoming inactive, cancel both timers
    else if (!is_active && frame_timer) {
        app_timer_cancel(frame_timer);
        frame_timer = NULL;
        if (timeout_timer) {
            app_timer_cancel(timeout_timer);
            timeout_timer = NULL;
        }
    }
    // If already active but view changed, reset the timeout
    else if (is_active && frame_timer) {
        reset_timeout();  // Reset the timeout on view change
    }
}

void update_icons() {

    if(!is_active) {
        return;
    }

    // Calculate proportional anemometer speed, but clamp to min/max
    int wind_speed = forecast_hours[selected_hour].wind_speed;
    int range = ANEMOMETER_SPEED_MAX - ANEMOMETER_SPEED_MIN;
    int speed = ANEMOMETER_SPEED_MIN;
    if (HIGH_WIND_SPEED > 0) {
        speed = (wind_speed * range) / HIGH_WIND_SPEED + ANEMOMETER_SPEED_MIN;
    }
    if (speed > ANEMOMETER_SPEED_MAX) speed = ANEMOMETER_SPEED_MAX;
    if (speed < ANEMOMETER_SPEED_MIN) speed = ANEMOMETER_SPEED_MIN;
    anemometer_speed = speed;
}

void reset_anemometer_timeout(void) {
    reset_timeout();
}

static void timeout_callback() {
    // Stop the anemometer animation by canceling the frame timer
    if (frame_timer) {
        app_timer_cancel(frame_timer);
        frame_timer = NULL;
    }
    timeout_timer = NULL;
    
    // Mark the layer dirty to redraw without animation
    if (airflow_layer) {
        layer_mark_dirty(airflow_layer);
    }
}

static void reset_timeout() {
    // Cancel existing timeout timer if it exists
    if (timeout_timer) {
        app_timer_cancel(timeout_timer);
        timeout_timer = NULL;
    }
    
    // Start a new timeout timer only if we're active and animating
    if (is_active && frame_timer) {
        timeout_timer = app_timer_register(ANEMOMETER_TIMEOUT_MS, (AppTimerCallback) timeout_callback, NULL);
    }
}

void frame_update() {
    // Only update if we're active
    if (!is_active) return;
    
    current_angle += anemometer_speed;
    if(current_angle >= TRIG_MAX_ANGLE) {
        current_angle -= TRIG_MAX_ANGLE;
    }
    layer_mark_dirty(airflow_layer);
    frame_timer = app_timer_register(frame_ms, (AppTimerCallback) frame_update, NULL);
}

Layer* init_airflow_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image) {
    airflow_layer = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(airflow_layer, draw_airflow);
    layer_add_child(window_layer, airflow_layer);

    // Store the image references
    prev_image_ref = prev_image;
    current_image_ref = current_image;
    next_image_ref = next_image;

    // Load the wind vane and wind speed images smartly (only used directions/speeds)
    wind_vane_images = init_wind_vane_images();
    wind_speed_images = init_wind_speed_images();

    return airflow_layer;
}

void deinit_airflow_layers() {
    // Clean up wind vane images
    if (wind_vane_images) {
        deinit_wind_vane_images(wind_vane_images);
        wind_vane_images = NULL;
    }
    
    // Clean up wind speed images
    if (wind_speed_images) {
        deinit_wind_speed_images(wind_speed_images);
        wind_speed_images = NULL;
    }
    
    // Clean up the layer
    if (airflow_layer) {
        layer_destroy(airflow_layer);
        airflow_layer = NULL;
    }
    
    // Cancel any active timers
    if (frame_timer) {
        app_timer_cancel(frame_timer);
        frame_timer = NULL;
    }
    if (timeout_timer) {
        app_timer_cancel(timeout_timer);
        timeout_timer = NULL;
    }
}

void draw_airflow(Layer* layer, GContext* ctx) {
    if(!is_active) {
        return;
    }

    // Use layout positioning for the wind vane center
    GPoint center = layout.current_icon_pos;
    center.x += layout.icon_large / 2;  // Center of the 50px icon
    center.y += layout.icon_large / 2;
    
    // Set up the drawing style
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);
    
    // Create a rect for the anemometer circle
    GRect anemometer_rect = GRect(
        center.x - radius,
        center.y - radius,
        radius * 2,
        radius * 2
    );
    
    // Draw three lines with semicircles at 120 degree intervals
    for(int i = 0; i < 3; i++) {
        int32_t base_angle = current_angle + (i * TRIG_MAX_ANGLE / 3);
        
        // Calculate end point on the perimeter
        GPoint end = gpoint_from_polar(anemometer_rect, GOvalScaleModeFitCircle, base_angle);
        
        // Calculate cup position (moved inward by half cup size)
        GPoint cup_center = {
            .x = center.x + ((end.x - center.x) * (radius - cup_size/2)) / radius,
            .y = center.y + ((end.y - center.y) * (radius - cup_size/2)) / radius
        };
        
        // Draw semicircle at the cup position
        GRect circle_rect = GRect(
            cup_center.x - cup_size/2,
            cup_center.y - cup_size/2,
            cup_size,
            cup_size
        );
        
        // Fill the cup with white
        graphics_fill_radial(ctx, circle_rect, GOvalScaleModeFitCircle, cup_size/2, base_angle, base_angle + TRIG_MAX_ANGLE/2);
        
        // Draw the line from center to perimeter
        graphics_draw_line(ctx, center, end);
        
        // Draw the outline of the cup
        graphics_draw_arc(ctx, circle_rect, GOvalScaleModeFitCircle, base_angle, base_angle + TRIG_MAX_ANGLE/2);
    }
} 