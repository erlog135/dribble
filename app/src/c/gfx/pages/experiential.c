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

static GDrawCommandImage* emoji_image;

static void update_icons();

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

static void release_emoji(void) {
    if (emoji_image) {
        gdraw_command_image_destroy(emoji_image);
        emoji_image = NULL;
    }
}

void set_experiential_view(int hour) {
    is_active = (hour >= 0);
    selected_hour = hour;

    release_owned_slots();

    if (!is_active) {
        release_emoji();
        return;
    }

    update_icons();

    // The emoji is the same all session; keep it loaded while the page is
    // active and only load it on entry.
    if (!emoji_image) {
        emoji_image = load_random_emoji_image();
    }
    if (hour > 0) {
        owned_slots[0] = load_experiential_image(forecast_hours[hour - 1].experiential_icon, false);
    }
    owned_slots[1] = load_experiential_image(forecast_hours[hour].experiential_icon, true);
    if (hour < LAST_FORECAST_HOUR) {
        owned_slots[2] = load_experiential_image(forecast_hours[hour + 1].experiential_icon, false);
    }
    *prev_image_ref = owned_slots[0];
    *current_image_ref = owned_slots[1];
    *next_image_ref = owned_slots[2];
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

    return experiential_layer;
}

void deinit_experiential_layers(void) {
    release_owned_slots();
    release_emoji();
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
        gdraw_command_image_draw(ctx, emoji_image, LAYOUT_CUR_ICON_POS);
    }
} 