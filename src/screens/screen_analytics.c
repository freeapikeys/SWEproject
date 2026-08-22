/* ==========================================================================
 *  AURA :: screen_analytics.c   --   how the day is going
 *
 *  The rest of the application stores and moves data.  This reads it back and
 *  answers the questions somebody running the airport actually asks: how many
 *  movements, how many late and by how much, which gate is doing the most
 *  work, where is everybody going, and is the baggage system keeping up.
 *
 *  Nothing here is stored.  The report is recomputed from the live world a
 *  few times a second, so it can never drift out of step with the screens it
 *  is describing.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 *  a horizontal league table
 * ------------------------------------------------------------------------- */

static void league(App *a, float x, float y, float w, float h,
                   const char *title, const char *sub,
                   const AnRow *rows, int n, Color col, int icon)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);

    cv_circle(c, x + 26.f, y + 24.f, 13.f, col_alpha(col, .12f));
    icon_draw(c, icon, x + 26.f, y + 24.f, 14.f, col);
    tx_draw(c, title, x + 46.f, y + 13.f, font_track(TF_UI, 10, TW_BOLD, 2),
            col, AL_L, AV_T);
    tx_clipped(c, sub, x + 46.f, y + 30.f, w - 66.f,
               font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

    float ry = y + 54.f;
    float rowH = (h - 66.f) / (n > 0 ? (float)n : 1.f);
    if (rowH > 34.f) rowH = 34.f;
    int   peak = n ? rows[0].value : 1;
    if (peak < 1) peak = 1;

    for (int i = 0; i < n; i++) {
        float yy = ry + i*rowH;
        if (yy + rowH > y + h) break;

        float frac = (float)rows[i].value / (float)peak;
        /* the bar sits behind the label rather than beside it: the name is
         * what you read, the bar is what you compare */
        cv_rrect(c, x + 18.f, yy, (w - 36.f)*frac, rowH - 6.f, 6.f,
                 col_alpha(col, .13f));

        tx_clipped(c, rows[i].label, x + 26.f, yy + rowH*0.5f - 7.f,
                   w - 110.f, font_make(TF_UI, 12, TW_MED), C_INK, AL_L, AV_T);
        char v[24];
        snprintf(v, sizeof v, "%d", rows[i].value);
        tx_draw(c, v, x + w - 56.f, yy + rowH*0.5f, font_make(TF_UI, 12, TW_BOLD),
                C_INK, AL_R, AV_M);
        char p[16];
        snprintf(p, sizeof p, "%.0f%%", rows[i].pct);
        tx_draw(c, p, x + w - 18.f, yy + rowH*0.5f, font_make(TF_UI, 10, TW_MED),
                C_INK_4, AL_R, AV_M);
    }
    if (!n)
        tx_draw(c, "No data yet", x + w*0.5f, y + h*0.5f,
                font_make(TF_UI, 11, TW_MED), C_INK_4, AL_C, AV_M);
}

/* --------------------------------------------------------------------------
 *  movements by hour
 * ------------------------------------------------------------------------- */

static void by_hour(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    Report *r  = &a->report;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "MOVEMENTS BY HOUR", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char sub[110];
    snprintf(sub, sizeof sub, "peak %d at %02d:00  -  runway %.0f%% used",
             r->movementsPeakHour, r->peakHour, r->runwayUtilPct);
    tx_clipped(c, sub, x + 18.f, y + 32.f, w - 36.f,
               font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);

    float gx = x + 24.f, gw = w - 48.f;
    float gy = y + 58.f, gh = h - 88.f;
    int peak = r->movementsPeakHour > 0 ? r->movementsPeakHour : 1;
    float bw = gw / 24.f;

    for (int i = 0; i < 24; i++) {
        float v = (float)r->byHour[i].value / (float)peak;
        float bh = 4.f + v * (gh - 8.f);
        int isNow = ((int)wo->clock / 60) == i;
        Color bc = isNow ? C_MAGENTA
                 : (r->byHour[i].value == peak ? C_V600 : C_V400);
        cv_rrect(c, gx + i*bw + 1.5f, gy + gh - bh, bw - 3.f, bh, 3.f,
                 col_alpha(bc, r->byHour[i].value ? .85f : .16f));

        if (ui_hit(gx + i*bw, gy, bw, gh)) {
            ui_cursor(1);
            char tip[60];
            snprintf(tip, sizeof tip, "%02d:00  -  %d movements",
                     i, r->byHour[i].value);
            ui_tooltip(gx + i*bw, gy - 6.f, tip);
        }
        if (i % 3 == 0) {
            char lab[6]; snprintf(lab, sizeof lab, "%02d", i);
            tx_draw(c, lab, gx + i*bw + bw*0.5f, gy + gh + 6.f,
                    font_make(TF_UI, 9, TW_MED), C_INK_4, AL_C, AV_T);
        }
    }
}

/* ==========================================================================
 *  the screen
 * ========================================================================== */

void screen_analytics(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    float pad  = PAD;

    /*  Rebuilt a few times a second rather than every frame: the report walks
     *  every flight, passenger and bag, and nothing on it changes fast enough
     *  to be worth doing sixty times a second.                             */
    a->reportTimer += a->dt;
    if (a->reportTimer > 0.5f || a->report.flights == 0) {
        a->reportTimer = 0.f;
        an_build(&a->report, wo);
    }
    Report *r = &a->report;

    /* ---- headline -------------------------------------------------------- */
    float bx = x + pad, bw = w - pad*2.f;
    float hy = y + pad;

    ui_card(bx, hy, bw, 74.f, R_LG);
    tx_backdrop(C_SURF);
    char hd[80];
    snprintf(hd, sizeof hd, "%s %d %s %d", weekday_name(wo->weekday), wo->day,
             month_name(wo->month), wo->year);
    tx_draw(c, hd, bx + 22.f, hy + 14.f, font_track(TF_DISPLAY, 20, TW_BOLD, 0),
            C_INK, AL_L, AV_T);
    char line[170];
    an_headline(r, line, (int)sizeof line);
    tx_clipped(c, line, bx + 22.f, hy + 42.f, bw - 260.f,
               font_make(TF_UI, 12, TW_MED), C_INK_2, AL_L, AV_T);

    if (ui_button_i(uid("anexport"), bx + bw - 210.f, hy + 18.f, 190.f, 38.f,
                    "Write the report to disk", IC_SAVE, BTN_PRIMARY)) {
        StoreResult res = store_export_report(wo);
        ui_toast(res.ok ? TOAST_OK : TOAST_ERR,
                 res.ok ? "Report written" : "Could not write", res.message);
    }

    /* ---- the numbers ----------------------------------------------------- */
    float ty = hy + 86.f;
    float tileH = 92.f;
    float tw = (bw - 5.f*12.f) / 6.f;
    char v[40], s2[70];

    snprintf(v, sizeof v, "%d", r->flights);
    snprintf(s2, sizeof s2, "%d arrivals, %d departures", r->arrivals,
             r->departures);
    draw_stat_tile(c, bx, ty, tw, tileH, "MOVEMENTS", v, s2, IC_PLANE, C_V600);

    snprintf(v, sizeof v, "%.0f%%", r->onTimePct);
    snprintf(s2, sizeof s2, "%d on time of %d", r->onTime,
             r->flights - r->cancelled);
    draw_stat_tile(c, bx + (tw+12.f), ty, tw, tileH, "PUNCTUALITY", v, s2,
                   IC_CHECK, r->onTimePct >= 80.f ? C_OK : C_WARN);

    snprintf(v, sizeof v, "%d", r->delayed);
    snprintf(s2, sizeof s2, "average %.0f min late", r->avgDelayOfLate);
    draw_stat_tile(c, bx + 2*(tw+12.f), ty, tw, tileH, "DELAYED", v, s2,
                   IC_CLOCK, r->delayed ? C_WARN : C_OK);

    snprintf(v, sizeof v, "%d", r->cancelled);
    snprintf(s2, sizeof s2, "%s", r->cancelled ? "gates released"
                                               : "none cancelled today");
    draw_stat_tile(c, bx + 3*(tw+12.f), ty, tw, tileH, "CANCELLED", v, s2,
                   IC_CLOSE, r->cancelled ? C_DANGER : C_OK);

    snprintf(v, sizeof v, "%d", r->paxTotal);
    snprintf(s2, sizeof s2, "%.0f%% of seats sold", r->loadFactor);
    draw_stat_tile(c, bx + 4*(tw+12.f), ty, tw, tileH, "PASSENGERS", v, s2,
                   IC_USERS, C_TEAL);

    snprintf(v, sizeof v, "%d", r->busiestGate);
    snprintf(s2, sizeof s2, "%d movements today", r->busiestGateCount);
    draw_stat_tile(c, bx + 5*(tw+12.f), ty, tw, tileH, "BUSIEST GATE", v, s2,
                   IC_GATE, C_MAGENTA);

    /* second row */
    float t2y = ty + tileH + 12.f;

    snprintf(v, sizeof v, "%.0f min", r->avgDelayAll);
    snprintf(s2, sizeof s2, "worst %s at %+d min",
             r->worstFlight[0] ? r->worstFlight : "--", r->worstDelay);
    draw_stat_tile(c, bx, t2y, tw, tileH, "AVERAGE DELAY", v, s2, IC_TREND,
                   r->avgDelayAll > 15.f ? C_WARN : C_OK);

    snprintf(v, sizeof v, "%d", r->bags);
    snprintf(s2, sizeof s2, "%d in the system, %d loaded", r->bagsInSystem,
             r->bagsLoaded);
    draw_stat_tile(c, bx + (tw+12.f), t2y, tw, tileH, "BAGS", v, s2,
                   IC_LUGGAGE, C_GOLD);

    snprintf(v, sizeof v, "%.1f", r->mishandledPer1000);
    snprintf(s2, sizeof s2, "%d mishandled of %d", r->bagsMishandled, r->bags);
    draw_stat_tile(c, bx + 2*(tw+12.f), t2y, tw, tileH, "PER 1000 BAGS", v, s2,
                   IC_ALERT, r->mishandledPer1000 > 8.f ? C_DANGER : C_OK);

    snprintf(v, sizeof v, "%d", r->gateConflicts);
    snprintf(s2, sizeof s2, "%d stands of %d in use", r->standsUsed,
             r->standsTotal);
    draw_stat_tile(c, bx + 3*(tw+12.f), t2y, tw, tileH, "GATE CONFLICTS", v, s2,
                   IC_GATE, r->gateConflicts ? C_DANGER : C_OK);

    snprintf(v, sizeof v, "%d", r->assistance);
    snprintf(s2, sizeof s2, "%d priority, %d infants", r->priority, r->infants);
    draw_stat_tile(c, bx + 4*(tw+12.f), t2y, tw, tileH, "ASSISTANCE", v, s2,
                   IC_HEART, C_V500);

    snprintf(v, sizeof v, "%d", emg_active(&a->emg));
    snprintf(s2, sizeof s2, "%d raised, %d resolved", a->emg.totalRaised,
             a->emg.totalResolved);
    draw_stat_tile(c, bx + 5*(tw+12.f), t2y, tw, tileH, "INCIDENTS", v, s2,
                   IC_SHIELD, emg_active(&a->emg) ? C_DANGER : C_OK);

    /* ---- the tables ------------------------------------------------------ */
    float ly = t2y + tileH + 14.f;
    float lh = h - (ly - y) - pad;
    if (lh < 120.f) return;

    float colW = (bw - 3.f*14.f) / 4.f;
    league(a, bx, ly, colW, lh, "BUSIEST DESTINATIONS",
           "where the aircraft are going", r->topDest, r->nTopDest,
           C_V600, IC_GLOBE);
    league(a, bx + colW + 14.f, ly, colW, lh, "AIRLINES",
           "movements operated today", r->topAirline, r->nTopAirline,
           C_TEAL, IC_PLANE);
    league(a, bx + 2*(colW + 14.f), ly, colW, lh, "GATE LOAD",
           "how the work is spread", r->topGate, r->nTopGate,
           C_MAGENTA, IC_GATE);
    by_hour(a, bx + 3*(colW + 14.f), ly, colW, lh);
}
