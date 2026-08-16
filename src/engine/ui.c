/* ==========================================================================
 *  AURA :: ui.c  --  widget layer
 * ========================================================================== */

#include "ui.h"
#include "icons.h"
#include "../../include/theme.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static Canvas *g_c;
static Input  *g_in;
static uint64_t g_focus;
static int     g_layer;
static int     g_blocking;
static int     g_cursorHand;
static int     g_mouseUsed;

/* ------------------------------------------------------- persistent state -- */

#define USLOTS 2048
typedef struct { uint64_t id; float v; int used; } UState;
static UState g_us[USLOTS];

float *ui_state(uint64_t id, float initial)
{
    uint32_t h = (uint32_t)(id ^ (id >> 32)) & (USLOTS - 1);
    for (int p = 0; p < 96; p++) {
        UState *s = &g_us[(h + p) & (USLOTS - 1)];
        if (s->used && s->id == id) return &s->v;
        if (!s->used) { s->used = 1; s->id = id; s->v = initial; return &s->v; }
    }
    UState *s = &g_us[h];
    s->id = id; s->v = initial;
    return &s->v;
}

/* ------------------------------------------------------------------ core -- */

void in_new_frame(Input *in)
{
    in->pmx = in->mx; in->pmy = in->my;
    in->pressed = in->released = in->rpressed = in->dbl = 0;
    in->wheel = 0.f;
    in->nchars = 0;
    memset(in->keyHit, 0, sizeof in->keyHit);
}

void ui_begin(Canvas *c, Input *in)
{
    g_c = c; g_in = in;
    g_layer = 0;
    g_cursorHand = 0;
    g_mouseUsed = 0;
}

void ui_end(void) { }

Canvas *ui_canvas(void) { return g_c; }
Input  *ui_input (void) { return g_in; }

void ui_set_blocking(int on) { g_blocking = on; }
void ui_layer_push(void)     { g_layer++; }
void ui_layer_pop (void)     { if (g_layer > 0) g_layer--; }
int  ui_layer_blocked(void)  { return g_blocking && g_layer == 0; }

void ui_capture_mouse(void)  { g_mouseUsed = 1; }
void ui_cursor(int hand)     { if (hand) g_cursorHand = 1; }
int  ui_cursor_hand(void)    { return g_cursorHand; }

void ui_focus(uint64_t id)   { g_focus = id; }
void ui_focus_clear(void)    { g_focus = 0; }
int  ui_field_focused(uint64_t id) { return g_focus == id; }

int ui_hit(float x, float y, float w, float h)
{
    if (ui_layer_blocked()) return 0;
    if (!g_in) return 0;
    float mx = g_in->mx, my = g_in->my;
    if (mx < x || mx >= x + w || my < y || my >= y + h) return 0;
    int *cl = g_c->clip[g_c->nclip];
    if (mx < cl[0] || mx >= cl[2] || my < cl[1] || my >= cl[3]) return 0;
    return 1;
}

int ui_clicked(float x, float y, float w, float h)
{
    if (g_mouseUsed) return 0;
    return ui_hit(x, y, w, h) && g_in->pressed;
}

float ui_hover_f(uint64_t id, int hovered)
{
    return anim_to(id ^ 0x9E3779B97F4A7C15ULL, hovered ? 1.f : 0.f, 16.f);
}

int ui_hover_energy(uint64_t id, int hovered)
{
    return (int)(ui_hover_f(id, hovered) * 255.f);
}

/* ============================================================== containers = */

void ui_shadow_card(float x,float y,float w,float h,float r,float lift)
{
    if (lift <= 0.f) return;
    cv_shadow(g_c, x, y + lift*0.42f, w, h, r, 8.f + lift*2.2f,
              RGBA(58, 22, 120, (int)(30 + lift*5)));
}

void ui_card(float x,float y,float w,float h, float radius)
{
    cv_shadow(g_c, x, y + 3.f, w, h, radius, 16.f, RGBA(58,22,120,26));
    cv_rrect(g_c, x, y, w, h, radius, C_SURF);
    cv_rrect_line(g_c, x, y, w, h, radius, C_LINE, 1.f);
}

void ui_card_soft(float x,float y,float w,float h, float radius)
{
    cv_rrect(g_c, x, y, w, h, radius, C_SURF_2);
    cv_rrect_line(g_c, x, y, w, h, radius, C_LINE, 1.f);
}

void ui_panel_dark(float x,float y,float w,float h, float radius)
{
    Paint p = paint_linear(x, y, x, y+h, C_NIGHT_2, C_NIGHT);
    cv_shadow(g_c, x, y + 4.f, w, h, radius, 20.f, RGBA(20,6,50,70));
    cv_rrect_p(g_c, x, y, w, h, radius, &p);
    cv_rrect_line(g_c, x, y, w, h, radius, col_alpha(C_V400, .22f), 1.f);
}

void ui_divider(float x,float y,float w)
{
    cv_rect(g_c, x, y, w, 1.f, C_LINE);
}

void ui_section(float x,float y, const char *eyebrow, const char *title)
{
    tx_backdrop(C_BG);
    if (eyebrow && *eyebrow)
        tx_draw(g_c, eyebrow, x, y, font_track(TF_UI, 11, TW_BOLD, 2),
                C_V500, AL_L, AV_T);
    tx_draw(g_c, title, x, y + (eyebrow && *eyebrow ? 16.f : 0.f),
            font_make(TF_DISPLAY, 21, TW_SEMI), C_INK, AL_L, AV_T);
}

/* ---------------------------------------------------------------- scroll -- */

typedef struct { float x,y,w,h,content,off; uint64_t id; } ScrollFrame;
static ScrollFrame g_scroll[8];
static int         g_nscroll;

float ui_scroll_begin(uint64_t id, float x,float y,float w,float h,
                      float contentH)
{
    float *off = ui_state(id, 0.f);
    float maxOff = contentH - h;
    if (maxOff < 0.f) maxOff = 0.f;

    if (ui_hit(x, y, w, h) && g_in->wheel != 0.f)
        *off -= g_in->wheel * 68.f;
    if (*off < 0.f) *off = 0.f;
    if (*off > maxOff) *off = maxOff;

    float smooth = anim_to(id ^ 0x5151ULL, *off, 20.f);

    if (g_nscroll < 8) {
        ScrollFrame *f = &g_scroll[g_nscroll++];
        f->x=x; f->y=y; f->w=w; f->h=h; f->content=contentH;
        f->off=smooth; f->id=id;
    }
    cv_clip_push(g_c, x, y, w, h);
    return smooth;
}

void ui_scroll_end(void)
{
    cv_clip_pop(g_c);
    if (!g_nscroll) return;
    ScrollFrame *f = &g_scroll[--g_nscroll];
    if (f->content <= f->h + 1.f) return;

    float trackH = f->h - 8.f;
    float thumbH = trackH * (f->h / f->content);
    if (thumbH < 30.f) thumbH = 30.f;
    float maxOff = f->content - f->h;
    float t = maxOff > 0.f ? f->off / maxOff : 0.f;
    float tx = f->x + f->w - 7.f;
    float ty = f->y + 4.f + (trackH - thumbH) * t;

    int hov = ui_hit(f->x + f->w - 16.f, f->y, 16.f, f->h);
    float e = ui_hover_f(f->id ^ 0x77ULL, hov);
    cv_rrect(g_c, tx, f->y + 4.f, 4.f, trackH, 2.f, col_alpha(C_V300, .28f));
    cv_rrect(g_c, tx - e*1.f, ty, 4.f + e*2.f, thumbH, 3.f,
             col_mix(C_V300, C_V600, e));
}

/* ============================================================== controls == */

typedef struct { Color bg, bgHover, fg, border; } BtnStyle;

static BtnStyle btn_style(int variant, float e, int down)
{
    BtnStyle s;
    switch (variant) {
    case BTN_PRIMARY:
        s.bg = col_mix(C_V600, C_V500, e); s.fg = HEX(0xFFFFFF);
        s.border = RGBA(0,0,0,0);
        break;
    case BTN_SOFT:
        s.bg = col_mix(C_V100, C_V200, e); s.fg = C_V700;
        s.border = RGBA(0,0,0,0);
        break;
    case BTN_GHOST:
        s.bg = col_alpha(C_V500, 0.10f * e); s.fg = C_INK_2;
        s.border = RGBA(0,0,0,0);
        break;
    case BTN_OUTLINE:
        s.bg = col_mix(C_SURF, C_V50, e); s.fg = C_INK;
        s.border = col_mix(C_LINE_2, C_V400, e);
        break;
    case BTN_DANGER:
        s.bg = col_mix(C_DANGER, col_lighten(C_DANGER,.14f), e);
        s.fg = HEX(0xFFFFFF); s.border = RGBA(0,0,0,0);
        break;
    case BTN_SUCCESS:
        s.bg = col_mix(C_OK, col_lighten(C_OK,.14f), e);
        s.fg = HEX(0xFFFFFF); s.border = RGBA(0,0,0,0);
        break;
    case BTN_DARK:
    default:
        s.bg = col_mix(C_V900, C_V800, e); s.fg = HEX(0xFFFFFF);
        s.border = RGBA(0,0,0,0);
        break;
    }
    if (down) s.bg = col_darken(s.bg, 0.12f);
    s.bgHover = s.bg;
    return s;
}

int ui_button_i(uint64_t id, float x,float y,float w,float h,
                const char *label, int icon, int variant)
{
    int hov  = ui_hit(x, y, w, h) && !g_mouseUsed;
    int down = hov && g_in->down;
    float e  = ui_hover_f(id, hov);
    float pr = anim_to(id ^ 0xABCULL, down ? 1.f : 0.f, 26.f);
    if (hov) ui_cursor(1);

    float sy = y + pr * 1.4f;
    float sh = h - pr * 1.4f;
    BtnStyle s = btn_style(variant, e, down);

    if (variant == BTN_PRIMARY || variant == BTN_DARK ||
        variant == BTN_DANGER  || variant == BTN_SUCCESS) {
        cv_shadow(g_c, x, sy + 3.f - pr*2.f, w, sh, h*0.5f > 12.f ? 12.f : h*0.5f,
                  10.f + e*6.f, col_alpha(s.bg, 0.34f + e*0.12f));
    }
    float r = h * 0.5f > 14.f ? 14.f : h * 0.42f;
    cv_rrect(g_c, x, sy, w, sh, r, s.bg);
    if (COL_A(s.border) > 0)
        cv_rrect_line(g_c, x, sy, w, sh, r, s.border, 1.2f);

    /* glossy top edge on filled buttons */
    if (variant == BTN_PRIMARY || variant == BTN_DARK) {
        Paint gl = paint_linear(x, sy, x, sy + sh*0.55f,
                                col_alpha(HEX(0xFFFFFF), .16f),
                                col_alpha(HEX(0xFFFFFF), .0f));
        cv_rrect_p(g_c, x+1, sy+1, w-2, sh*0.55f, r-1, &gl);
    }

    Font f = font_make(TF_UI, h >= 40.f ? 14 : 13, TW_SEMI);
    tx_backdrop(s.bg);
    float tw = label && *label ? (float)tx_width(g_c, label, f) : 0.f;
    float isz = h * 0.44f;
    float gap = (label && *label && icon) ? 8.f : 0.f;
    float total = tw + gap + (icon ? isz : 0.f);
    float cx = x + (w - total) * 0.5f;

    if (icon) {
        icon_draw(g_c, icon, cx + isz*0.5f, sy + sh*0.5f, isz, s.fg);
        cx += isz + gap;
    }
    if (label && *label)
        tx_draw(g_c, label, cx, sy + sh*0.5f, f, s.fg, AL_L, AV_M);

    int clicked = hov && g_in->pressed;
    if (clicked) g_mouseUsed = 1;
    return clicked;
}

int ui_button(uint64_t id, float x,float y,float w,float h,
              const char *label, int variant)
{
    return ui_button_i(id, x, y, w, h, label, 0, variant);
}

int ui_icon_btn(uint64_t id, float x,float y,float sz, int icon, int variant)
{
    int hov  = ui_hit(x, y, sz, sz) && !g_mouseUsed;
    int down = hov && g_in->down;
    float e  = ui_hover_f(id, hov);
    if (hov) ui_cursor(1);
    BtnStyle s = btn_style(variant, e, down);
    float scale = 1.f - (down ? 0.06f : 0.f);

    if (variant != BTN_GHOST || e > 0.01f)
        cv_rrect(g_c, x, y, sz, sz, sz*0.32f, s.bg);
    icon_draw(g_c, icon, x + sz*0.5f, y + sz*0.5f, sz*0.52f*scale, s.fg);

    int clicked = hov && g_in->pressed;
    if (clicked) g_mouseUsed = 1;
    return clicked;
}

int ui_chip(uint64_t id, float x,float y,float h, const char *label, int active)
{
    Font f = font_make(TF_UI, 12, TW_SEMI);
    float pad = 13.f;
    float w = tx_width(g_c, label, f) + pad*2.f;
    int hov = ui_hit(x, y, w, h) && !g_mouseUsed;
    float e = ui_hover_f(id, hov);
    float a = anim_to(id ^ 0x31ULL, active ? 1.f : 0.f, 16.f);
    if (hov) ui_cursor(1);

    Color bg = col_mix(col_mix(C_SURF, C_V50, e), C_V600, a);
    Color fg = col_mix(col_mix(C_INK_2, C_V700, e), HEX(0xFFFFFF), a);
    cv_rrect(g_c, x, y, w, h, h*0.5f, bg);
    if (a < 0.5f)
        cv_rrect_line(g_c, x, y, w, h, h*0.5f, col_mix(C_LINE_2, C_V300, e), 1.f);
    tx_backdrop(bg);
    tx_draw(g_c, label, x + w*0.5f, y + h*0.5f, f, fg, AL_C, AV_M);

    int clicked = hov && g_in->pressed;
    if (clicked) g_mouseUsed = 1;
    return clicked;
}

int ui_tab(uint64_t id, float x,float y,float w,float h,
           const char *label, int icon, int active)
{
    int hov = ui_hit(x, y, w, h) && !g_mouseUsed;
    float e = ui_hover_f(id, hov);
    float a = anim_to(id ^ 0x41ULL, active ? 1.f : 0.f, 15.f);
    if (hov) ui_cursor(1);

    Color fg = col_mix(col_mix(C_INK_3, C_INK, e), C_V700, a);
    if (e > 0.01f && a < 0.9f)
        cv_rrect(g_c, x, y + 3.f, w, h - 6.f, 9.f, col_alpha(C_V500, .07f*e));

    Font f = font_make(TF_UI, 13, a > .5f ? TW_SEMI : TW_MED);
    float tw = (float)tx_width(g_c, label, f);
    float isz = 17.f;
    float total = tw + (icon ? isz + 7.f : 0.f);
    float cx = x + (w - total)*0.5f;
    tx_backdrop(C_SURF);
    if (icon) { icon_draw(g_c, icon, cx + isz*0.5f, y + h*0.5f, isz, fg); cx += isz + 7.f; }
    tx_draw(g_c, label, cx, y + h*0.5f, f, fg, AL_L, AV_M);

    if (a > 0.01f) {
        float uw = (total + 22.f) * a;
        cv_rrect(g_c, x + (w-uw)*0.5f, y + h - 3.f, uw, 3.f, 1.5f, C_V600);
    }
    int clicked = hov && g_in->pressed;
    if (clicked) g_mouseUsed = 1;
    return clicked;
}

int ui_toggle(uint64_t id, float x,float y, int *value)
{
    const float w = 44.f, h = 24.f;
    int hov = ui_hit(x, y, w, h) && !g_mouseUsed;
    if (hov) ui_cursor(1);
    int changed = 0;
    if (hov && g_in->pressed) { *value = !*value; changed = 1; g_mouseUsed = 1; }

    float a = anim_spring(id, *value ? 1.f : 0.f, 320.f, 26.f);
    float e = ui_hover_f(id ^ 9ULL, hov);
    Color track = col_mix(col_mix(C_SURF_3, C_LINE_2, e), C_V600, a);
    cv_rrect(g_c, x, y, w, h, h*0.5f, track);
    float kx = x + 3.f + a * (w - h + 0.f - 6.f + 0.f);
    cv_shadow(g_c, kx, y + 4.f, h - 6.f, h - 6.f, (h-6.f)*0.5f, 6.f,
              RGBA(40,10,90,80));
    cv_circle(g_c, kx + (h-6.f)*0.5f, y + h*0.5f, (h-6.f)*0.5f, HEX(0xFFFFFF));
    return changed;
}

float ui_slider(uint64_t id, float x,float y,float w, float v, float lo, float hi)
{
    const float h = 22.f;
    int hov = ui_hit(x, y - 6.f, w, h + 12.f);
    float *drag = ui_state(id ^ 0x5AULL, 0.f);
    if (hov) ui_cursor(1);
    if (hov && g_in->pressed) { *drag = 1.f; g_mouseUsed = 1; }
    if (!g_in->down) *drag = 0.f;
    if (*drag > 0.5f) {
        float t = (g_in->mx - x) / (w > 0.f ? w : 1.f);
        v = lo + cv_clampf(t, 0.f, 1.f) * (hi - lo);
    }
    float t = (hi > lo) ? (v - lo)/(hi - lo) : 0.f;
    t = cv_clampf(t, 0.f, 1.f);
    float e = ui_hover_f(id, hov || *drag > 0.5f);

    cv_rrect(g_c, x, y + h*0.5f - 3.f, w, 6.f, 3.f, C_SURF_3);
    Paint fp = paint_linear(x, y, x + w, y, C_V500, C_V700);
    if (t > 0.f) cv_rrect_p(g_c, x, y + h*0.5f - 3.f, w*t, 6.f, 3.f, &fp);
    float kx = x + w*t;
    cv_shadow(g_c, kx - 9.f, y + h*0.5f - 6.f, 18.f, 18.f, 9.f, 8.f,
              RGBA(60,20,130,90));
    cv_circle(g_c, kx, y + h*0.5f, 9.f + e*1.6f, HEX(0xFFFFFF));
    cv_circle(g_c, kx, y + h*0.5f, 4.6f + e*0.8f, C_V600);
    return v;
}

int ui_stepper(uint64_t id, float x,float y,float w,float h, int *v, int lo, int hi)
{
    int changed = 0;
    if (ui_icon_btn(id ^ 1ULL, x, y, h, IC_MINUS, BTN_SOFT)) {
        if (*v > lo) { (*v)--; changed = 1; }
    }
    if (ui_icon_btn(id ^ 2ULL, x + w - h, y, h, IC_PLUS, BTN_SOFT)) {
        if (*v < hi) { (*v)++; changed = 1; }
    }
    char buf[32]; snprintf(buf, sizeof buf, "%d", *v);
    tx_backdrop(C_SURF);
    tx_draw(g_c, buf, x + w*0.5f, y + h*0.5f, font_make(TF_DISPLAY, 17, TW_SEMI),
            C_INK, AL_C, AV_M);
    return changed;
}

/* ------------------------------------------------------------ text field -- */

int ui_text_field(uint64_t id, float x,float y,float w,float h,
                  char *buf, int cap, const char *placeholder, int icon)
{
    int hov = ui_hit(x, y, w, h);
    int focused = (g_focus == id);
    if (hov) ui_cursor(1);
    if (hov && g_in->pressed) { g_focus = id; focused = 1; g_mouseUsed = 1; }
    else if (!hov && g_in->pressed && focused && !ui_layer_blocked()) {
        g_focus = 0; focused = 0;
    }

    float fe = anim_to(id ^ 0x11ULL, focused ? 1.f : 0.f, 18.f);
    float he = ui_hover_f(id, hov);

    Color bg = col_mix(C_SURF_2, C_SURF, fe);
    cv_rrect(g_c, x, y, w, h, 11.f, bg);
    if (fe > 0.02f)
        cv_rrect_line(g_c, x - fe*2.f, y - fe*2.f, w + fe*4.f, h + fe*4.f,
                      11.f + fe*2.f, col_alpha(C_V400, .30f*fe), 2.f);
    cv_rrect_line(g_c, x, y, w, h, 11.f,
                  col_mix(col_mix(C_LINE_2, C_V300, he), C_V600, fe), 1.4f);

    float tx0 = x + 14.f;
    if (icon) {
        icon_draw(g_c, icon, x + 18.f, y + h*0.5f, 17.f,
                  col_mix(C_INK_3, C_V600, fe));
        tx0 = x + 36.f;
    }

    int changed = 0;
    int len = (int)strlen(buf);
    float *caretf = ui_state(id ^ 0x22ULL, 0.f);
    int caret = (int)*caretf;
    if (caret > len) caret = len;

    if (focused) {
        for (int i = 0; i < g_in->nchars; i++) {
            unsigned short ch = g_in->chars[i];
            if (ch == 8) {                              /* backspace        */
                if (caret > 0) {
                    memmove(buf + caret - 1, buf + caret, (size_t)(len - caret + 1));
                    caret--; len--; changed = 1;
                }
            } else if (ch >= 32 && ch < 127) {
                if (len < cap - 1) {
                    memmove(buf + caret + 1, buf + caret, (size_t)(len - caret + 1));
                    buf[caret] = (char)ch;
                    caret++; len++; changed = 1;
                }
            }
        }
        if (g_in->keyHit[VK_LEFT]  && caret > 0)   caret--;
        if (g_in->keyHit[VK_RIGHT] && caret < len) caret++;
        if (g_in->keyHit[VK_HOME]) caret = 0;
        if (g_in->keyHit[VK_END])  caret = len;
        if (g_in->keyHit[VK_DELETE] && caret < len) {
            memmove(buf + caret, buf + caret + 1, (size_t)(len - caret));
            len--; changed = 1;
        }
    }
    *caretf = (float)caret;

    Font f = font_make(TF_UI, 14, TW_MED);
    tx_backdrop(bg);
    cv_clip_push(g_c, tx0, y, w - (tx0 - x) - 14.f, h);
    if (len > 0) {
        tx_draw(g_c, buf, tx0, y + h*0.5f, f, C_INK, AL_L, AV_M);
    } else if (placeholder) {
        tx_draw(g_c, placeholder, tx0, y + h*0.5f, f, C_INK_4, AL_L, AV_M);
    }
    if (focused) {
        char pre[512];
        int n = caret < (int)sizeof pre - 1 ? caret : (int)sizeof pre - 1;
        memcpy(pre, buf, (size_t)n); pre[n] = 0;
        float cx = tx0 + (float)tx_width(g_c, pre, f);
        float blink = 0.5f + 0.5f*sinf(anim_time()*7.f);
        if (blink > 0.35f)
            cv_rect(g_c, cx, y + h*0.5f - 9.f, 1.8f, 18.f, C_V600);
    }
    cv_clip_pop(g_c);
    return changed;
}

/* ================================================================ chrome == */

void ui_badge(float x,float y, const char *text, Color fg, Color bg)
{
    Font f = font_track(TF_UI, 10, TW_BOLD, 1);
    float w = tx_width(g_c, text, f) + 16.f;
    cv_rrect(g_c, x, y, w, 19.f, 5.f, bg);
    tx_backdrop(bg);
    tx_draw(g_c, text, x + w*0.5f, y + 9.5f, f, fg, AL_C, AV_M);
}

void ui_pill(float x,float y,float h, const char *text, Color fg, Color bg)
{
    Font f = font_make(TF_UI, 12, TW_SEMI);
    float w = tx_width(g_c, text, f) + h;
    cv_rrect(g_c, x, y, w, h, h*0.5f, bg);
    tx_backdrop(bg);
    tx_draw(g_c, text, x + w*0.5f, y + h*0.5f, f, fg, AL_C, AV_M);
}

void ui_progress(float x,float y,float w,float h, float t, Color col)
{
    cv_rrect(g_c, x, y, w, h, h*0.5f, C_SURF_3);
    t = cv_clampf(t, 0.f, 1.f);
    if (t > 0.001f) cv_rrect(g_c, x, y, w*t, h, h*0.5f, col);
}

void ui_meter(float x,float y,float w,float h, float t, Color a, Color b)
{
    cv_rrect(g_c, x, y, w, h, h*0.5f, C_SURF_3);
    t = cv_clampf(t, 0.f, 1.f);
    if (t > 0.001f) {
        Paint p = paint_linear(x, y, x + w, y, a, b);
        cv_rrect_p(g_c, x, y, w*t, h, h*0.5f, &p);
    }
}

void ui_spinner(float cx,float cy,float r, Color col)
{
    float t = anim_time() * 2.6f;
    for (int i = 0; i < 8; i++) {
        float a = t + (float)i * 0.7853981f;
        float f = 1.f - (float)i / 8.f;
        cv_circle(g_c, cx + cosf(a)*r, cy + sinf(a)*r, r*0.20f,
                  col_alpha(col, f));
    }
}

void ui_skeleton(float x,float y,float w,float h,float r)
{
    float t = anim_time()*1.5f;
    float shimmer = 0.5f + 0.5f*sinf(t);
    cv_rrect(g_c, x, y, w, h, r, col_mix(C_SURF_2, C_SURF_3, shimmer));
}

void ui_tooltip(float x,float y, const char *text)
{
    Font f = font_make(TF_UI, 12, TW_MED);
    float w = tx_width(g_c, text, f) + 22.f, h = 30.f;
    float tx0 = x - w*0.5f, ty0 = y - h - 10.f;
    if (tx0 < 6.f) tx0 = 6.f;
    if (tx0 + w > g_c->w - 6.f) tx0 = g_c->w - 6.f - w;
    cv_shadow(g_c, tx0, ty0 + 3.f, w, h, 9.f, 14.f, RGBA(30,8,70,90));
    cv_rrect(g_c, tx0, ty0, w, h, 9.f, C_V900);
    cv_tri(g_c, x - 5.f, ty0 + h, x + 5.f, ty0 + h, x, ty0 + h + 6.f, C_V900);
    tx_backdrop(C_V900);
    tx_draw(g_c, text, tx0 + w*0.5f, ty0 + h*0.5f, f, HEX(0xFFFFFF), AL_C, AV_M);
}

/* ---------------------------------------------------------------- toasts -- */

#define TOAST_MAX 5
typedef struct {
    int   used, kind;
    char  title[64], body[160];
    float age, life;
} Toast;
static Toast g_toast[TOAST_MAX];

void ui_toast(int kind, const char *title, const char *body)
{
    int slot = -1;
    for (int i = 0; i < TOAST_MAX; i++) if (!g_toast[i].used) { slot = i; break; }
    if (slot < 0) {                        /* recycle the oldest             */
        float best = -1.f;
        for (int i = 0; i < TOAST_MAX; i++)
            if (g_toast[i].age > best) { best = g_toast[i].age; slot = i; }
    }
    Toast *t = &g_toast[slot];
    memset(t, 0, sizeof *t);
    t->used = 1; t->kind = kind; t->age = 0.f; t->life = 4.6f;
    snprintf(t->title, sizeof t->title, "%s", title ? title : "");
    snprintf(t->body,  sizeof t->body,  "%s", body  ? body  : "");
}

void ui_toasts_draw(float screenW, float screenH)
{
    float dt = anim_dt();
    float y = screenH - 24.f;
    for (int i = TOAST_MAX - 1; i >= 0; i--) {
        Toast *t = &g_toast[i];
        if (!t->used) continue;
        t->age += dt;
        if (t->age > t->life) { t->used = 0; continue; }

        float in  = ease_out_back(cv_clampf(t->age / 0.42f, 0.f, 1.f));
        float out = 1.f - cv_clampf((t->age - (t->life - 0.4f)) / 0.4f, 0.f, 1.f);
        float w = 330.f, h = t->body[0] ? 74.f : 52.f;
        float x = screenW - w - 24.f + (1.f - in) * 60.f;
        y -= h + 12.f;
        float ty = y + (1.f - out) * 16.f;

        Color accent = C_V600;
        int ic = IC_INFO;
        if (t->kind == TOAST_OK)   { accent = C_OK;     ic = IC_CHECK; }
        if (t->kind == TOAST_WARN) { accent = C_WARN;   ic = IC_ALERT; }
        if (t->kind == TOAST_ERR)  { accent = C_DANGER; ic = IC_CLOSE; }

        float a = in * out;
        cv_shadow(g_c, x, ty + 6.f, w, h, 14.f, 22.f,
                  RGBA(40,12,95,(int)(70*a)));
        cv_rrect(g_c, x, ty, w, h, 14.f, C_SURF);
        cv_rrect_line(g_c, x, ty, w, h, 14.f, C_LINE, 1.f);
        cv_rrect(g_c, x, ty + 10.f, 4.f, h - 20.f, 2.f, accent);

        cv_circle(g_c, x + 32.f, ty + 26.f, 13.f, col_alpha(accent, .14f));
        icon_draw(g_c, ic, x + 32.f, ty + 26.f, 15.f, accent);

        tx_backdrop(C_SURF);
        tx_draw_a(g_c, t->title, x + 54.f, ty + 17.f,
                  font_make(TF_UI, 13, TW_SEMI), C_INK, AL_L, AV_T, a);
        if (t->body[0])
            tx_para(g_c, t->body, x + 54.f, ty + 36.f, w - 70.f,
                    font_make(TF_UI, 12, TW_REG), C_INK_3, 1);

        /* life bar */
        float lt = 1.f - t->age / t->life;
        cv_rrect(g_c, x + 14.f, ty + h - 5.f, (w - 28.f)*lt, 2.5f, 1.25f,
                 col_alpha(accent, .45f));
    }
}
