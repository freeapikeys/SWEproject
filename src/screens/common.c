/* ==========================================================================
 *  AURA :: common.c  --  widgets shared between screens
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void draw_flight_row(App *a, float x, float y, float w, float h, Flight *f,
                     int selected, int showRisk)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    uint64_t id = uidi("frow", f->id);

    int hov = ui_hit(x, y, w, h);
    float e  = ui_hover_f(id, hov);
    float se = anim_to(id ^ 3ULL, selected ? 1.f : 0.f, 15.f);
    if (hov) ui_cursor(1);

    Color bg = col_mix(col_mix(C_SURF, C_V50, e), C_V100, se);
    cv_rrect(c, x, y, w, h, 10.f, bg);
    if (se > 0.01f)
        cv_rrect_line(c, x, y, w, h, 10.f, col_alpha(C_V500, se*0.8f), 1.4f);

    Color ac = wo->airline[f->airline].col;
    cv_rrect(c, x + 5.f, y + 8.f, 3.f, h - 16.f, 1.5f, ac);

    /* direction chevron */
    icon_draw(c, f->arrival ? IC_LANDING : IC_TAKEOFF,
              x + 24.f, y + h*0.5f, 16.f, f->arrival ? C_TEAL : C_V600);

    tx_backdrop(bg);
    tx_draw(c, f->no, x + 40.f, y + 8.f, font_track(TF_UI, 13, TW_BOLD, 0),
            C_INK, AL_L, AV_T);

    char route[48];
    snprintf(route, sizeof route, "%s %s", f->arrival ? "from" : "to",
             wo->airport[f->airport].city);
    tx_clipped(c, route, x + 40.f, y + h - 20.f, w*0.46f,
               font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);

    char est[8]; fmt_hhmm(f->estMin, est);
    tx_draw(c, est, x + w - 12.f, y + 8.f, font_track(TF_DISPLAY, 15, TW_BOLD, 0),
            f->delayMin >= 15 ? C_DANGER : C_INK, AL_R, AV_T);

    if (showRisk && !f->arrival) {
        Color rc = f->delayRisk > .6f ? C_DANGER
                 : (f->delayRisk > .3f ? C_WARN : C_OK);
        char pc[12]; snprintf(pc, sizeof pc, "%.0f%%", f->delayRisk*100.f);
        tx_draw(c, pc, x + w - 12.f, y + h - 20.f, font_make(TF_UI, 11, TW_SEMI),
                rc, AL_R, AV_T);
    } else {
        Color sc = fs_color(f->state);
        tx_clipped(c, fs_name(f->state), x + w - 12.f, y + h - 20.f, w*0.42f,
                   font_make(TF_UI, 11, TW_SEMI), sc, AL_R, AV_T);
    }

    if (hov && a->in.pressed) {
        a->selFlight = f->id;
        if (f->stand >= 0) a->selStand = f->stand;
    }
}

/* --------------------------------------------------------------------------
 *  A row of stars.
 *
 *  `value` is a rating, not a count, so 4.3 draws four filled stars and a
 *  partial fifth -- clipping the fill rather than rounding is what makes an
 *  average of 4.3 look different from one of 4.0.
 * ------------------------------------------------------------------------- */
void draw_stars(Canvas *c, float x, float y, float size, float value,
                int outOf, Color on, Color off)
{
    float pitch = size * 1.22f;
    for (int i = 0; i < outOf; i++) {
        float cx = x + size*0.5f + i*pitch;
        float fill = value - (float)i;
        if (fill < 0.f) fill = 0.f;
        if (fill > 1.f) fill = 1.f;

        icon_draw(c, IC_STAR, cx, y + size*0.5f, size, off);
        if (fill > 0.001f) {
            /* clip to the filled fraction so a half star reads as a half */
            cv_clip_push(c, cx - size*0.5f, y, size*fill + 0.5f, size);
            icon_draw(c, IC_STAR, cx, y + size*0.5f, size, on);
            cv_clip_pop(c);
        }
    }
}

/* --------------------------------------------------------------------------
 *  The AURA mark.
 *
 *  A rounded badge with a diagonal purple-to-magenta gradient, a paper plane
 *  climbing out of it, and a single orbiting dot on a short arc -- the plane
 *  for travel, the orbit for the tracking and radar the application is built
 *  around.  Drawn entirely with the rasteriser so it stays crisp at any size
 *  and needs no image file, which the no-dependencies rule would forbid.
 *
 *  `r` is the badge's corner radius; the whole mark spans about 2.4r.
 * ------------------------------------------------------------------------- */
void draw_logo(Canvas *c, float cx, float cy, float r)
{
    float s = r * 1.20f;                     /* badge half-extent            */

    /* the badge */
    Paint g = paint_linear(cx - s, cy - s, cx + s, cy + s, C_V500, C_MAGENTA);
    cv_rrect_p(c, cx - s, cy - s, s*2.f, s*2.f, r*0.55f, &g);
    cv_glow(c, cx - s*0.35f, cy - s*0.45f, s*1.05f, HEX(0xFFFFFF), 0.10f);
    cv_rrect_line(c, cx - s, cy - s, s*2.f, s*2.f, r*0.55f,
                  col_alpha(HEX(0xFFFFFF), .20f), 1.2f);

    /* the orbit: a tilted ellipse arc, with a dot travelling along it */
    float ph = anim_time() * 0.9f;
    float ea = r * 1.02f, eb = r * 0.44f;   /* semi-axes                    */
    float tilt = -0.62f;                     /* radians                      */
    float ct = cosf(tilt), st = sinf(tilt);
    float prevx = 0.f, prevy = 0.f;
    for (int i = 0; i <= 40; i++) {
        float a = (float)i / 40.f * 6.2831853f;
        float ox = cosf(a) * ea, oy = sinf(a) * eb;
        float px = cx + ox*ct - oy*st;
        float py = cy + ox*st + oy*ct;
        /* the far half of the orbit is drawn fainter, so it reads as 3-D */
        float depth = 0.5f + 0.5f*sinf(a);
        if (i > 0)
            cv_line(c, prevx, prevy, px, py,
                    col_alpha(HEX(0xFFFFFF), 0.10f + 0.16f*depth), 1.4f);
        prevx = px; prevy = py;
    }
    {
        float ox = cosf(ph) * ea, oy = sinf(ph) * eb;
        float dx = cx + ox*ct - oy*st;
        float dy = cy + ox*st + oy*ct;
        cv_circle(c, dx, dy, r*0.11f, HEX(0xFFFFFF));
        cv_glow(c, dx, dy, r*0.4f, HEX(0xFFFFFF), 0.5f);
    }

    /* the paper plane, climbing up-right */
    Path body; path_reset(&body);
    path_move (&body, cx - r*0.62f, cy + r*0.10f);   /* left wingtip        */
    path_line (&body, cx + r*0.66f, cy - r*0.60f);   /* nose, up-right      */
    path_line (&body, cx + r*0.02f, cy + r*0.66f);   /* tail               */
    path_line (&body, cx - r*0.10f, cy + r*0.12f);   /* inner notch        */
    path_close(&body);
    cv_fill_col(c, &body, HEX(0xFFFFFF));

    /* the underside fold, a touch darker so the two wings separate */
    Path fold; path_reset(&fold);
    path_move (&fold, cx - r*0.10f, cy + r*0.12f);
    path_line (&fold, cx + r*0.02f, cy + r*0.66f);
    path_line (&fold, cx + r*0.66f, cy - r*0.60f);
    path_close(&fold);
    cv_fill_col(c, &fold, col_alpha(C_V700, .32f));
}
