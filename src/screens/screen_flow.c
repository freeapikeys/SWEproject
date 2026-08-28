/* ==========================================================================
 *  AURA :: screen_flow.c   --   terminal flow and central search
 *
 *  Passengers are drawn as individual figures walking the queue lanes.  The
 *  number in the hall comes from the flow model, and the rate they clear the
 *  archways comes from the M/M/c service model -- so closing a lane visibly
 *  backs the hall up, which is the point of the screen.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LANES_MAX 12

static void figure2(Canvas *c, float x, float y, float s, Color col, float ph)
{
    float b = sinf(ph) * s * 0.09f;
    cv_circle(c, x, y - s*0.66f + b, s*0.26f, col);
    Path p; path_reset(&p);
    path_move(&p, x - s*0.32f, y + s*0.42f + b);
    path_cubic(&p, x - s*0.32f, y - s*0.22f + b,
                   x + s*0.32f, y - s*0.22f + b,
                   x + s*0.32f, y + s*0.42f + b);
    path_close(&p);
    cv_fill_col(c, &p, col);
}

static void draw_search_hall(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    FlowModel *m = &a->flow;

    ui_panel_dark(c ? x : x, y, w, h, R_LG);
    cv_clip_push(c, x + 1.f, y + 1.f, w - 2.f, h - 2.f);

    tx_backdrop(C_NIGHT_2);
    tx_draw(c, "CENTRAL SEARCH", x + 18.f, y + 14.f,
            font_track(TF_UI, 10, TW_BOLD, 2), col_alpha(HEX(0xFFFFFF), .9f),
            AL_L, AV_T);
    /*  Throughput -- how many passengers clear per minute -- is the number
     *  a duty manager actually watches, so it goes next to the wait.  Each
     *  lane processes one passenger about every twelve seconds.            */
    float perMin = (float)m->lanesOpen * (60.f / 12.f);
    char sub[120];
    snprintf(sub, sizeof sub,
             "%d lanes open   -   %.0f min wait   -   ~%.0f clearing per minute"
             "   -   %.0f%% busy",
             m->lanesOpen, m->waitNow, perMin, m->utilisation*100.f);
    tx_draw(c, sub, x + 18.f, y + 32.f, font_make(TF_UI, 11, TW_MED),
            col_alpha(C_V200, .72f), AL_L, AV_T);

    /* a small legend so the hall reads without a manual */
    struct { const char *t; Color col; } FL[3] = {
        { "open lane", C_TEAL }, { "waiting", C_V300 }, { "airside", C_OK } };
    float lgx = x + w - 16.f;
    for (int i = 2; i >= 0; i--) {
        Font lf = font_make(TF_UI, 10, TW_SEMI);
        float tw2 = (float)tx_width(c, FL[i].t, lf);
        lgx -= tw2;
        tx_draw(c, FL[i].t, lgx, y + 16.f, lf, col_alpha(HEX(0xFFFFFF), .7f),
                AL_L, AV_T);
        lgx -= 10.f;
        cv_circle(c, lgx, y + 21.f, 4.f, FL[i].col);
        lgx -= 14.f;
    }

    int lanes = m->lanesOpen;
    if (lanes < 1) lanes = 1;
    if (lanes > LANES_MAX) lanes = LANES_MAX;

    float top = y + 56.f;
    float bottom = y + h - 18.f;
    float laneW = (w - 40.f) / (float)lanes;
    float archY = top + 46.f;

    /* the queue length per lane comes from the model, not from a script */
    int totalQ = (int)(m->history[m->nHist ? m->nHist-1 : 0] * 1.9f);
    if (totalQ < 0) totalQ = 0;

    for (int L = 0; L < lanes; L++) {
        float lx = x + 20.f + L*laneW + laneW*0.5f;

        /* lane floor */
        cv_rrect(c, lx - laneW*0.40f, top, laneW*0.80f, bottom - top, 8.f,
                 col_alpha(HEX(0x000000), .18f));

        /* archway */
        cv_rrect(c, lx - 26.f, archY - 30.f, 52.f, 60.f, 8.f, HEX(0x33195F));
        cv_rrect(c, lx - 20.f, archY - 24.f, 40.f, 48.f, 6.f, C_NIGHT);
        cv_rrect_line(c, lx - 26.f, archY - 30.f, 52.f, 60.f, 8.f,
                      col_alpha(C_TEAL, .45f), 1.2f);
        float scan = fmodf(anim_time()*0.8f + L*0.2f, 1.f);
        cv_rect(c, lx - 19.f, archY - 24.f + 48.f*scan, 38.f, 2.f,
                col_alpha(C_TEAL, .75f));
        char ln[8]; snprintf(ln, sizeof ln, "%d", L + 1);
        tx_backdrop(C_NIGHT);
        tx_draw(c, ln, lx, archY, font_make(TF_UI, 13, TW_BOLD),
                col_alpha(C_V200, .8f), AL_C, AV_M);

        /* someone stepping through the archway */
        float step = fmodf(anim_time()*0.55f + L*0.37f, 1.f);
        figure2(c, lx, archY + 36.f - step*74.f, 17.f,
                col_alpha(C_GOLD, 0.35f + 0.65f*(1.f - fabsf(step-0.5f)*2.f)),
                anim_time()*6.f + L);

        /* the queue behind it */
        int q = totalQ / lanes + ((L < (totalQ % lanes)) ? 1 : 0);
        int show = q > 14 ? 14 : q;

        /* an OPEN marker and this lane's own count, so closing one reads */
        char lq[20]; snprintf(lq, sizeof lq, "OPEN  -  %d", q);
        tx_backdrop(C_NIGHT_2);
        tx_draw(c, lq, lx, archY + 44.f, font_make(TF_UI, 9, TW_SEMI),
                col_alpha(C_TEAL, .85f), AL_C, AV_M);
        for (int k = 0; k < show; k++) {
            float fy = archY + 62.f + k*20.f;
            if (fy > bottom - 8.f) break;
            float sway = sinf(anim_time()*1.6f + k*0.7f + L) * 3.f;
            figure2(c, lx + sway, fy, 16.f,
                    col_alpha(k % 4 == 0 ? C_V200 : C_V300, .85f - k*0.03f),
                    anim_time()*2.f + k*0.6f + L*0.4f);
        }
        if (q > 14) {
            char more[12]; snprintf(more, sizeof more, "+%d", q - 14);
            tx_backdrop(C_NIGHT_2);
            tx_draw(c, more, lx, bottom - 4.f, font_make(TF_UI, 10, TW_BOLD),
                    col_alpha(C_V200, .85f), AL_C, AV_B);
        }

        /* click a lane to close it */
        if (ui_hit(lx - laneW*0.40f, top, laneW*0.80f, bottom - top)) {
            ui_cursor(1);
            cv_rrect(c, lx - laneW*0.40f, top, laneW*0.80f, bottom - top, 8.f,
                     col_alpha(C_V400, .07f));
            if (a->in.pressed && m->lanesOpen > 1) {
                m->lanesOpen--;
                world_log(wo, LG_INFO, "Search lane closed, %d now open",
                          m->lanesOpen);
            }
        }
    }

    /* the sterile side */
    cv_line(c, x + 12.f, top + 4.f, x + w - 12.f, top + 4.f,
            col_alpha(C_V300, .22f), 1.4f);
    tx_backdrop(C_NIGHT_2);
    tx_draw(c, "AIRSIDE", x + w - 20.f, top + 12.f, font_track(TF_UI, 9, TW_BOLD, 2),
            col_alpha(C_V300, .6f), AL_R, AV_T);

    cv_clip_pop(c);
}

void screen_flow(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    FlowModel *m = &a->flow;
    float pad = PAD;

    /* ---- tiles ----------------------------------------------------------- */
    float tw = (w - pad*2.f - 36.f) / 4.f;
    char v[40], s[64];

    snprintf(v, sizeof v, "%.0f min", m->waitNow);
    snprintf(s, sizeof s, "target is under 10 minutes");
    draw_stat_tile(c, x + pad, y + pad, tw, 92.f, "SEARCH WAIT", v, s, IC_CLOCK,
                   m->waitNow > 14.f ? C_DANGER : C_OK);

    int q = sim_security_queue(wo);
    snprintf(v, sizeof v, "%d", q);
    snprintf(s, sizeof s, "%d in the check-in hall", sim_checkin_queue(wo));
    draw_stat_tile(c, x + pad + tw + 12.f, y + pad, tw, 92.f, "IN THE QUEUE", v, s,
                   IC_USERS, C_V600);

    snprintf(v, sizeof v, "%d / %d", m->lanesOpen, m->lanesNeeded);
    snprintf(s, sizeof s, "open versus needed at peak");
    draw_stat_tile(c, x + pad + (tw + 12.f)*2.f, y + pad, tw, 92.f, "LANES", v, s,
                   IC_SCAN, m->lanesNeeded > m->lanesOpen ? C_WARN : C_TEAL);

    int boarding = sim_count_state(wo, FS_BOARDING) + sim_count_state(wo, FS_FINAL);
    snprintf(v, sizeof v, "%d", boarding);
    snprintf(s, sizeof s, "flights at the gates now");
    draw_stat_tile(c, x + pad + (tw + 12.f)*3.f, y + pad, tw, 92.f, "BOARDING", v, s,
                   IC_GATE, C_MAGENTA);

    /* ---- hall ------------------------------------------------------------ */
    float hy = y + pad + 104.f;
    float rightW = 340.f;
    float hw = w - pad*2.f - rightW - 14.f;
    float hh = h - (hy - y) - pad;
    draw_search_hall(a, x + pad, hy, hw, hh);

    /* lane controls under the hall */
    float bx = x + pad + hw - 208.f, by = hy + hh - 46.f;
    if (ui_icon_btn(uid("laneminus"), bx, by, 32.f, IC_MINUS, BTN_DARK))
        if (m->lanesOpen > 1) m->lanesOpen--;
    if (ui_icon_btn(uid("laneplus"), bx + 38.f, by, 32.f, IC_PLUS, BTN_DARK))
        if (m->lanesOpen < LANES_MAX) m->lanesOpen++;
    if (m->lanesNeeded != m->lanesOpen &&
        ui_button(uid("laneauto"), bx + 78.f, by, 118.f, 32.f,
                  "Apply advice", BTN_PRIMARY)) {
        m->lanesOpen = m->lanesNeeded;
        ui_toast(TOAST_OK, "Lanes adjusted",
                 "Set to the level the queue model recommends.");
        world_log(wo, LG_AI, "Flow model advice applied: %d lanes open",
                  m->lanesOpen);
    }

    /* ---- right column ---------------------------------------------------- */
    float rx = x + pad + hw + 14.f;

    /* wait gauge */
    float gh2 = 210.f;
    ui_card(rx, hy, rightW, gh2, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "SERVICE LEVEL", rx + 18.f, hy + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float gcx = rx + rightW*0.5f, gcy = hy + 116.f, gr = 66.f;
    float t = cv_clampf(m->waitNow / 25.f, 0.f, 1.f);
    float shown = anim_to(uid("waitgauge"), t, 5.f);
    Path bgArc; path_reset(&bgArc);
    path_ring_arc(&bgArc, gcx, gcy, gr, gr - 14.f, 2.36f, 6.98f);
    cv_fill_col(c, &bgArc, C_SURF_3);
    Path fgArc; path_reset(&fgArc);
    path_ring_arc(&fgArc, gcx, gcy, gr, gr - 14.f, 2.36f, 2.36f + 4.62f*shown);
    Color gc = m->waitNow > 14.f ? C_DANGER : (m->waitNow > 9.f ? C_WARN : C_OK);
    Paint gp = paint_linear(gcx - gr, gcy, gcx + gr, gcy, col_lighten(gc,.25f), gc);
    cv_fill(c, &fgArc, &gp, 1.f);

    char wv[16]; snprintf(wv, sizeof wv, "%.0f", m->waitNow);
    tx_draw(c, wv, gcx, gcy - 6.f, font_track(TF_DISPLAY, 40, TW_BOLD, 0),
            C_INK, AL_C, AV_M);
    tx_draw(c, "minutes", gcx, gcy + 24.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_C, AV_M);
    tx_draw(c, m->waitNow > 14.f ? "Above target" : "Within target",
            gcx, hy + gh2 - 22.f, font_make(TF_UI, 12, TW_SEMI), gc, AL_C, AV_B);

    /* live journal */
    float jy = hy + gh2 + 14.f;
    float jh = hh - gh2 - 14.f;
    ui_card(rx, jy, rightW, jh, R_LG);
    tx_draw(c, "OPERATIONS JOURNAL", rx + 18.f, jy + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float ry = jy + 42.f, rh = jh - 52.f;
    int show = wo->nLog;
    float rowH = 46.f;
    float off = ui_scroll_begin(uid("logscroll"), rx + 6.f, ry, rightW - 12.f,
                                rh, show*rowH + 8.f);
    for (int i = 0; i < show; i++) {
        LogEntry *e = &wo->log[show - 1 - i];
        float ey = ry + i*rowH - off;
        if (ey + rowH < ry || ey > ry + rh) continue;

        Color kc = e->kind == LG_OK ? C_OK : (e->kind == LG_WARN ? C_WARN
                 : (e->kind == LG_ERR ? C_DANGER
                 : (e->kind == LG_AI ? C_MAGENTA : C_INFO)));
        cv_rrect(c, rx + 14.f, ey + 6.f, 3.f, rowH - 18.f, 1.5f, kc);
        char hhmm[8]; fmt_hhmm(e->minute, hhmm);
        tx_backdrop(C_SURF);
        tx_draw(c, hhmm, rx + 24.f, ey + 6.f, font_track(TF_MONO, 11, TW_BOLD, 0),
                C_INK_3, AL_L, AV_T);
        tx_para(c, e->text, rx + 68.f, ey + 4.f, rightW - 86.f,
                font_make(TF_UI, 11, TW_MED), C_INK_2, 1);
    }
    ui_scroll_end();
}
