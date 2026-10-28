#include "conditions.h"
#include "../layout.h"
#include "../resources.h"
#include "../animation/precip_animation.h"
#include "../../utils/weather.h"

static Layer* conditions_layer;
static bool is_active = false;
static uint8_t selected_hour = 0;

// Graph drawing delay
static AppTimer* graph_draw_timer = NULL;
static bool show_graph = false;
static bool graph_points_dirty = true;  // Re-bake Y coords on next draw when set.

// Image references passed from viewer.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Axis images for precipitation display
static GDrawCommandImage* axis_small_image;
static GDrawCommandImage* axis_large_image;

// Images owned by the three visible slots; destroyed/reloaded on each view change.
static GDrawCommandImage* owned_slots[3];

static void release_owned_slots(void) {
    for (int i = 0; i < 3; i++) {
        if (owned_slots[i]) {
            gdraw_command_image_destroy(owned_slots[i]);
            owned_slots[i] = NULL;
        }
    }
    axis_small_image = NULL;
    axis_large_image = NULL;
}

// Precipitation graph configuration
static GPath* precipitation_graph;

// Precomputed gridline Y offsets, hoisted out of the per-frame draw.
static const int GRIDLINE_COUNT = 3;

// The precipitation graph
// 13 points for 0-60 minutes, in 5 minute intervals
// +2 bottom points to make it a closed path
// sized by layout.h
static GPathInfo precipitation_graph_info = {
  .num_points = 15,
  .points = (GPoint[]){
    {0, 0},
    {7, 0},
    {14, 0},
    {21, 0},
    {28, 0},
    {35, 0},
    {42, 0},
    {49, 0},
    {56, 0},
    {63, 0},
    {70, 0},
    {77, 0},
    {83, 0},
    {83, 0},
    {0, 0},
  }
};

// Bake the 13 data-point Y coordinates from current precipitation data.
// Sets graph_points_dirty to false; the two closing-path points are also set.
static void bake_graph_points(void) {
    const int step = LAYOUT_PRECIP_H / 4;
    for (int i = 0; i < 13; i++) {
        precipitation_graph_info.points[i].y =
            LAYOUT_PRECIP_H - (precipitation.precipitation_intensity[i] * step);
    }
    precipitation_graph_info.points[13].y = LAYOUT_PRECIP_H;
    precipitation_graph_info.points[14].y = LAYOUT_PRECIP_H;
    graph_points_dirty = false;
}

// Timer callback to start drawing the graph after a delay
static void graph_draw_timer_callback(void* context) {
    graph_draw_timer = NULL;
    show_graph = true;

    bake_graph_points();

#ifndef PBL_PLATFORM_APLITE
    if (conditions_layer) {
        precip_animation_start(conditions_layer, &precipitation_graph_info,
                               13, LAYOUT_PRECIP_H);
    }
#endif

    if (conditions_layer) {
        layer_mark_dirty(conditions_layer);
    }
}

void set_conditions_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;

    if (graph_draw_timer) {
        app_timer_cancel(graph_draw_timer);
        graph_draw_timer = NULL;
    }
    show_graph = false;

#ifndef PBL_PLATFORM_APLITE
    precip_animation_stop();
#endif

    // The graph's Y coordinates depend on precipitation data and on whether it's
    // visible at all; re-bake on every activation so a new forecast or hour
    // switch takes effect immediately on next draw.
    graph_points_dirty = true;

    // Free the previous slot images, then load only what the new view shows.
    release_owned_slots();
    if (is_active) {
        if (hour > 0) {
            if (hour == forecast_now_index + 1 && precipitation.precipitation_type > 0) {
                owned_slots[0] = load_axis_image(false);
                axis_small_image = owned_slots[0];
            } else {
                owned_slots[0] = load_condition_image(forecast_hours[hour-1].conditions_icon, false);
            }
        }
        if (hour == forecast_now_index && precipitation.precipitation_type > 0) {
            owned_slots[1] = load_axis_image(true);
            axis_large_image = owned_slots[1];
        } else {
            owned_slots[1] = load_condition_image(forecast_hours[hour].conditions_icon, true);
        }
        if (hour < LAST_FORECAST_HOUR) {
            owned_slots[2] = load_condition_image(forecast_hours[hour+1].conditions_icon, false);
        }
        *prev_image_ref = owned_slots[0];
        *current_image_ref = owned_slots[1];
        *next_image_ref = owned_slots[2];

        if (hour == forecast_now_index && precipitation.precipitation_type > 0) {
            graph_draw_timer = app_timer_register(300, graph_draw_timer_callback, NULL);
        }
    }

    if (conditions_layer) {
        layer_mark_dirty(conditions_layer);
    }
}

GDrawCommandImage* conditions_get_axis_small_image(void) {
    return axis_small_image;
}

GDrawCommandImage* conditions_get_axis_large_image(void) {
    return axis_large_image;
}

Layer* init_conditions_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image) {
    conditions_layer = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(conditions_layer, draw_conditions);
    layer_add_child(window_layer, conditions_layer);

    // Store the image references
    prev_image_ref = prev_image;
    current_image_ref = current_image;
    next_image_ref = next_image;

    // Initialize precipitation graph
    precipitation_graph = gpath_create(&precipitation_graph_info);
    precipitation_graph->offset = LAYOUT_PRECIP_POS;

    return conditions_layer;
}

void draw_conditions(Layer* layer, GContext* ctx) {
    if (!is_active) {
        return;
    }

    // Only draw the precipitation graph if hour 0 has precipitation and the
    // stagger timer has elapsed.
    if (selected_hour != forecast_now_index || precipitation.precipitation_type <= 0 || !show_graph) {
        return;
    }

    // Re-bake graph Y coordinates only when inputs have changed. Skip when the
    // precip animation is running — it owns the point values for each frame.
    if (graph_points_dirty) {
        bake_graph_points();
    }

    // Gridlines: three equally-spaced horizontal lines inside the precipitation rect.
    // During the rise animation they follow the same progress curve as the graph.
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    const int step = LAYOUT_PRECIP_H / 4;
    const int x0 = LAYOUT_PRECIP_X;
    const int x1 = x0 + LAYOUT_PRECIP_W;
#ifndef PBL_PLATFORM_APLITE
    const int bottom_y = LAYOUT_PRECIP_Y + LAYOUT_PRECIP_H;
    AnimationProgress anim_progress = precip_animation_get_progress();
#endif
    for (int i = 1; i <= GRIDLINE_COUNT; i++) {
        int target_y = LAYOUT_PRECIP_Y + (i * step);
#ifndef PBL_PLATFORM_APLITE
        int y = bottom_y + (int)((int32_t)(target_y - bottom_y) * anim_progress / ANIMATION_NORMALIZED_MAX);
#else
        int y = target_y;
#endif
        graphics_draw_line(ctx, GPoint(x0, y), GPoint(x1, y));
    }

    graphics_context_set_fill_color(ctx, GColorWhite);
    gpath_draw_filled(ctx, precipitation_graph);

    graphics_context_set_stroke_width(ctx, 2);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    gpath_draw_outline(ctx, precipitation_graph);
}

void deinit_conditions_layers(void) {
    // Cancel any active timer
    if (graph_draw_timer) {
        app_timer_cancel(graph_draw_timer);
        graph_draw_timer = NULL;
    }

#ifndef PBL_PLATFORM_APLITE
    precip_animation_deinit();
#endif

    release_owned_slots();
    if (precipitation_graph) {
        gpath_destroy(precipitation_graph);
        precipitation_graph = NULL;
    }

    if (conditions_layer) {
        layer_destroy(conditions_layer);
        conditions_layer = NULL;
    }
}
