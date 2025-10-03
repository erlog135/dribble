#pragma once

#include <pebble.h>

// Layout structure to hold all UI element positions and dimensions
typedef struct {
    // Screen dimensions
    int16_t screen_width;
    int16_t screen_height;
    bool is_round;
    bool is_pixel_dense;
    
    // Padding values
    int16_t padding_top;
    int16_t padding_bottom;
    int16_t padding_left;
    int16_t padding_right;
    
    // Text layer dimensions
    int16_t text_height;
    int16_t text_width;
    
    // Icon dimensions
    int16_t icon_small;
    int16_t icon_large;
    
    // Precipitation graph dimensions
    int16_t precipitation_graph_width;
    int16_t precipitation_graph_height;
    
    // Text layer positions
    GPoint prev_time_pos;
    GPoint current_time_pos;
    GPoint current_text_pos;
    GPoint next_time_pos;
    
    // Icon positions
    GPoint prev_icon_pos;
    GPoint current_icon_pos;
    GPoint next_icon_pos;
    
    // Precipitation graph position
    GPoint precipitation_graph_pos;
    
    // Axis image positions (for precipitation axis)
    GPoint axis_small_pos;  // 25x10 axis image position
    GPoint axis_large_pos;  // 86x10 axis image position
    
    // Text layer bounds
    GRect prev_time_bounds;
    GRect current_time_bounds;
    GRect current_text_bounds;
    GRect next_time_bounds;
    
    // Icon bounds
    GRect prev_icon_bounds;
    GRect current_icon_bounds;
    GRect next_icon_bounds;
    
    // Precipitation graph bounds
    GRect precipitation_graph_bounds;
    
    // Axis image bounds
    GRect axis_small_bounds;  // 25x10 axis image bounds
    GRect axis_large_bounds;  // 86x10 axis image bounds
    
    // Splash screen layout
    GRect splash_image_bounds;      // Top third for image
    GRect splash_text_bounds;       // Bottom area for status text
    GPoint splash_image_center;     // Center point for image positioning
} Layout;

// Global layout instance
Layout layout;

// Initialize the layout based on provided screen parameters
static void layout_init(int16_t screen_width, int16_t screen_height, bool is_round, bool is_pixel_dense) {
    // Set screen parameters
    layout.screen_width = screen_width;
    layout.screen_height = screen_height;
    layout.is_round = is_round;
    layout.is_pixel_dense = is_pixel_dense;

    // Set padding values locally
    layout.padding_top = 4;
    layout.padding_bottom = 4;

    if(is_round) {
        layout.padding_left = 12;
    } else {
        layout.padding_left = 6;
    }

    if (is_round) {
        layout.padding_right = 12;
    } else {
        layout.padding_right = 6;
    }
    
    // Set text dimensions
    layout.text_height = 20;
    layout.text_width = layout.screen_width - layout.padding_left - layout.padding_right;
    
    // Set icon dimensions
    layout.icon_small = 25;
    layout.icon_large = 50;
    
    // Set precipitation graph dimensions
    layout.precipitation_graph_width = 84;
    layout.precipitation_graph_height = 40;
    
    // Calculate text positions (left padding affects x values for all text)
    if (layout.is_round) {
        // For round screens, prev and next time are positioned off-screen vertically
        layout.prev_time_pos = GPoint(layout.padding_left, -layout.text_height);
        layout.current_time_pos = GPoint(layout.padding_left, layout.screen_height/2 - layout.text_height*2);
        layout.current_text_pos = GPoint(layout.padding_left, layout.screen_height/2 - layout.text_height);
        layout.next_time_pos = GPoint(layout.padding_left, layout.screen_height);
    } else {
        // For rectangular screens, use original positioning with padding
        layout.prev_time_pos = GPoint(layout.padding_left, layout.padding_top);
        layout.current_time_pos = GPoint(layout.padding_left, layout.screen_height/2 - layout.text_height*2);
        layout.current_text_pos = GPoint(layout.padding_left, layout.screen_height/2 - layout.text_height);
        layout.next_time_pos = GPoint(layout.padding_left, layout.screen_height - layout.text_height - layout.padding_bottom);
    }
    
    // Calculate icon positions (right padding affects x values for all icons)
    if (layout.is_round) {
        // For round screens, prev and next icons go to top and bottom center
        layout.prev_icon_pos = GPoint((layout.screen_width - layout.icon_small) / 2, layout.padding_top);
        layout.current_icon_pos = GPoint(layout.screen_width - layout.icon_large - layout.padding_right, 
                                         layout.screen_height/2 - layout.icon_large/2);
        layout.next_icon_pos = GPoint((layout.screen_width - layout.icon_small) / 2, 
                                      layout.screen_height - layout.icon_small - layout.padding_bottom);
    } else {
        // For rectangular screens, use original positioning with right padding
        layout.prev_icon_pos = GPoint(layout.screen_width - layout.icon_small - layout.padding_right, layout.padding_top);
        layout.current_icon_pos = GPoint(layout.screen_width - layout.icon_large - layout.padding_right, 
                                         layout.screen_height/2 - layout.icon_large/2);
        layout.next_icon_pos = GPoint(layout.screen_width - layout.icon_small - layout.padding_right, 
                                      layout.screen_height - layout.icon_small - layout.padding_bottom);
    }
    
    // Calculate precipitation graph position
    layout.precipitation_graph_pos = GPoint(
        layout.screen_width - layout.precipitation_graph_width - layout.padding_right,
        (layout.screen_height - layout.precipitation_graph_height) / 2
    );
    
    // Calculate axis image positions
    // Large axis (86x10) positioned beneath precipitation graph, centered horizontally with it
    layout.axis_large_pos = GPoint(
        layout.precipitation_graph_pos.x - 1, //stay in line with graph
        layout.precipitation_graph_pos.y + layout.precipitation_graph_height - 4  // 2px below graph
    );
    
    // Small axis (25x10) centered within prev icon position (25x25)
    layout.axis_small_pos = GPoint(
        layout.prev_icon_pos.x,  // Same X as 25x25 position (25x10 fits exactly in width)
        layout.prev_icon_pos.y + (layout.icon_small - 10) / 2  // Center vertically: (25-10)/2 = 7.5 ≈ 8
    );

    
    // Calculate text bounds
    if (layout.is_round) {
        // For round screens, prev and next time bounds are off-screen vertically
        layout.prev_time_bounds = GRect(layout.padding_left, -layout.text_height, layout.text_width, layout.text_height);
        layout.current_time_bounds = GRect(layout.padding_left, layout.screen_height/2 - layout.text_height*2, 
                                           layout.text_width, layout.text_height);
        layout.current_text_bounds = GRect(layout.padding_left, layout.screen_height/2 - layout.text_height, 
                                           layout.text_width, layout.text_height*3);
        layout.next_time_bounds = GRect(layout.padding_left, layout.screen_height, 
                                        layout.text_width, layout.text_height);
    } else {
        // For rectangular screens, use original positioning with padding
        layout.prev_time_bounds = GRect(layout.padding_left, layout.padding_top, layout.text_width, layout.text_height);
        layout.current_time_bounds = GRect(layout.padding_left, layout.screen_height/2 - layout.text_height*2, 
                                           layout.text_width, layout.text_height);
        layout.current_text_bounds = GRect(layout.padding_left, layout.screen_height/2 - layout.text_height, 
                                           layout.text_width, layout.text_height*3);
        layout.next_time_bounds = GRect(layout.padding_left, layout.screen_height - layout.text_height - layout.padding_bottom, 
                                        layout.text_width, layout.text_height);
    }
    
    // Calculate icon bounds
    if (layout.is_round) {
        // For round screens, prev and next icons are centered horizontally
        layout.prev_icon_bounds = GRect((layout.screen_width - layout.icon_small) / 2, layout.padding_top, 
                                        layout.icon_small, layout.icon_small);
        layout.current_icon_bounds = GRect(layout.screen_width - layout.icon_large - layout.padding_right, 
                                           layout.screen_height/2 - layout.icon_large/2, 
                                           layout.icon_large, layout.icon_large);
        layout.next_icon_bounds = GRect((layout.screen_width - layout.icon_small) / 2, 
                                        layout.screen_height - layout.icon_small - layout.padding_bottom, 
                                        layout.icon_small, layout.icon_small);
    } else {
        // For rectangular screens, use original positioning with right padding
        layout.prev_icon_bounds = GRect(layout.screen_width - layout.icon_small - layout.padding_right, layout.padding_top, 
                                        layout.icon_small, layout.icon_small);
        layout.current_icon_bounds = GRect(layout.screen_width - layout.icon_large - layout.padding_right, 
                                           layout.screen_height/2 - layout.icon_large/2, 
                                           layout.icon_large, layout.icon_large);
        layout.next_icon_bounds = GRect(layout.screen_width - layout.icon_small - layout.padding_right, 
                                        layout.screen_height - layout.icon_small - layout.padding_bottom, 
                                        layout.icon_small, layout.icon_small);
    }
    
    // Calculate precipitation graph bounds
    layout.precipitation_graph_bounds = GRect(
        layout.precipitation_graph_pos.x,
        layout.precipitation_graph_pos.y,
        layout.precipitation_graph_width,
        layout.precipitation_graph_height
    );
    
    // Calculate axis image bounds
    layout.axis_small_bounds = GRect(
        layout.axis_small_pos.x,
        layout.axis_small_pos.y,
        25,  // 25x10 axis image
        10
    );
    
    layout.axis_large_bounds = GRect(
        layout.axis_large_pos.x,
        layout.axis_large_pos.y,
        86,  // 86x10 axis image  
        10
    );
    
    // Calculate splash screen layout
    // Image area: top 2/3 of screen
    layout.splash_image_bounds = GRect(
        0,
        0,
        layout.screen_width,
        layout.screen_height * 2 / 3
    );
    
    // Text area: bottom 1/3 of screen with padding
    layout.splash_text_bounds = GRect(
        layout.padding_left,
        layout.screen_height * 2 / 3,
        layout.screen_width - layout.padding_left - layout.padding_right,
        layout.screen_height / 3
    );
    
    // Center point for 80x80 image in the top 2/3 area
    layout.splash_image_center = GPoint(
        layout.screen_width / 2,
        (layout.screen_height * 2 / 3) / 2
    );
}
