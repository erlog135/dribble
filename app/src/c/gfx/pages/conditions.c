#include "conditions.h"
#include "../layout.h"
#include "../resources.h"
#include "../../utils/weather.h"

static Layer* conditions_layer;
static bool is_active = false;
static uint8_t selected_hour = 0;

// Graph drawing delay
static AppTimer* graph_draw_timer = NULL;
static bool show_graph = false;

// Image references passed from main.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Conditions configuration
static GDrawCommandImage** condition_images_25px;
static GDrawCommandImage** condition_images_50px;

// Axis images for precipitation display
static GDrawCommandImage* axis_small_image;
static GDrawCommandImage* axis_large_image;

// Precipitation graph configuration
static GPath* precipitation_graph;

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
    {84, 0},
    {84, 0},
    {0, 0},
  }
};

// Timer callback to start drawing the graph after a delay
static void graph_draw_timer_callback(void* context) {
    graph_draw_timer = NULL;
    show_graph = true;
    
    // Mark the layer as dirty to redraw with the graph
    if (conditions_layer) {
        layer_mark_dirty(conditions_layer);
    }
}

void set_conditions_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;
    
    // Cancel any existing graph timer
    if (graph_draw_timer) {
        app_timer_cancel(graph_draw_timer);
        graph_draw_timer = NULL;
    }
    
    // Reset graph visibility - always start hidden immediately
    show_graph = false;
    
    // If switching to hour 0 with precipitation, ensure graph stays hidden during transition
    if (hour == 0 && precipitation.precipitation_type > 0) {
        // Force immediate redraw to hide any visible graph before starting timer
        if (conditions_layer) {
            layer_mark_dirty(conditions_layer);
        }
    }

    // Update image references
    if (is_active) {
        // Set previous hour condition icon if available
        if (hour > 0) {
            // If viewing hour 1 and hour 0 had precipitation, show small axis instead
            if (hour == 1 && precipitation.precipitation_type > 0) {
                *prev_image_ref = axis_small_image;
            } else {
                *prev_image_ref = condition_images_25px[forecast_hours[hour-1].conditions_icon];
            }
        } else {
            *prev_image_ref = NULL;
        }

        // Set current hour condition icon
        if (selected_hour == 0 && precipitation.precipitation_type > 0) {
            // Use axis image instead of hiding when precipitation exists
            *current_image_ref = axis_large_image;
        } else {
            *current_image_ref = condition_images_50px[forecast_hours[hour].conditions_icon];
        }

        // Set next hour condition icon if available
        if (hour < 11) {
            *next_image_ref = condition_images_25px[forecast_hours[hour+1].conditions_icon];
        } else {
            *next_image_ref = NULL;
        }
        
        // Start graph draw timer if we're showing precipitation
        if (hour == 0 && precipitation.precipitation_type > 0) {
            // Ensure graph starts hidden
            show_graph = false;
            graph_draw_timer = app_timer_register(300, graph_draw_timer_callback, NULL); // 300ms delay
        }
    } else {
        // Clear all image references when inactive
        *prev_image_ref = NULL;
        *current_image_ref = NULL;
        *next_image_ref = NULL;
    }

    // Mark the layer as dirty to redraw
    if (conditions_layer) {
        layer_mark_dirty(conditions_layer);
    }
}

Layer* init_conditions_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image) {
    conditions_layer = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(conditions_layer, draw_conditions);
    layer_add_child(window_layer, conditions_layer);

    // Store the image references
    prev_image_ref = prev_image;
    current_image_ref = current_image;
    next_image_ref = next_image;

    // Initialize condition images
    condition_images_25px = init_25px_condition_images();
    condition_images_50px = init_50px_condition_images();
    
    // Initialize axis images
    axis_small_image = init_axis_small_image();
    axis_large_image = init_axis_large_image();

    // Initialize precipitation graph
    precipitation_graph = gpath_create(&precipitation_graph_info);
    precipitation_graph->offset = layout.precipitation_graph_pos;

    return conditions_layer;
}

void draw_conditions(Layer* layer, GContext* ctx) {
    if (!is_active) {
        return;
    }

    // Only draw the precipitation graph if the first hour has precipitation AND the timer has elapsed
    if (selected_hour == 0 && precipitation.precipitation_type > 0 && show_graph) {
        // Draw the precipitation graph, first hours worth of points
        for (int i = 0; i < 13; i++) {
            precipitation_graph_info.points[i].y = layout.precipitation_graph_height - (precipitation.precipitation_intensity[i] * (layout.precipitation_graph_height/4));
        }

        // Last two points stay at bottom
        precipitation_graph_info.points[13].y = layout.precipitation_graph_height;
        precipitation_graph_info.points[14].y = layout.precipitation_graph_height;

        // Draw 3 lines to signify precipitation intensity
        graphics_context_set_stroke_width(ctx, 1);
        graphics_context_set_stroke_color(ctx, GColorDarkGray);

        for (int i = 1; i < 4; i++) {
            int xoffset = layout.precipitation_graph_pos.x;
            int yoffset = layout.precipitation_graph_pos.y;
            int y = (i * (layout.precipitation_graph_height/4));
            graphics_draw_line(ctx, GPoint(xoffset, y+yoffset), GPoint(layout.precipitation_graph_width+xoffset, y+yoffset));
        }

        graphics_context_set_fill_color(ctx, GColorWhite);
        gpath_draw_filled(ctx, precipitation_graph);

        graphics_context_set_stroke_width(ctx, 2);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        gpath_draw_outline(ctx, precipitation_graph);
    }
}

void deinit_conditions_layers(void) {
    // Cancel any active timer
    if (graph_draw_timer) {
        app_timer_cancel(graph_draw_timer);
        graph_draw_timer = NULL;
    }
    
    if (condition_images_25px) {
        deinit_25px_condition_images(condition_images_25px);
        condition_images_25px = NULL;
    }
    if (condition_images_50px) {
        deinit_50px_condition_images(condition_images_50px);
        condition_images_50px = NULL;
    }
    if (axis_small_image) {
        deinit_axis_image(axis_small_image);
        axis_small_image = NULL;
    }
    if (axis_large_image) {
        deinit_axis_image(axis_large_image);
        axis_large_image = NULL;
    }
    if (precipitation_graph) {
        gpath_destroy(precipitation_graph);
        precipitation_graph = NULL;
    }
} 