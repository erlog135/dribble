#pragma once

//put all funny animation stuff here - it should be really easy to make PDC images do silly things

#include <pebble.h>

#define KM_LINEAR_SLICES 4
#define KM_RADIAL_SLICES 8

#define KM_MAX_PTS 256
#define KM_ANIMATION_DURATION_MS 1000

typedef enum {
    // Linear sweep directions
    KM_SWEEP_LEFT,
    KM_SWEEP_RIGHT,
    KM_SWEEP_UP,
    KM_SWEEP_DOWN,
    
    // Radial sweep directions, will eventually be for something maybe
    KM_SWEEP_CLOCKWISE,
    KM_SWEEP_COUNTERCLOCKWISE,
    KM_SWEEP_SIMULTANEOUS
} SweepDirection;

typedef enum {
    KM_TRANSLATE,
    KM_SCALE,
    KM_TRANSLATE_AND_SCALE
} TransformationType;

typedef struct KMAnimationPoint {
    GDrawCommand* draw_command;
    uint16_t point_index;
    GPoint start;
    GPoint end; 
    GPoint current; //maybe not needed
    struct KMAnimationPoint* next;
} KMAnimationPoint;

typedef struct {
    Layer* draw_layer;
    GDrawCommandImage* draw_command_image;
    KMAnimationPoint** slices;
    Animation** slice_animations;
    void (*finished_callback)(void);
} KMAnimation;
