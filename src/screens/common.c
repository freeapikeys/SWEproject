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
