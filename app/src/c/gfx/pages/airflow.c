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

// Images owned by the three visible slots; destroyed/reloaded on each view change.
static GDrawCommandImage* owned_slots[3];
static void release_owned_slots(void) {
    for (int i = 0; i < 3; i++) {
        if (owned_slots[i]) {
            gdraw_command_image_destroy(owned_slots[i]);
            owned_slots[i] = NULL;
        }
    }
}

static void frame_update(void* data);
static void update_icons(void);

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
    release_owned_slots();
    if (is_active) {
        if (hour > 0) {
            owned_slots[0] = load_wind_speed_image(forecast_hours[hour-1].wind_speed_resource_id);
        }
        owned_slots[1] = load_wind_vane_image(forecast_hours[hour].wind_direction);
        if (hour < LAST_FORECAST_HOUR) {
            owned_slots[2] = load_wind_speed_image(forecast_hours[hour+1].wind_speed_resource_id);
        }
        *prev_image_ref = owned_slots[0];
        *current_image_ref = owned_slots[1];
        *next_image_ref = owned_slots[2];
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

    return airflow_layer;
}

void deinit_airflow_layers(void) {
    release_owned_slots();
    
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
