#include "precip_animation.h"

#ifndef PBL_PLATFORM_APLITE

#define PRECIP_ANIM_DURATION_MS  450
#define PRECIP_MAX_DATA_POINTS   13

static Animation*         s_animation       = NULL;
static Layer*             s_layer           = NULL;
static GPathInfo*         s_path_info       = NULL;
static int                s_num_points      = 0;
static int16_t            s_target_y[PRECIP_MAX_DATA_POINTS];
static int16_t            s_bottom_y        = 0;
static AnimationProgress  s_progress        = 0;

// Snap all animated points to their final target values.
static void snap_to_targets(void) {
    if (!s_path_info) return;
    for (int i = 0; i < s_num_points; i++) {
        s_path_info->points[i].y = s_target_y[i];
    }
}

static void precip_anim_update(Animation* animation,
                               const AnimationProgress progress) {
    if (!s_path_info) return;
    s_progress = progress;
    for (int i = 0; i < s_num_points; i++) {
        int16_t delta = s_target_y[i] - s_bottom_y;
        s_path_info->points[i].y =
            s_bottom_y + (int16_t)((int32_t)delta * progress / ANIMATION_NORMALIZED_MAX);
    }
    if (s_layer) {
        layer_mark_dirty(s_layer);
    }
}

static void precip_anim_teardown(Animation* animation) {
    s_animation = NULL;
    s_progress  = ANIMATION_NORMALIZED_MAX;
    snap_to_targets();
    if (s_layer) {
        layer_mark_dirty(s_layer);
    }
}

static const AnimationImplementation s_implementation = {
    .update   = precip_anim_update,
    .teardown = precip_anim_teardown,
};

void precip_animation_start(Layer* layer, GPathInfo* path_info,
                             int num_data_points, int16_t bottom_y) {
    precip_animation_stop();

    s_layer      = layer;
    s_path_info  = path_info;
    s_progress   = 0;
    s_num_points = (num_data_points < PRECIP_MAX_DATA_POINTS)
                     ? num_data_points : PRECIP_MAX_DATA_POINTS;
    s_bottom_y   = bottom_y;

    // Snapshot target Y values then reset each point to the baseline.
    for (int i = 0; i < s_num_points; i++) {
        s_target_y[i]          = path_info->points[i].y;
        path_info->points[i].y = bottom_y;
    }

    s_animation = animation_create();
    animation_set_duration(s_animation, PRECIP_ANIM_DURATION_MS);
    animation_set_curve(s_animation, AnimationCurveEaseOut);
    animation_set_implementation(s_animation, &s_implementation);
    animation_schedule(s_animation);
}

void precip_animation_stop(void) {
    if (s_animation) {
        Animation* anim = s_animation;
        s_animation = NULL;
        s_progress  = ANIMATION_NORMALIZED_MAX;
        animation_unschedule(anim);
        // Teardown is not called on unschedule, so finalize manually.
        snap_to_targets();
        if (s_layer) {
            layer_mark_dirty(s_layer);
        }
    }
}

void precip_animation_deinit(void) {
    precip_animation_stop();
    s_layer     = NULL;
    s_path_info = NULL;
}

bool precip_animation_is_active(void) {
    return s_animation != NULL;
}

AnimationProgress precip_animation_get_progress(void) {
    return s_progress;
}

#endif // PBL_PLATFORM_APLITE
