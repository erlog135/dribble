#pragma once

#include <pebble.h>

/**
 * Animates the Y coordinates of a GPathInfo's data points from a common
 * bottom value (the axis baseline) up to their pre-baked target values.
 * Call this immediately after baking the final Y values into the path.
 *
 * Only available on non-Aplite platforms.
 */

#ifndef PBL_PLATFORM_APLITE

/**
 * Start the precipitation graph rise animation.
 *
 * @param layer         The Layer to mark dirty on each animation frame.
 * @param path_info     The GPathInfo whose first `num_data_points` points will
 *                      be animated. The points must already contain the target
 *                      Y values; this function snapshots them, resets each to
 *                      `bottom_y`, and then eases them up to the targets.
 * @param num_data_points  Number of leading points in path_info to animate
 *                         (the trailing two closing-path points are left alone).
 * @param bottom_y      The Y value all points start from (axis baseline).
 */
void precip_animation_start(Layer* layer, GPathInfo* path_info,
                             int num_data_points, int16_t bottom_y);

/**
 * Stop any running precipitation animation, snapping all points to their
 * target values immediately.
 */
void precip_animation_stop(void);

/**
 * Release all resources. Call from deinit_conditions_layers().
 */
void precip_animation_deinit(void);

/**
 * Returns true while the rise animation is in progress.
 */
bool precip_animation_is_active(void);

/**
 * Returns the current animation progress (0 → ANIMATION_NORMALIZED_MAX).
 * Returns ANIMATION_NORMALIZED_MAX when no animation is running.
 */
AnimationProgress precip_animation_get_progress(void);

#endif // PBL_PLATFORM_APLITE
