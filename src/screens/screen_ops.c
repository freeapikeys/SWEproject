/* ==========================================================================
 *  AURA :: screen_ops.c   --   the operations desk
 *
 *  The controller's screen, and the core of the application.  Everything a
 *  duty officer does to the day's programme happens here: retime a flight,
 *  change its status, move it to another gate, cancel it, or add one that was
 *  not in the schedule at all.
 *
 *  Two panels do work nothing else in the system does.  The gate board finds
 *  every pair of flights planned onto the same gate at overlapping times and
 *  names a gate that would clear the clash -- which is the failure that
 *  actually strands aeroplanes on a stand at a real airport.  The runway
 *  panel does the same for the single runway, where separation is the hard
 *  limit on how much traffic Plaisance can take.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ==========================================================================
 *  the flight list and its actions
 * ========================================================================== */

static void tab_flights(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    float listW = w*0.46f;
    if (listW > 460.f) listW = 460.f;

    /* ---- the list ------------------------------------------------------- */
    ui_card(x, y, listW, h, R_LG);
    ui_text_field(uid("opssearch"), x + 14.f, y + 14.f, listW - 28.f, 40.f,
                  a->opsSearch, sizeof a->opsSearch,
                  "Number, city, airline, tail or status", IC_SEARCH);

    int idx[MAX_FLIGHTS];
    int n = ops_flight_find(wo, a->opsSearch, idx, MAX_FLIGHTS);

    char cnt[70];
    snprintf(cnt, sizeof cnt, "%d of %d movements", n, wo->nFlights);
    tx_backdrop(C_SURF);
    tx_draw(c, cnt, x + 16.f, y + 62.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_L, AV_T);

    float ly = y + 82.f, lh = h - 94.f;
    float rowH = 54.f;
    float off = ui_scroll_begin(uid("opslist"), x + 6.f, ly, listW - 12.f, lh,
                                n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        float ry = ly + i*rowH - off;
        if (ry + rowH < ly || ry > ly + lh) continue;
        Flight *f = &wo->flight[idx[i]];
        draw_flight_row(a, x + 12.f, ry, listW - 24.f, rowH - 6.f, f,
                        a->selFlight == f->id, 0);
    }
    ui_scroll_end();
    if (!n) {
        tx_backdrop(C_SURF);
        tx_draw(c, "Nothing matches that search", x + listW*0.5f, ly + 40.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_T);
    }

    /* ---- the detail and the controls ------------------------------------ */
    float dx = x + listW + 16.f, dw = w - listW - 16.f;
    ui_card(dx, y, dw, h, R_LG);
    tx_backdrop(C_SURF);

    Flight *f = a->selFlight ? flight_by_id(wo, a->selFlight) : NULL;
    if (!f) {
        icon_draw(c, IC_PLANE, dx + dw*0.5f, y + h*0.5f - 22.f, 32.f, C_INK_4);
        tx_draw(c, "Select a movement to work on it", dx + dw*0.5f,
                y + h*0.5f + 8.f, font_make(TF_UI, 13, TW_MED), C_INK_3,
                AL_C, AV_M);
        tx_draw(c, "Retime, restatus, move gate, or cancel", dx + dw*0.5f,
                y + h*0.5f + 28.f, font_make(TF_UI, 11, TW_REG), C_INK_4,
                AL_C, AV_M);
        return;
    }

    Color ac = wo->airline[f->airline].col;
    Paint g = paint_linear(dx, y, dx + dw, y, col_alpha(ac, .16f),
                           col_alpha(ac, .02f));
    cv_rrect_p(c, dx, y, dw, 86.f, R_LG, &g);
    cv_rrect(c, dx, y, 4.f, 86.f, 2.f, ac);

    tx_draw(c, f->no, dx + 22.f, y + 14.f, font_track(TF_DISPLAY, 26, TW_BOLD, 1),
            C_INK, AL_L, AV_T);
    char sub[130];
    snprintf(sub, sizeof sub, "%s   %s %s   %s   %s",
             wo->airline[f->airline].name, f->arrival ? "from" : "to",
             wo->airport[f->airport].city, wo->actype[f->acType].code, f->reg);
    tx_clipped(c, sub, dx + 22.f, y + 48.f, dw - 200.f,
               font_make(TF_UI, 12, TW_MED), C_INK_2, AL_L, AV_T);

    char est[8], sch[8];
    fmt_hhmm(f->estMin, est); fmt_hhmm(f->schedMin, sch);
    tx_draw(c, est, dx + dw - 22.f, y + 14.f,
            font_track(TF_DISPLAY, 26, TW_BOLD, 0),
            f->delayMin >= 15 ? C_DANGER : C_INK, AL_R, AV_T);
    char dl[54];
    if (f->delayMin) snprintf(dl, sizeof dl, "sched %s, %+d min", sch, f->delayMin);
    else             snprintf(dl, sizeof dl, "on schedule");
    tx_draw(c, dl, dx + dw - 22.f, y + 50.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_R, AV_T);

    float ry = y + 100.f;

    /* status pill */
    ui_pill(dx + 22.f, ry, 24.f, fs_name(f->state), HEX(0xFFFFFF),
            fs_color(f->state));
    char gt[40];
    snprintf(gt, sizeof gt, "Gate %d   -   stand %s", f->gate,
             f->stand >= 0 ? wo->stand[f->stand].name : "--");
    tx_draw(c, gt, dx + dw - 22.f, ry + 12.f, font_make(TF_UI, 12, TW_SEMI),
            C_INK_2, AL_R, AV_M);
    ry += 38.f;

    /* ---- retiming -------------------------------------------------------- */
    tx_draw(c, "RETIME", dx + 22.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    struct { const char *lab; int mins; } DEL[5] = {
        { "-15", -15 }, { "-5", -5 }, { "+5", 5 }, { "+15", 15 }, { "+30", 30 },
    };
    float bw = (dw - 44.f - 4.f*6.f) / 5.f;
    for (int i = 0; i < 5; i++) {
        if (ui_button(uidi("opsdel", i), dx + 22.f + i*(bw + 6.f), ry, bw, 36.f,
                      DEL[i].lab, DEL[i].mins < 0 ? BTN_SOFT : BTN_OUTLINE)) {
            ops_flight_delay(wo, f->id, DEL[i].mins);
            nt_scan(&a->notify, wo);
        }
    }
    ry += 48.f;

    /* ---- status ---------------------------------------------------------- */
    tx_draw(c, "STATUS", dx + 22.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    struct { const char *lab; FlightState st; } ST[4] = {
        { "On time",  FS_SCHEDULED },
        { "Delayed",  FS_DELAYED   },
        { "Boarding", FS_BOARDING  },
        { "Departed", FS_DEPARTED  },
    };
    float sw2 = (dw - 44.f - 3.f*6.f) / 4.f;
    for (int i = 0; i < 4; i++) {
        int on = (f->state == ST[i].st);
        if (ui_button(uidi("opsst", i), dx + 22.f + i*(sw2 + 6.f), ry, sw2, 36.f,
                      ST[i].lab, on ? BTN_PRIMARY : BTN_SOFT)) {
            if (ST[i].st == FS_SCHEDULED && f->delayMin) {
                ops_flight_delay(wo, f->id, -f->delayMin);
            }
            ops_flight_state(wo, f->id, ST[i].st);
            nt_scan(&a->notify, wo);
        }
    }
    ry += 48.f;

    /* ---- gate -----------------------------------------------------------
     *  Plaisance has eight contact gates.  A gate is "taken" when another
     *  aeroplane is parked on it during the window this movement needs it --
     *  so the colours describe THIS flight's window, not the whole day, and
     *  the tile names the flight in the way, which is the thing a controller
     *  actually needs to know.                                             */
    int gfrom, gto;
    ops_gate_window(wo, f, &gfrom, &gto);
    char gw0[8], gw1[8]; fmt_hhmm(gfrom, gw0); fmt_hhmm(gto, gw1);
    char ghdr[90];
    snprintf(ghdr, sizeof ghdr, "GATE   -   needs one free %s to %s", gw0, gw1);
    tx_draw(c, ghdr, dx + 22.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);

    /* a legend, so the colours are not a guessing game */
    struct { const char *t; Color sw, tc; } LEG[3] = {
        { "on this gate", C_V600, C_V700 },
        { "free",         C_OK,   C_OK    },
        { "taken",        C_WARN, C_WARN  },
    };
    float lx = dx + dw - 22.f;
    for (int i = 2; i >= 0; i--) {
        float tw2 = (float)tx_width(c, LEG[i].t, font_make(TF_UI, 10, TW_MED));
        lx -= tw2 + 6.f;
        tx_draw(c, LEG[i].t, lx, ry + 1.f, font_make(TF_UI, 10, TW_MED),
                LEG[i].tc, AL_L, AV_T);
        lx -= 12.f;
        cv_circle(c, lx + 4.f, ry + 6.f, 4.f, LEG[i].sw);
        lx -= 12.f;
    }
    ry += 20.f;

    float gw = (dw - 44.f - 7.f*5.f) / 8.f;
    for (int i = 0; i < OPS_MAX_GATE; i++) {
        int gate = i + 1;
        float gx = dx + 22.f + i*(gw + 5.f), gy = ry, gh2 = 44.f;
        int occ = ops_gate_occupant(wo, gate, gfrom, gto, f->id);
        int on  = (f->gate == gate);
        int hov = ui_hit(gx, gy, gw, gh2);
        if (hov) ui_cursor(1);

        Color base = on ? C_V600 : (occ ? C_WARN : C_OK);
        Color bg   = on ? C_V600
                        : col_alpha(base, (hov ? .24f : .13f));
        Color fg   = on ? HEX(0xFFFFFF)
                        : (occ ? C_WARN : HEX(0x0A6B42));
        cv_rrect(c, gx, gy, gw, gh2, 8.f, bg);
        if (!on)
            cv_rrect_line(c, gx, gy, gw, gh2, 8.f, col_alpha(base, .5f), 1.2f);

        char lab[8]; snprintf(lab, sizeof lab, "%d", gate);
        tx_backdrop(on ? C_V600 : C_SURF);
        tx_draw(c, lab, gx + gw*0.5f, gy + (occ ? 11.f : 15.f),
                font_track(TF_DISPLAY, 16, TW_BOLD, 0), fg, AL_C, AV_T);
        if (occ) {
            Flight *of = flight_by_id(wo, occ);
            tx_clipped(c, of ? of->no : "--", gx + gw*0.5f, gy + 29.f, gw - 6.f,
                       font_make(TF_UI, 9, TW_SEMI), fg, AL_C, AV_T);
        }

        if (hov) {
            char tip[90];
            if (on)       snprintf(tip, sizeof tip, "Gate %d -- this flight", gate);
            else if (occ) {
                Flight *of = flight_by_id(wo, occ);
                int o0, o1; if (of) ops_gate_window(wo, of, &o0, &o1);
                char oh[8]; fmt_hhmm(of ? o1 : 0, oh);
                snprintf(tip, sizeof tip, "Gate %d -- taken by %s until %s",
                         gate, of ? of->no : "another flight", oh);
            } else snprintf(tip, sizeof tip, "Gate %d -- free, click to assign",
                            gate);
            ui_tooltip(gx, gy - 6.f, tip);

            if (a->in.pressed) {
                ops_flight_gate(wo, f->id, gate);
                nt_scan(&a->notify, wo);
                if (occ)
                    ui_toast(TOAST_WARN, "Gate clash",
                             "That gate is taken in this flight's window.");
            }
        }
    }
    ry += 52.f;

    int sug = ops_gate_suggest(wo, f->id);
    if (sug > 0) {
        char st2[100];
        snprintf(st2, sizeof st2,
                 "Gate %d is free for this movement -- click it above", sug);
        tx_draw(c, st2, dx + 22.f, ry, font_make(TF_UI, 11, TW_MED),
                C_OK, AL_L, AV_T);
    } else {
        tx_draw(c, "Every contact gate is taken in this window -- retime, or "
                   "use a bussed remote stand", dx + 22.f, ry,
                font_make(TF_UI, 11, TW_MED), C_WARN, AL_L, AV_T);
    }
    ry += 26.f;

    /* ---- cancel ---------------------------------------------------------- */
    if (ry + 44.f < y + h) {
        if (f->state == FS_CANCELLED) {
            cv_rrect(c, dx + 22.f, ry, dw - 44.f, 40.f, 10.f,
                     col_alpha(C_DANGER, .10f));
            tx_draw(c, "This flight is cancelled", dx + dw*0.5f, ry + 20.f,
                    font_make(TF_UI, 12, TW_SEMI), C_DANGER, AL_C, AV_M);
        } else if (ui_button_i(uid("opscancel"), dx + 22.f, ry, dw - 44.f, 40.f,
                               "Cancel this flight", IC_CLOSE, BTN_DANGER)) {
            if (ops_flight_cancel(wo, f->id)) {
                nt_scan(&a->notify, wo);
                ui_toast(TOAST_WARN, "Flight cancelled",
                         "Passengers have been notified and the gate released.");
            } else {
                ui_toast(TOAST_ERR, "Cannot cancel",
                         "The aircraft has already gone.");
            }
        }
    }
}

/* ==========================================================================
 *  the gate board
 * ========================================================================== */

static void tab_gates(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    GateConflict gc[64];
    int nc = ops_gate_conflicts(wo, gc, 64);
    DelayImpact di[32];
    int nd = ops_delay_impact(wo, di, 32);

    /* ---- the Gantt ------------------------------------------------------ */
    float chartW = w*0.62f;
    ui_card(x, y, chartW, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "GATE OCCUPANCY", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "every gate against the clock -- overlaps are drawn in red",
            x + 18.f, y + 32.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    float gx = x + 66.f, gw = chartW - 84.f;
    float gy = y + 58.f;
    /* fill the panel: eight rows in a tall card should not sit in a band
     * across the top with half the card empty below them */
    float rowH = (h - 92.f) / (float)OPS_MAX_GATE;
    if (rowH > 56.f) rowH = 56.f;

    /* hour grid, 05:00 to midnight */
    const float T0 = 300.f, T1 = 1440.f;
    for (int hr = 6; hr <= 24; hr += 3) {
        float hx = gx + gw * ((float)hr*60.f - T0) / (T1 - T0);
        cv_rect(c, hx, gy, 1.f, rowH*OPS_MAX_GATE, col_alpha(C_LINE, .9f));
        char lab[8]; snprintf(lab, sizeof lab, "%02d", hr % 24);
        tx_draw(c, lab, hx, gy - 14.f, font_make(TF_UI, 9, TW_MED),
                C_INK_4, AL_C, AV_T);
    }

    for (int g = 1; g <= OPS_MAX_GATE; g++) {
        float ry = gy + (g-1)*rowH;
        if (g & 1) cv_rect(c, gx, ry, gw, rowH - 2.f, col_alpha(C_SURF_2, .8f));
        char lab[10]; snprintf(lab, sizeof lab, "Gate %d", g);
        tx_backdrop(C_SURF);
        tx_draw(c, lab, x + 58.f, ry + rowH*0.5f, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_R, AV_M);

        for (int i = 0; i < wo->nFlights; i++) {
            Flight *f = &wo->flight[i];
            if (f->gate != g || f->state == FS_CANCELLED) continue;
            int from, to;
            ops_gate_window(wo, f, &from, &to);
            float bx = gx + gw * ((float)from - T0) / (T1 - T0);
            float bw = gw * (float)(to - from) / (T1 - T0);
            if (bw < 3.f) bw = 3.f;
            if (bx + bw < gx || bx > gx + gw) continue;

            int clash = 0;
            for (int k = 0; k < nc; k++)
                if (gc[k].a == f->id || gc[k].b == f->id) { clash = 1; break; }

            Color bc = clash ? C_DANGER : wo->airline[f->airline].col;
            cv_rrect(c, bx, ry + 3.f, bw, rowH - 8.f, 4.f,
                     col_alpha(bc, clash ? .85f : .55f));
            if (bw > 44.f) {
                tx_backdrop(col_alpha(bc, .7f));
                tx_clipped(c, f->no, bx + 5.f, ry + rowH*0.5f - 6.f, bw - 8.f,
                           font_track(TF_UI, 9, TW_BOLD, 0), HEX(0xFFFFFF),
                           AL_L, AV_T);
            }
            if (ui_hit(bx, ry + 3.f, bw, rowH - 8.f)) {
                ui_cursor(1);
                if (a->in.pressed) { a->selFlight = f->id; a->opsTab = OPT_FLIGHTS; }
            }
        }
    }

    /* the live clock line */
    float nx = gx + gw * (wo->clock - T0) / (T1 - T0);
    if (nx >= gx && nx <= gx + gw) {
        cv_rect(c, nx, gy - 6.f, 1.6f, rowH*OPS_MAX_GATE + 8.f,
                col_alpha(C_MAGENTA, .9f));
        cv_circle(c, nx, gy - 8.f, 3.4f, C_MAGENTA);
    }

    /* ---- conflicts ------------------------------------------------------ */
    float dx = x + chartW + 16.f, dw = w - chartW - 16.f;
    ui_card(dx, y, dw, h, R_LG);
    tx_backdrop(C_SURF);

    Color hdr = nc ? C_DANGER : C_OK;
    cv_circle(c, dx + 26.f, y + 24.f, 12.f, col_alpha(hdr, .14f));
    icon_draw(c, nc ? IC_ALERT : IC_CHECK, dx + 26.f, y + 24.f, 14.f, hdr);
    char t[70];
    snprintf(t, sizeof t, nc ? "%d gate conflict%s" : "No gate conflicts",
             nc, nc == 1 ? "" : "s");
    tx_draw(c, t, dx + 46.f, y + 15.f, font_track(TF_DISPLAY, 17, TW_BOLD, 0),
            C_INK, AL_L, AV_T);
    tx_draw(c, nc ? "two aircraft on one gate at the same time"
                  : "every movement has a gate to itself",
            dx + 46.f, y + 36.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    float ry = y + 62.f;
    float lh = h - 74.f;
    float off = ui_scroll_begin(uid("opsconf"), dx + 6.f, ry, dw - 12.f, lh,
                                nd*116.f + 8.f);
    for (int i = 0; i < nd; i++) {
        float cy2 = ry + i*116.f - off;
        if (cy2 + 116.f < ry || cy2 > ry + lh) continue;

        cv_rrect(c, dx + 12.f, cy2, dw - 24.f, 108.f, 10.f,
                 col_alpha(C_DANGER, .07f));
        cv_rrect(c, dx + 12.f, cy2, 3.f, 108.f, 1.5f, C_DANGER);

        tx_backdrop(C_SURF);
        char hd[60];
        snprintf(hd, sizeof hd, "GATE %d  -  %d MIN OVERLAP",
                 di[i].gate, di[i].overlap);
        tx_draw(c, hd, dx + 24.f, cy2 + 10.f, font_track(TF_UI, 9, TW_BOLD, 1),
                C_DANGER, AL_L, AV_T);
        tx_para(c, di[i].text, dx + 24.f, cy2 + 26.f, dw - 48.f,
                font_make(TF_UI, 11, TW_MED), C_INK_2, 3);

        if (di[i].suggested > 0) {
            char btn[52];
            snprintf(btn, sizeof btn, "Move to gate %d", di[i].suggested);
            if (ui_button(uidi("opsfix", i), dx + 24.f, cy2 + 70.f,
                          dw - 48.f, 30.f, btn, BTN_SUCCESS)) {
                ops_flight_gate(wo, di[i].victimId, di[i].suggested);
                nt_scan(&a->notify, wo);
                Flight *v = flight_by_id(wo, di[i].victimId);
                char m[100];
                snprintf(m, sizeof m, "%s moved to gate %d",
                         v ? v->no : "Flight", di[i].suggested);
                ui_toast(TOAST_OK, "Conflict resolved", m);
            }
        } else {
            tx_draw(c, "No free gate -- one of the two must be retimed",
                    dx + 24.f, cy2 + 78.f, font_make(TF_UI, 11, TW_SEMI),
                    C_WARN, AL_L, AV_T);
        }
    }
    ui_scroll_end();

    if (!nd) {
        icon_draw(c, IC_CHECK, dx + dw*0.5f, y + h*0.5f - 10.f, 30.f,
                  col_alpha(C_OK, .5f));
        tx_backdrop(C_SURF);
        tx_draw(c, "The gate plan is clean", dx + dw*0.5f, y + h*0.5f + 20.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    }
}

/* ==========================================================================
 *  the runway
 * ========================================================================== */

static void tab_runway(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    RunwaySlot sl[MAX_FLIGHTS];
    int ns = ops_runway_slots(wo, sl, MAX_FLIGHTS);
    int pairs[64];
    int np = ops_runway_conflicts(wo, pairs, 32);

    int busyBy = 0;
    int busy   = ops_runway_occupied(wo, wo->clock, &busyBy);

    /* ---- status ---------------------------------------------------------- */
    float tileH = 96.f;
    float tw = (w - 3.f*14.f) / 4.f;
    char v[40], s2[70];

    snprintf(v, sizeof v, "%02d", wo->activeRunway);
    snprintf(s2, sizeof s2, "3,390 m  -  wind %03.0f/%.0fkt",
             wo->windDir, wo->windKt);
    draw_stat_tile(c, x, y, tw, tileH, "RUNWAY IN USE", v, s2, IC_RUNWAY, C_V600);

    Flight *bf = busy ? flight_by_id(wo, busyBy) : NULL;
    snprintf(v, sizeof v, "%s", busy ? "OCCUPIED" : "CLEAR");
    snprintf(s2, sizeof s2, "%s", bf ? bf->no : "no movement on the runway");
    draw_stat_tile(c, x + tw + 14.f, y, tw, tileH, "RIGHT NOW", v, s2,
                   busy ? IC_ALERT : IC_CHECK, busy ? C_WARN : C_OK);

    snprintf(v, sizeof v, "%d", ns);
    snprintf(s2, sizeof s2, "%d min separation between movements", OPS_RWY_SEP);
    draw_stat_tile(c, x + 2*(tw + 14.f), y, tw, tileH, "MOVEMENTS TODAY", v, s2,
                   IC_PLANE, C_TEAL);

    snprintf(v, sizeof v, "%d", np);
    snprintf(s2, sizeof s2, "%s", np ? "two aircraft too close together"
                                     : "separation is respected all day");
    draw_stat_tile(c, x + 3*(tw + 14.f), y, tw, tileH, "CONFLICTS", v, s2,
                   np ? IC_ALERT : IC_SHIELD, np ? C_DANGER : C_OK);

    /* ---- the timeline ---------------------------------------------------- */
    float ty = y + tileH + 16.f;
    float th = h - tileH - 16.f;
    ui_card(x, ty, w, th, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "RUNWAY TIMELINE", x + 18.f, ty + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "arrivals above the line, departures below; red means the "
               "separation minimum is broken",
            x + 18.f, ty + 32.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    float gx = x + 30.f, gw = w - 60.f;
    float mid = ty + th*0.52f;
    const float T0 = 300.f, T1 = 1440.f;

    cv_rect(c, gx, mid - 1.f, gw, 2.f, C_LINE_2);
    for (int hr = 5; hr <= 24; hr++) {
        float hx = gx + gw * ((float)hr*60.f - T0) / (T1 - T0);
        int major = (hr % 3 == 0);
        cv_rect(c, hx, mid - (major ? 8.f : 4.f), 1.f, major ? 16.f : 8.f,
                col_alpha(C_LINE_2, major ? 1.f : .6f));
        if (major) {
            char lab[8]; snprintf(lab, sizeof lab, "%02d", hr % 24);
            tx_draw(c, lab, hx, mid + 12.f, font_make(TF_UI, 9, TW_MED),
                    C_INK_4, AL_C, AV_T);
        }
    }

    for (int i = 0; i < ns; i++) {
        Flight *f = flight_by_id(wo, sl[i].flightId);
        if (!f) continue;
        float bx = gx + gw * ((float)sl[i].fromMin - T0) / (T1 - T0);
        float bw = gw * (float)(sl[i].toMin - sl[i].fromMin) / (T1 - T0);
        if (bw < 2.5f) bw = 2.5f;

        int clash = 0;
        for (int k = 0; k < np; k++)
            if (pairs[k*2] == f->id || pairs[k*2+1] == f->id) { clash = 1; break; }

        Color bc = clash ? C_DANGER
                         : (sl[i].arrival ? C_TEAL : C_V600);
        float bh = 26.f;
        float by = sl[i].arrival ? mid - 10.f - bh : mid + 10.f;
        /* stagger so adjacent movements do not sit on top of each other */
        by += (float)((i % 3) - 1) * (sl[i].arrival ? -30.f : 30.f);

        cv_rrect(c, bx, by, bw, bh, 4.f, col_alpha(bc, clash ? .9f : .55f));
        if (bw > 30.f) {
            tx_backdrop(col_alpha(bc, .6f));
            tx_clipped(c, f->no, bx + 4.f, by + 7.f, bw - 6.f,
                       font_track(TF_UI, 9, TW_BOLD, 0), HEX(0xFFFFFF),
                       AL_L, AV_T);
        }
        if (ui_hit(bx, by, bw < 14.f ? 14.f : bw, bh)) {
            ui_cursor(1);
            char tip[110];
            char hm[8]; fmt_hhmm(f->estMin, hm);
            snprintf(tip, sizeof tip, "%s  %s  %s  %s", f->no, hm,
                     sl[i].arrival ? "arrival" : "departure",
                     wo->airport[f->airport].city);
            ui_tooltip(bx, by - 8.f, tip);
            if (a->in.pressed) { a->selFlight = f->id; a->opsTab = OPT_FLIGHTS; }
        }
    }

    float nx = gx + gw * (wo->clock - T0) / (T1 - T0);
    if (nx >= gx && nx <= gx + gw)
        cv_rect(c, nx, ty + 50.f, 1.6f, th - 70.f, col_alpha(C_MAGENTA, .8f));

    /* ---- next free slot -------------------------------------------------- */
    int nf = ops_runway_next_free(wo, (int)wo->clock);
    char hm[8]; fmt_hhmm(nf, hm);
    char msg[130];
    if (np)
        snprintf(msg, sizeof msg,
                 "%d pair%s of movements are inside the %d minute separation "
                 "minimum. Next clear slot: %s.",
                 np, np == 1 ? "" : "s", OPS_RWY_SEP, hm);
    else
        snprintf(msg, sizeof msg,
                 "Separation is respected across all %d movements. "
                 "Next clear slot for an unscheduled departure: %s.", ns, hm);
    tx_backdrop(C_SURF);
    tx_para(c, msg, x + 20.f, ty + th - 44.f, w - 40.f,
            font_make(TF_UI, 12, TW_MED), np ? C_DANGER : C_INK_2, 2);
}

/* ==========================================================================
 *  add a flight
 * ========================================================================== */

/*  slot must be distinct per picker on screen: the widget id is built from
 *  it, and two pickers sharing an id share their hover and press state.   */
static void picker(App *a, int slot, float x, float y, float w,
                   const char *label, const char *value, int *idx, int count)
{
    Canvas *c = &a->cv;
    if (count < 1) return;
    tx_backdrop(C_SURF);
    tx_draw(c, label, x, y, font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3,
            AL_L, AV_T);
    float by = y + 17.f;
    if (ui_icon_btn(uidi("pkl", slot), x, by, 36.f, IC_CHEV_L, BTN_SOFT))
        *idx = (*idx - 1 + count) % count;
    if (ui_icon_btn(uidi("pkr", slot), x + w - 36.f, by, 36.f, IC_CHEV_R,
                    BTN_SOFT))
        *idx = (*idx + 1) % count;
    cv_rrect(c, x + 42.f, by, w - 84.f, 36.f, 9.f, C_SURF_2);
    tx_backdrop(C_SURF_2);
    tx_clipped(c, value, x + w*0.5f, by + 18.f, w - 96.f,
               font_make(TF_UI, 12, TW_SEMI), C_INK, AL_C, AV_M);
}

static void tab_new(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    float fw = 520.f;
    if (fw > w - 40.f) fw = w - 40.f;
    float fx = x + (w - fw)*0.5f;

    ui_card(fx, y, fw, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "ADD A MOVEMENT", fx + 24.f, y + 20.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "a charter, a diversion, a positioning flight -- anything that "
               "was not in the published schedule",
            fx + 24.f, y + 38.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    float ry = y + 70.f;
    float half = (fw - 48.f - 14.f) * 0.5f;

    tx_draw(c, "FLIGHT NUMBER", fx + 24.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ui_text_field(uid("nfno"), fx + 24.f, ry + 17.f, half, 40.f, a->nfNo,
                  (int)sizeof a->nfNo, "MK123", IC_TAG);

    tx_draw(c, "TIME  (HH:MM)", fx + 24.f + half + 14.f, ry,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    ui_text_field(uid("nftime"), fx + 24.f + half + 14.f, ry + 17.f, half, 40.f,
                  a->nfTime, (int)sizeof a->nfTime, "14:30", IC_CLOCK);
    ry += 76.f;

    picker(a, 0, fx + 24.f, ry, fw - 48.f, "AIRLINE",
           wo->airline[a->nfAirline].name, &a->nfAirline, wo->nAirlines);
    ry += 66.f;
    picker(a, 1, fx + 24.f, ry, fw - 48.f, "AIRCRAFT",
           wo->actype[a->nfType].name, &a->nfType, wo->nActypes);
    ry += 66.f;
    picker(a, 2, fx + 24.f, ry, fw - 48.f,
           a->nfArrival ? "ORIGIN" : "DESTINATION",
           wo->airport[a->nfDest].city, &a->nfDest, wo->nAirports);
    ry += 66.f;

    tx_draw(c, "DIRECTION", fx + 24.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 17.f;
    if (ui_button_i(uid("nfdep"), fx + 24.f, ry, half, 40.f, "Departure",
                    IC_TAKEOFF, a->nfArrival ? BTN_SOFT : BTN_PRIMARY))
        a->nfArrival = 0;
    if (ui_button_i(uid("nfarr"), fx + 24.f + half + 14.f, ry, half, 40.f,
                    "Arrival", IC_LANDING,
                    a->nfArrival ? BTN_PRIMARY : BTN_SOFT))
        a->nfArrival = 1;
    ry += 56.f;

    /*  Validate before offering the button rather than after pressing it, so
     *  the reason it is disabled is on screen next to it.                   */
    int hh = -1, mm = -1;
    if (strlen(a->nfTime) >= 4) {
        int got = sscanf(a->nfTime, "%d:%d", &hh, &mm);
        if (got != 2) { hh = -1; mm = -1; }
    }
    int timeOK = (hh >= 0 && hh < 24 && mm >= 0 && mm < 60);
    int noOK   = (strlen(a->nfNo) >= 3);
    int dupe   = noOK && flight_by_no(wo, a->nfNo) != NULL;
    int roomOK = wo->nFlights < MAX_FLIGHTS;

    const char *why = NULL;
    if (!noOK)        why = "A flight number needs at least three characters.";
    else if (dupe)    why = "That flight number is already operating today.";
    else if (!timeOK) why = "Enter the time as HH:MM, for example 14:30.";
    else if (!roomOK) why = "The day is full -- no more movements can be added.";

    if (why) {
        tx_draw(c, why, fx + 24.f, ry + 12.f, font_make(TF_UI, 11, TW_MED),
                C_WARN, AL_L, AV_T);
    } else if (ui_button_i(uid("nfadd"), fx + 24.f, ry, fw - 48.f, 46.f,
                           "Create the flight", IC_PLUS, BTN_PRIMARY)) {
        FlightSpec sp;
        memset(&sp, 0, sizeof sp);
        snprintf(sp.no, sizeof sp.no, "%s", a->nfNo);
        sp.airline  = a->nfAirline;
        sp.acType   = a->nfType;
        sp.airport  = a->nfDest;
        sp.arrival  = a->nfArrival;
        sp.schedMin = hh*60 + mm;

        int id = ops_flight_add(wo, &sp);
        if (id) {
            a->selFlight = id;
            a->opsTab    = OPT_FLIGHTS;
            a->nfNo[0]   = 0;
            a->nfTime[0] = 0;
            ai_delay_run_all(&a->delay, wo);
            nt_scan(&a->notify, wo);
            Flight *nf = flight_by_id(wo, id);
            char m[110];
            snprintf(m, sizeof m, "%s added on gate %d", nf ? nf->no : "Flight",
                     nf ? nf->gate : 0);
            ui_toast(TOAST_OK, "Movement created", m);
        } else {
            ui_toast(TOAST_ERR, "Not created",
                     "The flight could not be added to today's programme.");
        }
    }
}

/* ==========================================================================
 *  the screen
 * ========================================================================== */

void screen_ops(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    float pad  = PAD;

    static const struct { const char *lab; int icon; } TAB[OPT_COUNT] = {
        { "Flights",    IC_PLANE    },
        { "Gates",      IC_GATE     },
        { "Runway",     IC_RUNWAY   },
        { "Add flight", IC_PLUS     },
    };

    float cy = y + pad;
    float tw = 148.f;
    for (int i = 0; i < OPT_COUNT; i++)
        if (ui_tab(uidi("opstab", i), x + pad + i*(tw + 6.f), cy, tw, 40.f,
                   TAB[i].lab, TAB[i].icon, a->opsTab == i))
            a->opsTab = i;

    /* a live conflict count, always visible whichever tab is open */
    GateConflict gc[64];
    int nc = ops_gate_conflicts(wo, gc, 64);
    int pairs[64];
    int np = ops_runway_conflicts(wo, pairs, 32);
    char st[110];
    snprintf(st, sizeof st, "%d movements   -   %d gate conflict%s   -   "
                            "%d runway conflict%s",
             wo->nFlights, nc, nc == 1 ? "" : "s", np, np == 1 ? "" : "s");
    tx_backdrop(C_BG);
    tx_draw(c, st, x + w - pad, cy + 20.f, font_make(TF_UI, 12, TW_MED),
            (nc || np) ? C_DANGER : C_INK_3, AL_R, AV_M);

    float bx = x + pad, by = cy + 56.f;
    float bw = w - pad*2.f, bh = h - (by - y) - pad;

    switch (a->opsTab) {
    case OPT_GATES:  tab_gates  (a, bx, by, bw, bh); break;
    case OPT_RUNWAY: tab_runway (a, bx, by, bw, bh); break;
    case OPT_NEW:    tab_new    (a, bx, by, bw, bh); break;
    default:         tab_flights(a, bx, by, bw, bh); break;
    }
}
