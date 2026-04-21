#include "airflow.h"
#include "../layout.h"
#include "../resources.h"
#include "../../utils/weather.h"

#define HIGH_WIND_SPEED 75
#define ANEMOMETER_TIMEOUT_MS (60 * 1000)  // 1 minute in milliseconds

// 182 is a single degree of rotation, times rotations per frame
#define ANEMOMETER_SPEED_MIN 182*6
#define ANEMOMETER_SPEED_MAX 182*30

// Anemometer geometry (drawn in layer-local coordinates)
#define ANEMO_RADIUS 30
#define ANEMO_CUP_SIZE 15
#define ANEMO_DIAM (ANEMO_RADIUS * 2)
#define ANEMO_PADDING 1

static const uint32_t frame_ms = 33;

static AppTimer* frame_timer;
static AppTimer* timeout_timer;

static int32_t anemometer_speed = ANEMOMETER_SPEED_MIN;

static Layer* airflow_layer;
static int32_t current_angle = 0;
static bool is_active = false;
static uint8_t selected_hour = 0;

// Image references passed from viewer.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Wind vane and wind speed configuration
static GDrawCommandImage** wind_vane_images;      // Array of 8 directional wind vanes
static GDrawCommandImage** wind_speed_images;     // Array of 24 wind speed images (3 speeds × 8 directions)

static void frame_update(void* data);
static void update_icons(void);

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

// Timeout functions for if view doesn't change for a while
// stops the animation, hopefully saving battery
static void timeout_callback(void* data);
static void reset_timeout(void);

void set_airflow_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;

    update_icons();

    // Update image references (viewer pre-clears slots on page switch; we only
    // need to populate them when active).
    if (is_active) {
        // Previous hour wind-speed icon, if any.
        if (hour > 0) {
            uint32_t prev_resource_id = forecast_hours[hour-1].wind_speed_resource_id;
            int prev_index = get_wind_speed_image_index(prev_resource_id);
            *prev_image_ref = (prev_index >= 0) ? wind_speed_images[prev_index] : NULL;
        } else {
            *prev_image_ref = NULL;
        }

        // Current image is the wind vane for this hour's direction.
        int8_t vane_dir = forecast_hours[hour].wind_direction;
        *current_image_ref = (vane_dir >= 0 && vane_dir < 8) ? wind_vane_images[vane_dir] : NULL;

        // Next hour wind-speed icon, if any.
        if (hour < 11) {
            uint32_t next_resource_id = forecast_hours[hour+1].wind_speed_resource_id;
            int next_index = get_wind_speed_image_index(next_resource_id);
            *next_image_ref = (next_index >= 0) ? wind_speed_images[next_index] : NULL;
        } else {
            *next_image_ref = NULL;
        }
    }

    if (is_active && !frame_timer) {
        frame_timer = app_timer_register(frame_ms, frame_update, NULL);
        reset_timeout();
    } else if (!is_active && frame_timer) {
        app_timer_cancel(frame_timer);
        frame_timer = NULL;
        if (timeout_timer) {
            app_timer_cancel(timeout_timer);
            timeout_timer = NULL;
        }
    } else if (is_active && frame_timer) {
        reset_timeout();
    }
}

static void update_icons(void) {
    if(!is_active) {
        return;
    }

    // Calculate proportional anemometer speed, but clamp to min/max.
    int wind_speed = forecast_hours[selected_hour].wind_speed;
    int range = ANEMOMETER_SPEED_MAX - ANEMOMETER_SPEED_MIN;
    int speed = (wind_speed * range) / HIGH_WIND_SPEED + ANEMOMETER_SPEED_MIN;
    if (speed > ANEMOMETER_SPEED_MAX) speed = ANEMOMETER_SPEED_MAX;
    if (speed < ANEMOMETER_SPEED_MIN) speed = ANEMOMETER_SPEED_MIN;
    anemometer_speed = speed;
}

void reset_anemometer_timeout(void) {
    reset_timeout();
}

static void timeout_callback(void* data) {
    if (frame_timer) {
        app_timer_cancel(frame_timer);
        frame_timer = NULL;
    }
    timeout_timer = NULL;

    if (airflow_layer) {
        layer_mark_dirty(airflow_layer);
    }
}

static void reset_timeout(void) {
    if (timeout_timer) {
        app_timer_cancel(timeout_timer);
        timeout_timer = NULL;
    }

    if (is_active && frame_timer) {
        timeout_timer = app_timer_register(ANEMOMETER_TIMEOUT_MS, timeout_callback, NULL);
    }
}

static void frame_update(void* data) {
    if (!is_active) return;

    current_angle += anemometer_speed;
    if(current_angle >= TRIG_MAX_ANGLE) {
        current_angle -= TRIG_MAX_ANGLE;
    }
    layer_mark_dirty(airflow_layer);
    frame_timer = app_timer_register(frame_ms, frame_update, NULL);
}

Layer* init_airflow_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image) {
    // Size the layer to exactly the anemometer bounding box. The wind vane icon
    // lives in the 50x50 current-icon slot; the anemometer is a 60x60 disc
    // centred on that slot and is the only thing this layer draws. Keeping the
    // layer small means frame_update()'s 30fps mark_dirty invalidates a 60x60
    // region instead of the full screen.
    GPoint icon_origin = LAYOUT_CUR_ICON_POS;
    int cx = icon_origin.x + LAYOUT_ICON_LG / 2;
    int cy = icon_origin.y + LAYOUT_ICON_LG / 2;
    GRect anemo_frame = GRect(
        cx - ANEMO_RADIUS - ANEMO_PADDING,
        cy - ANEMO_RADIUS - ANEMO_PADDING,
        ANEMO_DIAM + ANEMO_PADDING * 2,
        ANEMO_DIAM + ANEMO_PADDING * 2
    );

    airflow_layer = layer_create(anemo_frame);
    layer_set_update_proc(airflow_layer, draw_airflow);
    layer_add_child(window_layer, airflow_layer);

    prev_image_ref = prev_image;
    current_image_ref = current_image;
    next_image_ref = next_image;

    // Load the wind vane and wind speed images smartly (only used directions/speeds)
    wind_vane_images = init_wind_vane_images();
    wind_speed_images = init_wind_speed_images();

    return airflow_layer;
}

void deinit_airflow_layers(void) {
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

    // Layer is sized to ANEMO_DIAM x ANEMO_DIAM; draw in layer-local coords.
    GRect bounds = layer_get_bounds(layer);
    GPoint center = GPoint(
        bounds.origin.x + ANEMO_RADIUS + ANEMO_PADDING,
        bounds.origin.y + ANEMO_RADIUS + ANEMO_PADDING
    );

    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);

    GRect anemometer_rect = GRect(
        center.x - ANEMO_RADIUS,
        center.y - ANEMO_RADIUS,
        ANEMO_DIAM,
        ANEMO_DIAM
    );

    // Draw three lines with semicircles at 120 degree intervals
    for(int i = 0; i < 3; i++) {
        int32_t base_angle = current_angle + (i * TRIG_MAX_ANGLE / 3);

        GPoint end = gpoint_from_polar(anemometer_rect, GOvalScaleModeFitCircle, base_angle);

        GPoint cup_center = {
            .x = center.x + ((end.x - center.x) * (ANEMO_RADIUS - ANEMO_CUP_SIZE/2)) / ANEMO_RADIUS,
            .y = center.y + ((end.y - center.y) * (ANEMO_RADIUS - ANEMO_CUP_SIZE/2)) / ANEMO_RADIUS
        };

        GRect circle_rect = GRect(
            cup_center.x - ANEMO_CUP_SIZE/2,
            cup_center.y - ANEMO_CUP_SIZE/2,
            ANEMO_CUP_SIZE,
            ANEMO_CUP_SIZE
        );

        graphics_fill_radial(ctx, circle_rect, GOvalScaleModeFitCircle, ANEMO_CUP_SIZE/2, base_angle, base_angle + TRIG_MAX_ANGLE/2);
        graphics_draw_line(ctx, center, end);
        graphics_draw_arc(ctx, circle_rect, GOvalScaleModeFitCircle, base_angle, base_angle + TRIG_MAX_ANGLE/2);
    }
}
