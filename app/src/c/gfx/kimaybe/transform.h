#pragma once

#include "kimaybe.h"

KMAnimation* km_make_transformation_kmanimation(Layer* layer, GDrawCommandImage* draw_command_image, GRect from, GRect to, SweepDirection direction, int duration, TransformationType type);

void km_start_kmanimation(KMAnimation* kmanim, void (*callback)(void));

void km_dispose_kmanimation(KMAnimation* kmanim);
