#pragma once

#include <pebble.h>

// ── Screen ────────────────────────────────────────────────────────────────────
#define LAYOUT_W  PBL_DISPLAY_WIDTH
#define LAYOUT_H  PBL_DISPLAY_HEIGHT

// ── Pixel-dense platforms (Emery / Gabbro) ───────────────────────────────────
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  #define LAYOUT_PIXEL_DENSE 1
#else
  #define LAYOUT_PIXEL_DENSE 0
#endif

// ── Padding ───────────────────────────────────────────────────────────────────
// Scale layout spacing up 1.5x on pixel-dense displays.
#define LAYOUT_SCALE(v)  ((LAYOUT_PIXEL_DENSE) ? ((v) * 3 / 2) : (v))

#if defined(PBL_ROUND)
    #define LAYOUT_PAD_L  LAYOUT_SCALE(18)
    #define LAYOUT_PAD_R  LAYOUT_SCALE(18)
    #define LAYOUT_PAD_T  LAYOUT_SCALE(16)
    #define LAYOUT_PAD_B  LAYOUT_SCALE(16)
#else
  #define LAYOUT_PAD_L  LAYOUT_SCALE(6)
  #define LAYOUT_PAD_R  LAYOUT_SCALE(6)
  #define LAYOUT_PAD_T  LAYOUT_SCALE(4)
  #define LAYOUT_PAD_B  LAYOUT_SCALE(4)
#endif



// ── Text ──────────────────────────────────────────────────────────────────────
#if LAYOUT_PIXEL_DENSE
  #define LAYOUT_TEXT_H  30
#else
  #define LAYOUT_TEXT_H  20
#endif
#define LAYOUT_TEXT_W  (LAYOUT_W - LAYOUT_PAD_L - LAYOUT_PAD_R)

// ── Fonts ─────────────────────────────────────────────────────────────────────
#if defined(PBL_PLATFORM_APLITE)
  #define LAYOUT_TIME_FONT  FONT_KEY_GOTHIC_18_BOLD
#elif LAYOUT_PIXEL_DENSE
  #define LAYOUT_TIME_FONT  FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM
#else
  #define LAYOUT_TIME_FONT  FONT_KEY_LECO_20_BOLD_NUMBERS
#endif

#if LAYOUT_PIXEL_DENSE
  #define LAYOUT_TEXT_FONT  FONT_KEY_GOTHIC_28
#else
  #define LAYOUT_TEXT_FONT  FONT_KEY_GOTHIC_18
#endif

// ── Icons ─────────────────────────────────────────────────────────────────────
#define LAYOUT_ICON_SM  25
#define LAYOUT_ICON_LG  50

// ── Precipitation graph ───────────────────────────────────────────────────────
#define LAYOUT_PRECIP_W  84
#define LAYOUT_PRECIP_H  40
#define LAYOUT_PRECIP_X  (LAYOUT_W - LAYOUT_PRECIP_W - LAYOUT_PAD_R)
#define LAYOUT_PRECIP_Y  ((LAYOUT_H - LAYOUT_PRECIP_H) / 2)
#define LAYOUT_PRECIP_POS     GPoint(LAYOUT_PRECIP_X, LAYOUT_PRECIP_Y)
#define LAYOUT_PRECIP_BOUNDS  GRect(LAYOUT_PRECIP_X, LAYOUT_PRECIP_Y, LAYOUT_PRECIP_W, LAYOUT_PRECIP_H)

// ── Axis images ───────────────────────────────────────────────────────────────
// prev_icon x is needed by both the icon and the small axis, so expose it
#if defined(PBL_ROUND)
  #define LAYOUT_PREV_ICON_X  (LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_W / 4)
#else
  #define LAYOUT_PREV_ICON_X  (LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_PAD_R)
#endif
#define LAYOUT_PREV_ICON_Y  LAYOUT_PAD_T

// 25x10 axis centered on the prev-icon slot
#define LAYOUT_AXIS_SM_POS     GPoint(LAYOUT_PREV_ICON_X, LAYOUT_PREV_ICON_Y + (LAYOUT_ICON_SM - 10) / 2)
#define LAYOUT_AXIS_SM_BOUNDS  GRect(LAYOUT_PREV_ICON_X, LAYOUT_PREV_ICON_Y + (LAYOUT_ICON_SM - 10) / 2, 25, 10)

// 86x10 axis aligned with the left edge of the precipitation graph
#define LAYOUT_AXIS_LG_POS     GPoint(LAYOUT_PRECIP_X - 1, LAYOUT_PRECIP_Y + LAYOUT_PRECIP_H - 4)
#define LAYOUT_AXIS_LG_BOUNDS  GRect(LAYOUT_PRECIP_X - 1, LAYOUT_PRECIP_Y + LAYOUT_PRECIP_H - 4, 86, 10)

// ── Text positions & bounds ───────────────────────────────────────────────────
// Vertical offset to center LAYOUT_TEXT_H text within the LAYOUT_ICON_SM slot.
#define LAYOUT_ICON_SM_TEXT_OFFSET  ((LAYOUT_ICON_SM - LAYOUT_TEXT_H) / 2 - 3)

#if defined(PBL_ROUND)
  #define LAYOUT_ROUND_TEXT_W    (LAYOUT_W - (LAYOUT_W / 4) * 2)

  #define LAYOUT_PREV_TIME_POS   GPoint(LAYOUT_W / 4, LAYOUT_PAD_T + LAYOUT_ICON_SM_TEXT_OFFSET)
  #define LAYOUT_CUR_TIME_POS    GPoint(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H * 2)
  #define LAYOUT_CUR_TEXT_POS    GPoint(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H)
  #define LAYOUT_NEXT_TIME_POS   GPoint(LAYOUT_W / 4, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B + LAYOUT_ICON_SM_TEXT_OFFSET)

  #define LAYOUT_PREV_TIME_BOUNDS   GRect(LAYOUT_W / 4,  LAYOUT_PAD_T + LAYOUT_ICON_SM_TEXT_OFFSET,                                    LAYOUT_ROUND_TEXT_W, LAYOUT_TEXT_H)
  #define LAYOUT_CUR_TIME_BOUNDS    GRect(LAYOUT_PAD_L,  LAYOUT_H / 2 - LAYOUT_TEXT_H * 2,                                             LAYOUT_TEXT_W,       LAYOUT_TEXT_H)
  #define LAYOUT_CUR_TEXT_BOUNDS    GRect(LAYOUT_PAD_L,  LAYOUT_H / 2 - LAYOUT_TEXT_H,                                                 LAYOUT_TEXT_W,       LAYOUT_TEXT_H * 3)
  #define LAYOUT_NEXT_TIME_BOUNDS   GRect(LAYOUT_W / 4,  LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B + LAYOUT_ICON_SM_TEXT_OFFSET,        LAYOUT_ROUND_TEXT_W, LAYOUT_TEXT_H)
#else
  #define LAYOUT_PREV_TIME_POS   GPoint(LAYOUT_PAD_L, LAYOUT_PAD_T + LAYOUT_ICON_SM_TEXT_OFFSET)
  #define LAYOUT_CUR_TIME_POS    GPoint(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H * 2)
  #define LAYOUT_CUR_TEXT_POS    GPoint(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H)
  #define LAYOUT_NEXT_TIME_POS   GPoint(LAYOUT_PAD_L, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B + LAYOUT_ICON_SM_TEXT_OFFSET)

  #define LAYOUT_PREV_TIME_BOUNDS   GRect(LAYOUT_PAD_L, LAYOUT_PAD_T + LAYOUT_ICON_SM_TEXT_OFFSET,                                  LAYOUT_TEXT_W, LAYOUT_TEXT_H)
  #define LAYOUT_CUR_TIME_BOUNDS    GRect(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H * 2,                                          LAYOUT_TEXT_W, LAYOUT_TEXT_H)
  #define LAYOUT_CUR_TEXT_BOUNDS    GRect(LAYOUT_PAD_L, LAYOUT_H / 2 - LAYOUT_TEXT_H,                                              LAYOUT_TEXT_W, LAYOUT_TEXT_H * 3)
  #define LAYOUT_NEXT_TIME_BOUNDS   GRect(LAYOUT_PAD_L, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B + LAYOUT_ICON_SM_TEXT_OFFSET,     LAYOUT_TEXT_W, LAYOUT_TEXT_H)
#endif

// ── Icon positions & bounds ───────────────────────────────────────────────────
#if defined(PBL_ROUND)
  #define LAYOUT_PREV_ICON_POS   GPoint(LAYOUT_PREV_ICON_X, LAYOUT_PAD_T)
  #define LAYOUT_CUR_ICON_POS    GPoint(LAYOUT_W - LAYOUT_ICON_LG - LAYOUT_PAD_R, LAYOUT_H / 2 - LAYOUT_ICON_LG / 2)
  #define LAYOUT_NEXT_ICON_POS   GPoint(LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_W / 4, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B)

  #define LAYOUT_PREV_ICON_BOUNDS   GRect(LAYOUT_PREV_ICON_X,                       LAYOUT_PAD_T,                              LAYOUT_ICON_SM, LAYOUT_ICON_SM)
  #define LAYOUT_CUR_ICON_BOUNDS    GRect(LAYOUT_W - LAYOUT_ICON_LG - LAYOUT_PAD_R, LAYOUT_H / 2 - LAYOUT_ICON_LG / 2,        LAYOUT_ICON_LG, LAYOUT_ICON_LG)
  #define LAYOUT_NEXT_ICON_BOUNDS   GRect(LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_W / 4, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B, LAYOUT_ICON_SM, LAYOUT_ICON_SM)
#else
  #define LAYOUT_PREV_ICON_POS   GPoint(LAYOUT_PREV_ICON_X, LAYOUT_PAD_T)
  #define LAYOUT_CUR_ICON_POS    GPoint(LAYOUT_W - LAYOUT_ICON_LG - LAYOUT_PAD_R, LAYOUT_H / 2 - LAYOUT_ICON_LG / 2)
  #define LAYOUT_NEXT_ICON_POS   GPoint(LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_PAD_R, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B)

  #define LAYOUT_PREV_ICON_BOUNDS   GRect(LAYOUT_PREV_ICON_X,                       LAYOUT_PAD_T,                              LAYOUT_ICON_SM, LAYOUT_ICON_SM)
  #define LAYOUT_CUR_ICON_BOUNDS    GRect(LAYOUT_W - LAYOUT_ICON_LG - LAYOUT_PAD_R, LAYOUT_H / 2 - LAYOUT_ICON_LG / 2,        LAYOUT_ICON_LG, LAYOUT_ICON_LG)
  #define LAYOUT_NEXT_ICON_BOUNDS   GRect(LAYOUT_W - LAYOUT_ICON_SM - LAYOUT_PAD_R, LAYOUT_H - LAYOUT_ICON_SM - LAYOUT_PAD_B, LAYOUT_ICON_SM, LAYOUT_ICON_SM)
#endif

// ── Splash screen ─────────────────────────────────────────────────────────────
#define LAYOUT_SPLASH_IMG_BOUNDS   GRect(0,            0,                 LAYOUT_W,      LAYOUT_H * 2 / 3)
#define LAYOUT_SPLASH_TEXT_BOUNDS  GRect(LAYOUT_PAD_L, LAYOUT_H * 2 / 3, LAYOUT_TEXT_W, LAYOUT_H / 3)
#define LAYOUT_SPLASH_IMG_CENTER   GPoint(LAYOUT_W / 2, (LAYOUT_H * 2 / 3) / 2)
