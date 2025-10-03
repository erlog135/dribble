#include "airflow.h"
#include "../layout.h"
#include "../dcim.h"
#include "../resources.h"
#include "../../utils/weather.h"

uint32_t frame_ms = 33;

AppTimer* frame_timer;
AppTimer* timeout_timer;

#define HIGH_WIND_SPEED 75
#define ANEMOMETER_TIMEOUT_MS (60 * 1000)  // 1 minute in milliseconds

//182 is a single degree of rotation
#define ANEMOMETER_SPEED_MIN 182*8
#define ANEMOMETER_SPEED_MAX 182*16

int32_t anemometer_speed = ANEMOMETER_SPEED_MIN; 

static Layer* airflow_layer;
static int32_t current_angle = 0;
static bool is_active = false;
static uint8_t selected_hour = 0;

// Image references passed from main.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Anemometer configuration
static int radius = 30;        // Size of the anemometer
static int cup_size = 15;      // Size of the cups ( ͡° ͜ʖ ͡°)

// Wind vane configuration
static GDrawCommandImage* wind_vane_src_image;
static GDrawCommandImage* wind_vane_angle_src_image;
static GDrawCommandImage** wind_speed_src_images;

static void frame_update();
static void update_icons();

//timeout functions for if view doesn't change for a while
//stops the animation, hopefully saving battery
static void timeout_callback();
static void reset_timeout();

void set_airflow_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;

    //update icons
    update_icons();
    
    // Update image references
    if (is_active) {
        // Get the wind speed icon for previous hour if available
        if (hour > 0) {
            uint8_t prev_wind_speed_icon = forecast_hours[hour-1].wind_speed_icon;
            uint8_t dir = (forecast_hours[hour-1].wind_direction + 2) % 8;
            dcim_8angle_from_src(prev_image_ref, dir, wind_speed_src_images[prev_wind_speed_icon], wind_speed_src_images[prev_wind_speed_icon+1]);
        } else {
            *prev_image_ref = NULL;
        }

        // Current image is the wind vane

        dcim_8angle_from_src(current_image_ref, (forecast_hours[hour].wind_direction + 2) % 8, wind_vane_src_image, wind_vane_angle_src_image);
        
        // Get the wind speed icon for next hour if available
        if (hour < 11) {
            uint8_t next_wind_speed_icon = forecast_hours[hour+1].wind_speed_icon;
            uint8_t dir = (forecast_hours[hour+1].wind_direction + 2) % 8;
            dcim_8angle_from_src(next_image_ref, dir, wind_speed_src_images[next_wind_speed_icon], wind_speed_src_images[next_wind_speed_icon+1]);
        } else {
            *next_image_ref = NULL;
        }
    } else {
        // Clear all image references when inactive
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

    // Load the wind vane PDC
    wind_vane_src_image = init_wind_vane_image();
    wind_vane_angle_src_image = init_wind_vane_angle_image();
    wind_speed_src_images = init_wind_speed_images();


    return airflow_layer;
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