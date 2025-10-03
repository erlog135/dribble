#include "dcim.h"
#include <pebble.h>

//only way i know to check if an image is precise 
//is to check if any of the points (in int16 representation) 
//are outside the image bounds

bool is_draw_command_image_precise(GDrawCommandImage* draw_command_image) {
    GDrawCommandList* cmd_list = gdraw_command_image_get_command_list(draw_command_image);
    GSize img_size = gdraw_command_image_get_bounds_size(draw_command_image);
    
    size_t num_commands = gdraw_command_list_get_num_commands(cmd_list);
    for(size_t cmd_idx = 0; cmd_idx < num_commands; cmd_idx++) {
        GDrawCommand* cmd = gdraw_command_list_get_command(cmd_list, cmd_idx);
        size_t num_points = gdraw_command_get_num_points(cmd);
        
        for(size_t pt_idx = 0; pt_idx < num_points; pt_idx++) {
            GPoint point = gdraw_command_get_point(cmd, pt_idx);
            if(point.x >= img_size.w || point.y >= img_size.h || point.x < 0 || point.y < 0) {
                return true;
            }
        }
    }
    
    return false;
}

void dcim_flip_horizontal(GDrawCommandImage* image) {
    if (!image) {
        return;
    }

    // Get the command list from the image
    GDrawCommandList* cmd_list = gdraw_command_image_get_command_list(image);
    if (!cmd_list) {
        return;
    }

    // Get the bounds size
    GSize bounds = gdraw_command_image_get_bounds_size(image);
    bool precise = is_draw_command_image_precise(image);
    
    if (precise) {
        bounds.w = bounds.w << 3;
        bounds.h = bounds.h << 3;
    }

    // Get the number of commands
    uint32_t num_commands = gdraw_command_list_get_num_commands(cmd_list);

    // Flip each command's points across the vertical axis
    for (uint32_t i = 0; i < num_commands; i++) {
        GDrawCommand* cmd = gdraw_command_list_get_command(cmd_list, i);
        if (!cmd) continue;

        // Get number of points in the command
        uint32_t num_points = gdraw_command_get_num_points(cmd);

        // Flip each point
        for (uint32_t j = 0; j < num_points; j++) {
            GPoint point = gdraw_command_get_point(cmd, j);
            GPoint new_point = {
                .x = bounds.w - point.x,
                .y = point.y
            };
            gdraw_command_set_point(cmd, j, new_point);
        }
    }
}

void dcim_flip_vertical(GDrawCommandImage* image) {
    if (!image) {
        return;
    }

    // Get the command list from the image
    GDrawCommandList* cmd_list = gdraw_command_image_get_command_list(image);
    if (!cmd_list) {
        return;
    }

    // Get the bounds size
    GSize bounds = gdraw_command_image_get_bounds_size(image);
    bool precise = is_draw_command_image_precise(image);
    
    if (precise) {
        bounds.w = bounds.w << 3;
        bounds.h = bounds.h << 3;
    }

    // Get the number of commands
    uint32_t num_commands = gdraw_command_list_get_num_commands(cmd_list);

    // Flip each command's points across the horizontal axis
    for (uint32_t i = 0; i < num_commands; i++) {
        GDrawCommand* cmd = gdraw_command_list_get_command(cmd_list, i);
        if (!cmd) continue;

        // Get number of points in the command
        uint32_t num_points = gdraw_command_get_num_points(cmd);

        // Flip each point
        for (uint32_t j = 0; j < num_points; j++) {
            GPoint point = gdraw_command_get_point(cmd, j);
            GPoint new_point = {
                .x = point.x,
                .y = bounds.h - point.y
            };
            gdraw_command_set_point(cmd, j, new_point);
        }
    }
}

void dcim_transpose(GDrawCommandImage* image) {
    if (!image) {
        return;
    }

    // Get the command list from the image
    GDrawCommandList* cmd_list = gdraw_command_image_get_command_list(image);
    if (!cmd_list) {
        return;
    }

    // Get the bounds size
    GSize bounds = gdraw_command_image_get_bounds_size(image);
    bool precise = is_draw_command_image_precise(image);
    
    if (precise) {
        bounds.w = bounds.w << 3;
        bounds.h = bounds.h << 3;
    }

    // Get the number of commands
    uint32_t num_commands = gdraw_command_list_get_num_commands(cmd_list);

    // Transpose each command's points
    for (uint32_t i = 0; i < num_commands; i++) {
        GDrawCommand* cmd = gdraw_command_list_get_command(cmd_list, i);
        if (!cmd) continue;

        // Get number of points in the command
        uint32_t num_points = gdraw_command_get_num_points(cmd);

        // Transpose each point
        for (uint32_t j = 0; j < num_points; j++) {
            GPoint point = gdraw_command_get_point(cmd, j);
            GPoint new_point = {
                .x = point.y,
                .y = point.x
            };
            gdraw_command_set_point(cmd, j, new_point);
        }
    }
}

void dcim_8angle_from_src(GDrawCommandImage** target_image, uint8_t direction,
                          GDrawCommandImage* source_image, GDrawCommandImage* angled_source_image) {
    // Normalize direction to 0-7
    direction = direction % 8;

    // Free existing target image if it exists
    if (*target_image) {
        gdraw_command_image_destroy(*target_image);
    }

    // For diagonal directions (1, 3, 5, 7), use the angled source image
    if (direction % 2 == 1) {
        if (!angled_source_image) return;
        *target_image = gdraw_command_image_clone(angled_source_image);
        if (!*target_image) return;

        // Apply transformations based on which diagonal
        switch (direction) {
            case 1: // Bottom-right (45°)
                // Use as is
                break;
            case 3: // Bottom-left (135°)
                dcim_flip_horizontal(*target_image);
                break;
            case 5: // Top-left (225°)
                dcim_flip_horizontal(*target_image);
                dcim_flip_vertical(*target_image);
                break;
            case 7: // Top-right (315°)
                dcim_flip_vertical(*target_image);
                break;
        }
    } else {
        // For cardinal directions (0, 2, 4, 6), use the regular source image
        if (!source_image) return;
        *target_image = gdraw_command_image_clone(source_image);
        if (!*target_image) return;

        // Source image points right (0 degrees)
        // We need to rotate it to match the desired direction
        switch (direction) {
            case 0: // Right (0°)
                // Use as is
                break;
            case 2: // Down (90°)
                dcim_transpose(*target_image);
                break;
            case 4: // Left (180°)
                dcim_flip_horizontal(*target_image);
                break;
            case 6: // Up (270°)
                dcim_transpose(*target_image);
                dcim_flip_vertical(*target_image);
                break;
        }
    }
}