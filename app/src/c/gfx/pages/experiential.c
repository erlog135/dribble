#include "experiential.h"
#include "../layout.h"
#include "../resources.h"
#include "../../utils/weather.h"

static Layer* experiential_layer;
static bool is_active = false;
static uint8_t selected_hour = 0;

// Image references passed from main.c
static GDrawCommandImage** prev_image_ref;
static GDrawCommandImage** current_image_ref;
static GDrawCommandImage** next_image_ref;

// Experiential configuration
static GDrawCommandImage** experiential_images_25px;
static GDrawCommandImage** experiential_images_50px;

static GDrawCommandImage* emoji_image;

static void update_icons();

void set_experiential_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;

    // Update icons
    if(is_active) {
        update_icons();
    }

    // Update image references
    if (is_active) {
        // Get experiential icon for current hour
        uint8_t current_experiential_icon = forecast_hours[hour].experiential_icon;

        // Set previous hour experiential icon if available
        if (hour > 0) {
            uint8_t prev_experiential_icon = forecast_hours[hour - 1].experiential_icon;
            if (prev_experiential_icon == 0) {
                *prev_image_ref = NULL;
            } else {
                *prev_image_ref = experiential_images_25px[prev_experiential_icon - 1];
            }
        } else {
            *prev_image_ref = NULL;
        }

        // Set current hour experiential icon (for animations)
        if (current_experiential_icon == 0) {
            *current_image_ref = NULL;
        } else {
            *current_image_ref = experiential_images_50px[current_experiential_icon - 1];
        }

        // Set next hour experiential icon if available
        if (hour < 11) {
            uint8_t next_experiential_icon = forecast_hours[hour + 1].experiential_icon;
            if (next_experiential_icon == 0) {
                *next_image_ref = NULL;
            } else {
                *next_image_ref = experiential_images_25px[next_experiential_icon - 1];
            }
        } else {
            *next_image_ref = NULL;
        }
    } else {
        // Clear all image references when inactive
        *prev_image_ref = NULL;
        *current_image_ref = NULL;
        *next_image_ref = NULL;
    }
}

GDrawCommandImage* get_experiential_emoji(void) {
    return emoji_image;
}

void update_icons() {
    //emoji_image = emoji_images[rand() % 4];

    // Mark the layer as dirty to redraw
    if (experiential_layer) {
        layer_mark_dirty(experiential_layer);
    }
}

Layer* init_experiential_layers(Layer* window_layer, GDrawCommandImage** prev_image, GDrawCommandImage** current_image, GDrawCommandImage** next_image) {
    experiential_layer = layer_create(layer_get_bounds(window_layer));
    layer_set_update_proc(experiential_layer, draw_experiential);
    layer_add_child(window_layer, experiential_layer);

    // Store the image references
    prev_image_ref = prev_image;
    current_image_ref = current_image;
    next_image_ref = next_image;

    // TODO: Initialize experiential images
    experiential_images_25px = init_25px_experiential_images();
    experiential_images_50px = init_50px_experiential_images();
    
    // Randomly choose one emoji to load
    const uint32_t emoji_ids[] = {
        RESOURCE_ID_EMOJI_KISSING,
        RESOURCE_ID_EMOJI_SMILE,
        RESOURCE_ID_EMOJI_TEETH,
        RESOURCE_ID_EMOJI_WINKY_TONGUE
    };
    uint32_t chosen_emoji_id = emoji_ids[rand() % 4];
    emoji_image = gdraw_command_image_create_with_resource(chosen_emoji_id);

    return experiential_layer;
}

void deinit_experiential_layers(void) {
    if (experiential_images_25px) {
        deinit_25px_experiential_images(experiential_images_25px);
        experiential_images_25px = NULL;
    }
    if (experiential_images_50px) {
        deinit_50px_experiential_images(experiential_images_50px);
        experiential_images_50px = NULL;
    }
    if (emoji_image) {
        gdraw_command_image_destroy(emoji_image);
        emoji_image = NULL;
    }
    if (experiential_layer) {
        layer_destroy(experiential_layer);
        experiential_layer = NULL;
    }
}

void draw_experiential(Layer* layer, GContext* ctx) {
    if (!is_active) {
        return;
    }

    // Draw the emoji in center
    // The current experiential image will be drawn by main.c on top of this
    if (emoji_image) {
        gdraw_command_image_draw(ctx, emoji_image, layout.current_icon_pos);
    }
} 