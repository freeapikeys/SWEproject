/* ==========================================================================
 *  AURA :: ui.h  --  immediate-mode widget layer
 *
 *  Screens describe what should be on screen this frame and get interaction
 *  back as return values.  Anything that needs to persist between frames --
 *  scroll offsets, caret positions, hover energy -- is kept here in a table
 *  keyed by a hashed identifier, so screen code stays declarative.
 * ========================================================================== */
#ifndef AURA_UI_H
#define AURA_UI_H

#include "canvas.h"
#include "text.h"
#include "anim.h"

/* ---------------------------------------------------------------- input -- */

typedef struct {
    float mx, my, pmx, pmy;
    int   down, pressed, released;
    int   rdown, rpressed;
    int   dbl;
    float wheel;
    int   keyHit[256];
    int   keyDown[256];
    int   shift, ctrl, alt;
    unsigned short chars[32];
    int   nchars;
} Input;

void  in_new_frame(Input *in);          /* clears per-frame edge state       */

/* ----------------------------------------------------------------- core -- */

void  ui_begin(Canvas *c, Input *in);
void  ui_end  (void);
Canvas *ui_canvas(void);
Input  *ui_input (void);

float *ui_state(uint64_t id, float initial);   /* persistent per-id storage  */
int    ui_state_load(void);                    /* slots in use, for testing  */

int   ui_hit    (float x, float y, float w, float h);   /* hover + enabled  */
int   ui_clicked(float x, float y, float w, float h);
void  ui_cursor (int hand);
int   ui_cursor_hand(void);

/* Modal gating.  The application raises the blocking flag at the top of a
 * frame when an overlay is open; everything drawn on the base layer then goes
 * inert, and the overlay re-enables itself with ui_layer_push(). */
void  ui_set_blocking(int on);
void  ui_layer_push(void);
void  ui_layer_pop (void);
int   ui_layer_blocked(void);
void  ui_capture_mouse(void);           /* consume the click for this frame  */

/* ------------------------------------------------------------- controls -- */

enum { BTN_PRIMARY = 0, BTN_SOFT, BTN_GHOST, BTN_OUTLINE, BTN_DANGER,
       BTN_DARK, BTN_SUCCESS };

int   ui_button   (uint64_t id, float x,float y,float w,float h,
                   const char *label, int variant);
int   ui_button_i (uint64_t id, float x,float y,float w,float h,
                   const char *label, int icon, int variant);
int   ui_icon_btn (uint64_t id, float x,float y,float sz, int icon,
                   int variant);
int   ui_chip     (uint64_t id, float x,float y,float h, const char *label,
                   int active);
int   ui_tab      (uint64_t id, float x,float y,float w,float h,
                   const char *label, int icon, int active);
int   ui_toggle   (uint64_t id, float x,float y, int *value);
float ui_slider   (uint64_t id, float x,float y,float w, float v,
                   float lo, float hi);
int   ui_stepper  (uint64_t id, float x,float y,float w,float h, int *v,
                   int lo, int hi);
int   ui_text_field(uint64_t id, float x,float y,float w,float h,
                    char *buf, int cap, const char *placeholder, int icon);
/* same field, drawn as discs -- for passwords */
int   ui_secret_field(uint64_t id, float x,float y,float w,float h,
                    char *buf, int cap, const char *placeholder, int icon);
int   ui_field_focused(uint64_t id);
void  ui_focus    (uint64_t id);
void  ui_focus_clear(void);

/* ------------------------------------------------------------ containers -- */

void  ui_card     (float x,float y,float w,float h, float radius);
void  ui_card_soft(float x,float y,float w,float h, float radius);
void  ui_panel_dark(float x,float y,float w,float h, float radius);
void  ui_section  (float x,float y, const char *eyebrow, const char *title);
void  ui_divider  (float x,float y,float w);

/* Scroll region.  Returns the current (smoothed) offset; the caller draws
 * content translated by -offset and closes with ui_scroll_end(). */
float ui_scroll_begin(uint64_t id, float x,float y,float w,float h,
                      float contentH);
void  ui_scroll_end  (void);

/* ---------------------------------------------------------------- chrome -- */

void  ui_badge    (float x,float y, const char *text, Color fg, Color bg);
void  ui_pill     (float x,float y,float h, const char *text, Color fg, Color bg);
void  ui_progress (float x,float y,float w,float h, float t, Color col);
void  ui_meter    (float x,float y,float w,float h, float t, Color a, Color b);
void  ui_spinner  (float cx,float cy,float r, Color col);
void  ui_skeleton (float x,float y,float w,float h,float r);
void  ui_tooltip  (float x,float y, const char *text);

/* toasts ------------------------------------------------------------------ */
enum { TOAST_INFO = 0, TOAST_OK, TOAST_WARN, TOAST_ERR };
void  ui_toast(int kind, const char *title, const char *body);
void  ui_toasts_draw(float screenW, float screenH);

/* ------------------------------------------------------------------ misc -- */

void  ui_shadow_card(float x,float y,float w,float h,float r,float lift);
int   ui_hover_energy(uint64_t id, int hovered);   /* 0..1 as float via ret */
float ui_hover_f(uint64_t id, int hovered);

#endif /* AURA_UI_H */
