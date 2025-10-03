#include <pebble.h>

#include "kimaybe.h"
#include "transform.h"
#include "../dcim.h"

// Conditional logging for transform module
// Uncomment the line below to enable transform debug logging
// #define TRANSFORM_LOGGING

#ifdef TRANSFORM_LOGGING
  #define TRANSFORM_LOG(level, fmt, ...) APP_LOG(level, fmt, ##__VA_ARGS__)
#else
  #define TRANSFORM_LOG(level, fmt, ...)
#endif

// Duration to delay ratio: delay = duration * KM_DURATION_DELAY_RATIO
//or, the offset of each slice animation relative to the total duration
//lower to make more uniform, and higher to make it more stretchy
#define KM_DURATION_DELAY_RATIO 0.15f

static void add_point_to_slice(KMAnimationPoint* point, int slice_index, KMAnimation* kmanim) {
    if (!point || !kmanim || slice_index < 0 || slice_index >= KM_LINEAR_SLICES) {
        return;
    }
    point->next = kmanim->slices[slice_index];
    kmanim->slices[slice_index] = point;
}

// Improved slice index lookup with better validation
int kmanim_get_animation_slice_index(KMAnimation* kmanim, Animation* animation){
    if (!kmanim || !animation || !kmanim->slice_animations) {
        return -1;
    }
    
    for (int i = 0; i < KM_LINEAR_SLICES; i++){
        if (kmanim->slice_animations[i] == animation){
            return i;
        }
    }
    return -1;
}

static void implementation_setup(Animation* animation) {
    // Get the KMAnimation context from the animation
    KMAnimation* kmanim = (KMAnimation*)animation_get_context(animation);
    if (kmanim == NULL) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation setup: null context for animation %p", (void*)animation);
        return;
    }
    
    // Validate that this animation actually belongs to this KMAnimation
    int slice_index = kmanim_get_animation_slice_index(kmanim, animation);
    if (slice_index < 0) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation setup: animation %p not found in context %p", (void*)animation, (void*)kmanim);
        return;
    }
    
    if (slice_index == 0) {
        TRANSFORM_LOG(APP_LOG_LEVEL_INFO, "KMAnimation %p started!", (void*)kmanim);
    }
}

static void implementation_update(Animation* animation, const AnimationProgress progress) {
    // Get the KMAnimation context from the animation
    KMAnimation* kmanim = (KMAnimation*)animation_get_context(animation);
    if (kmanim == NULL) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation update: null context for animation %p", (void*)animation);
        return;
    }

    // Validate that this animation actually belongs to this KMAnimation
    int slice_index = kmanim_get_animation_slice_index(kmanim, animation);
    if (slice_index < 0) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation update: animation %p not found in context %p", (void*)animation, (void*)kmanim);
        return;
    }

    // Additional validation
    if (!kmanim->slices || slice_index >= KM_LINEAR_SLICES) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation update: invalid slice_index %d for context %p", slice_index, (void*)kmanim);
        return;
    }

    KMAnimationPoint* current_km_point = kmanim->slices[slice_index];

    while (current_km_point != NULL) {
        // Validate the animation point
        if (!current_km_point->draw_command) {
            TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation update: null draw_command in point");
            current_km_point = current_km_point->next;
            continue;
        }

        // Calculate current point position based on progress
        int16_t curr_x = current_km_point->start.x + ((current_km_point->end.x - current_km_point->start.x) * progress) / ANIMATION_NORMALIZED_MAX;
        int16_t curr_y = current_km_point->start.y + ((current_km_point->end.y - current_km_point->start.y) * progress) / ANIMATION_NORMALIZED_MAX;
        current_km_point->current = GPoint(curr_x, curr_y);

        // Safely update the draw command point
        gdraw_command_set_point(current_km_point->draw_command, current_km_point->point_index, current_km_point->current);
        current_km_point = current_km_point->next;

        //TODO: update line width 
    }

    // Safely mark layer dirty
    if (kmanim->draw_layer) {
        layer_mark_dirty(kmanim->draw_layer);
    }
}

static void implementation_teardown(Animation* animation) {
    // Get the KMAnimation context from the animation
    KMAnimation* kmanim = (KMAnimation*)animation_get_context(animation);
    if (kmanim == NULL) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation teardown: null context for animation %p", (void*)animation);
        return;
    }
    
    // Validate that this animation actually belongs to this KMAnimation
    int slice_index = kmanim_get_animation_slice_index(kmanim, animation);
    if (slice_index < 0) {
        TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "KMAnimation teardown: animation %p not found in context %p", (void*)animation, (void*)kmanim);
        return;
    }
    
    if (slice_index == KM_LINEAR_SLICES /2) {
        TRANSFORM_LOG(APP_LOG_LEVEL_INFO, "KMAnimation %p finished!", (void*)kmanim);
        
        // Safely call the finished callback
        if (kmanim->finished_callback != NULL) {
            void (*callback)(void) = kmanim->finished_callback;
            kmanim->finished_callback = NULL;  // Clear it first to prevent double-calls
            callback();
        }
    }
}

static const AnimationImplementation implementation = {
  .setup = implementation_setup,
  .update = implementation_update,
  .teardown = implementation_teardown
};

//get all the init info from the km animation and populate its slices
static KMAnimationPoint** prep_slices(KMAnimation* kmanim, GRect from, GRect to, SweepDirection direction, TransformationType type){
  if (!kmanim) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: null kmanim");
    return NULL;
  }

  if (kmanim->slices != NULL) {
    return kmanim->slices;
  }

  if (!kmanim->draw_command_image) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: null draw_command_image in kmanim %p", (void*)kmanim);
    return NULL;
  }

  //use this from DCIM
  bool precise_points = is_draw_command_image_precise(kmanim->draw_command_image);

  //initialize linked list of points for every slice
  kmanim->slices = calloc(KM_LINEAR_SLICES, sizeof(KMAnimationPoint*));
  if (!kmanim->slices) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: failed to allocate slices for kmanim %p", (void*)kmanim);
    return NULL;
  }
  
  GSize bounds = gdraw_command_image_get_bounds_size(kmanim->draw_command_image);

  float start_scale = (float)from.size.w / (float)bounds.w;
  float end_scale = (float)to.size.w / (float)bounds.w;

  if (precise_points){
    //shift from and to left 3 bits, to make them 13.3 fixed point
    from.origin.x = from.origin.x << 3;
    from.origin.y = from.origin.y << 3;
    from.size.w = from.size.w << 3;
    from.size.h = from.size.h << 3;

    to.origin.x = to.origin.x << 3;
    to.origin.y = to.origin.y << 3;
    to.size.w = to.size.w << 3;
    to.size.h = to.size.h << 3;
  }

  //this might matter, maybe
  uint16_t slice_size;
  if (direction == KM_SWEEP_LEFT || direction == KM_SWEEP_RIGHT){
    slice_size = bounds.w / KM_LINEAR_SLICES;
  } else {
    slice_size = bounds.h / KM_LINEAR_SLICES;
  }

  if (slice_size == 0) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: slice_size is 0, bounds: %dx%d", bounds.w, bounds.h);
    free(kmanim->slices);
    kmanim->slices = NULL;
    return NULL;
  }

  //also pretend points are relative to origin
  //^ actually i think they are already

  //iterate through points, and divide their values by the slice size to get the slice index

  GDrawCommandList* commands = gdraw_command_image_get_command_list(kmanim->draw_command_image);
  if (!commands) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: null command list for kmanim %p", (void*)kmanim);
    free(kmanim->slices);
    kmanim->slices = NULL;
    return NULL;
  }

  uint32_t num_commands = gdraw_command_list_get_num_commands(commands);

  // okay, now we can iterate through commands, and for each command, iterate through points

  for (uint32_t i = 0; i < num_commands; i++) {

    GDrawCommand* command = gdraw_command_list_get_command(commands, i);
    if (!command) {
      TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "prep_slices: null command at index %d", (int)i);
      continue;
    }

    uint32_t num_points = gdraw_command_get_num_points(command);

    for (uint32_t j = 0; j < num_points; j++) {
      GPoint point = gdraw_command_get_point(command, j);

      //now, to make a KMAnimationPoint
      KMAnimationPoint* anim_point = malloc(sizeof(KMAnimationPoint));
      if (!anim_point) {
        TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "prep_slices: failed to allocate animation point");
        // Continue with other points rather than failing completely
        continue;
      }

      anim_point->draw_command = command;
      anim_point->point_index = j;
      anim_point->start = GPoint(point.x * start_scale + from.origin.x, point.y * start_scale + from.origin.y);
      anim_point->end = GPoint(point.x * end_scale + to.origin.x, point.y * end_scale + to.origin.y);
      anim_point->current = anim_point->start;
      anim_point->next = NULL;

      gdraw_command_set_point(command, j, anim_point->start);

      if (precise_points){
        //shift right by 3 bits to move the point to the nearest integer
        //so it can be indexed
        point.x = point.x >> 3;
        point.y = point.y >> 3;
      }

      //divide value of interest by slice size to get slice index
      int slice_index = 0;
      if (direction == KM_SWEEP_LEFT){
        slice_index = point.x / slice_size;
      } else if (direction == KM_SWEEP_RIGHT){
        slice_index = KM_LINEAR_SLICES - 1 - (point.x / slice_size);
      } else if (direction == KM_SWEEP_UP){
        slice_index = KM_LINEAR_SLICES - 1 - (point.y / slice_size);
      } else if (direction == KM_SWEEP_DOWN){
        slice_index = point.y / slice_size;
      }

      //clamp slice index to 0 through KM_LINEAR_SLICES-1 for points that somehow get out of bounds
      if (slice_index < 0){
        slice_index = 0;
      } else if (slice_index >= KM_LINEAR_SLICES){
        slice_index = KM_LINEAR_SLICES - 1;
      }

      //APP_LOG(APP_LOG_LEVEL_INFO, "Point: %d, %d added to Slice index: %d", point.x, point.y, slice_index);

      //add point to slice
      add_point_to_slice(anim_point, slice_index, kmanim);

    }
  }

  if (kmanim->draw_layer) {
    layer_mark_dirty(kmanim->draw_layer);
  }

  return kmanim->slices;
}

KMAnimation* km_make_transformation_kmanimation(Layer* layer, GDrawCommandImage* draw_command_image, GRect from, GRect to, SweepDirection direction, int duration, TransformationType type) {
    
    if (!layer || !draw_command_image) {
      TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: null layer or draw_command_image");
      return NULL;
    }

    if (duration <= 0) {
      TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: invalid duration %d", duration);
      return NULL;
    }

    KMAnimation* kmanim = malloc(sizeof(KMAnimation));
    if (!kmanim) {
      TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: failed to allocate KMAnimation");
      return NULL;
    }

    kmanim->draw_layer = layer;
    kmanim->draw_command_image = draw_command_image;
    kmanim->slices = NULL;
    kmanim->finished_callback = NULL;
    
    kmanim->slice_animations = malloc(sizeof(Animation*) * KM_LINEAR_SLICES);
    if (!kmanim->slice_animations) {
      TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: failed to allocate slice_animations");
      free(kmanim);
      return NULL;
    }

    // Initialize all pointers to NULL first
    for (int i = 0; i < KM_LINEAR_SLICES; i++) {
      kmanim->slice_animations[i] = NULL;
    }

    // Calculate slice duration based on total duration
    int slice_duration = duration / KM_LINEAR_SLICES;
    if (slice_duration <= 0) {
      slice_duration = 1; // Minimum duration
    }
    
    // Calculate delay based on duration ratio
    int slice_delay = (int)(duration * KM_DURATION_DELAY_RATIO);

    for (int i = 0; i < KM_LINEAR_SLICES; i++){
      kmanim->slice_animations[i] = animation_create();
      if (!kmanim->slice_animations[i]) {
        TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: failed to create animation %d", i);
        // Clean up previously created animations
        for (int j = 0; j < i; j++) {
          if (kmanim->slice_animations[j]) {
            animation_destroy(kmanim->slice_animations[j]);
          }
        }
        free(kmanim->slice_animations);
        free(kmanim);
        return NULL;
      }

      animation_set_implementation(kmanim->slice_animations[i], &implementation);
      animation_set_duration(kmanim->slice_animations[i], slice_duration);
      animation_set_delay(kmanim->slice_animations[i], slice_delay * i);
      
      // Set the KMAnimation as the context for this slice animation
      animation_set_handlers(kmanim->slice_animations[i], (AnimationHandlers) {
        .started = NULL,
        .stopped = NULL
      }, kmanim);
    }

    if (!prep_slices(kmanim, from, to, direction, type)) {
      TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_make_transformation_kmanimation: prep_slices failed");
      km_dispose_kmanimation(kmanim);
      return NULL;
    }

    TRANSFORM_LOG(APP_LOG_LEVEL_DEBUG, "Created KMAnimation %p with %d slices", (void*)kmanim, KM_LINEAR_SLICES);
    return kmanim;
}

void km_start_kmanimation(KMAnimation* kmanim, void (*callback)(void)){

  if (!kmanim) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_start_kmanimation: null kmanim");
    return;
  }

  if (!kmanim->slice_animations) {
    TRANSFORM_LOG(APP_LOG_LEVEL_ERROR, "km_start_kmanimation: null slice_animations in kmanim %p", (void*)kmanim);
    return;
  }

  // Store the callback
  kmanim->finished_callback = callback;
  
  TRANSFORM_LOG(APP_LOG_LEVEL_DEBUG, "Starting KMAnimation %p with %d slice animations", (void*)kmanim, KM_LINEAR_SLICES);
  
  for (int i = 0; i < KM_LINEAR_SLICES; i++){
    if (kmanim->slice_animations[i]) {
      animation_schedule(kmanim->slice_animations[i]);
    } else {
      TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "km_start_kmanimation: null slice animation at index %d", i);
    }
  }
}

void km_dispose_kmanimation(KMAnimation* kmanim){

  if (!kmanim) {
    TRANSFORM_LOG(APP_LOG_LEVEL_WARNING, "km_dispose_kmanimation: null kmanim");
    return;
  }

  TRANSFORM_LOG(APP_LOG_LEVEL_DEBUG, "Disposing KMAnimation %p", (void*)kmanim);

  // Clear the callback to prevent it from being called during cleanup
  kmanim->finished_callback = NULL;

  //free slices and their points
  if (kmanim->slices) {
    for (int i = 0; i < KM_LINEAR_SLICES; i++){
      KMAnimationPoint* current_point = kmanim->slices[i];
      while (current_point != NULL){
        KMAnimationPoint* next_point = current_point->next;
        free(current_point);
        current_point = next_point;
      }
    }

    //free slices
    free(kmanim->slices);
    kmanim->slices = NULL;
  }

  //free animations - need to be careful here as they might still be running or nonexistent
  //something about this makes app mad
  if (kmanim->slice_animations) {
    for (int i = 0; i < KM_LINEAR_SLICES; i++){
      if (kmanim->slice_animations[i]) {
        // Unschedule the animation before destroying it
        TRANSFORM_LOG(APP_LOG_LEVEL_DEBUG, "Unscheduling animation %d", i);
        if(animation_is_scheduled(kmanim->slice_animations[i])){
          animation_unschedule(kmanim->slice_animations[i]);
          animation_destroy(kmanim->slice_animations[i]);
        }
        kmanim->slice_animations[i] = NULL;
      }
    }
    free(kmanim->slice_animations);
    kmanim->slice_animations = NULL;
  }

  //free animation
  free(kmanim);
}
