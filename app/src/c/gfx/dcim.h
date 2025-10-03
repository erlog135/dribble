/**
 * DCIM - Draw Command Image Manipulator
 * ====================================
 * 
 * A utility library for manipulating Pebble GDrawCommandImage objects.
 * 
 * This library provides functions to flip, transpose and transform PDC images
 * while preserving their vector drawing commands. It handles both regular and
 * precise-coordinate PDC images correctly.
 * 
 * Key Features:
 * ------------
 * - Vertical flipping (mirror across Y axis)
 * - Horizontal flipping (mirror across X axis) 
 * - Transposition (swap X/Y coordinates)
 * - Proper handling of precise-coordinate PDCs
 * 
 * Usage:
 * ------
 * ```c
 * // Load a PDC resource
 * GDrawCommandImage* my_image = gdraw_command_image_create_with_resource(RESOURCE_ID);
 * 
 * // Flip it horizontally 
 * dcim_flip_horizontal(my_image); // Modifies image in place
 * ```
 */

#pragma once

#include <pebble.h>

/**
 * @brief Checks if a GDrawCommandImage uses precise coordinates
 * 
 * @param draw_command_image The image to check
 * @return true if the image uses precise coordinates, false otherwise
 */
bool is_draw_command_image_precise(GDrawCommandImage* draw_command_image);

/**
 * @brief Flips the image horizontally
 * 
 * @param image The GDrawCommandImage to flip
 */
void dcim_flip_horizontal(GDrawCommandImage* image);

/**
 * @brief Flips the image vertically
 * 
 * @param image The GDrawCommandImage to flip
 */
void dcim_flip_vertical(GDrawCommandImage* image);

/**
 * @brief Transposes an image by swapping x and y coordinates in place
 * 
 * @param image The image to transpose
 */
void dcim_transpose(GDrawCommandImage* image);

/**
 * @brief Updates an image to point in the specified direction
 * 
 * @param target_image Pointer to the target image pointer to update
 * @param direction Direction (0-7):
 *                 0: Right (0°)
 *                 1: Bottom-right (45°)
 *                 2: Down (90°)
 *                 3: Bottom-left (135°)
 *                 4: Left (180°)
 *                 5: Top-left (225°)
 *                 6: Up (270°)
 *                 7: Top-right (315°)
 * @param source_image Source image to use for cardinal directions
 * @param angled_source_image Source image to use for diagonal directions
 */
void dcim_8angle_from_src(GDrawCommandImage** target_image, uint8_t direction, 
                          GDrawCommandImage* source_image, GDrawCommandImage* angled_source_image);
