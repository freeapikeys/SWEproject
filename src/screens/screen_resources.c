/* ==========================================================================
 *  AURA :: screen_resources.c   --   fleet, people and property
 *
 *  The things the airport has, rather than the things it is doing.  The
 *  aircraft types it can handle and what each one needs from a stand; the
 *  staff on duty and which shift they are working; the passengers who have
 *  asked for assistance and therefore need a person allocated to them; and
 *  the lost property desk.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int ci_has(const char *hay, const char *needle)
{
    if (!needle || !*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *p = h, *q = needle;
        while (*p && *q) {
            char cp = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p;
            char cq = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q;
            if (cp != cq) break;
            p++; q++;
        }
        if (!*q) return 1;
    }
    return 0;
}

/* ==========================================================================
 *  aircraft
 * ========================================================================== */

static void tab_aircraft(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "AIRCRAFT TYPES CLEARED FOR PLAISANCE", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "wingspan decides which stands can take it, and containers "
               "decide how long the turnaround is",
            x + 20.f, y + 33.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    const char *COL[6] = { "TYPE", "AIRCRAFT", "SEATS", "SPAN", "ULD",
                           "IN SERVICE TODAY" };
    float cx[6];
    cx[0] = x + 24.f;   cx[1] = x + 90.f;
    cx[2] = x + w*0.46f; cx[3] = x + w*0.57f;
    cx[4] = x + w*0.66f; cx[5] = x + w*0.76f;
    for (int i = 0; i < 6; i++)
        tx_draw(c, COL[i], cx[i], y + 58.f, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
    ui_divider(x + 20.f, y + 74.f, w - 40.f);

    float ry = y + 84.f;
    float rowH = 38.f;
    float lh = h - (ry - y) - 14.f;
    float off = ui_scroll_begin(uid("resac"), x + 6.f, ry, w - 12.f, lh,
                                wo->nActypes*rowH + 8.f);
    for (int i = 0; i < wo->nActypes; i++) {
        AcType *t = &wo->actype[i];
        float yy = ry + i*rowH - off;
        if (yy + rowH < ry || yy > ry + lh) continue;

        int inUse = 0, canFit = 0;
        for (int k = 0; k < wo->nFlights; k++)
            if (wo->flight[k].acType == i && wo->flight[k].state != FS_CANCELLED)
                inUse++;
        for (int s = 0; s < wo->nStands; s++)
            if (wo->stand[s].maxSpan >= t->wingspan) canFit++;

        if (i & 1) cv_rrect(c, x + 16.f, yy, w - 32.f, rowH - 4.f, 7.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);

        tx_draw(c, t->code, cx[0], yy + rowH*0.5f - 2.f,
                font_track(TF_MONO, 12, TW_BOLD, 0),
                t->widebody ? C_MAGENTA : C_V600, AL_L, AV_M);
        tx_clipped(c, t->name, cx[1], yy + rowH*0.5f - 8.f, cx[2] - cx[1] - 12.f,
                   font_make(TF_UI, 12, TW_MED), C_INK, AL_L, AV_T);

        char v[24];
        snprintf(v, sizeof v, "%d", t->seats);
        tx_draw(c, v, cx[2], yy + rowH*0.5f - 2.f, font_make(TF_UI, 12, TW_MED),
                C_INK_2, AL_L, AV_M);
        snprintf(v, sizeof v, "%.0f m", t->wingspan);
        tx_draw(c, v, cx[3], yy + rowH*0.5f - 2.f, font_make(TF_UI, 12, TW_MED),
                C_INK_2, AL_L, AV_M);
        snprintf(v, sizeof v, "%d", t->uld);
        tx_draw(c, v, cx[4], yy + rowH*0.5f - 2.f, font_make(TF_UI, 12, TW_MED),
                C_INK_2, AL_L, AV_M);

        char u[70];
        snprintf(u, sizeof u, "%d movement%s   -   %d of %d stands fit",
                 inUse, inUse == 1 ? "" : "s", canFit, wo->nStands);
        tx_clipped(c, u, cx[5], yy + rowH*0.5f - 8.f, x + w - cx[5] - 24.f,
                   font_make(TF_UI, 11, TW_MED),
                   inUse ? C_INK_2 : C_INK_4, AL_L, AV_T);
    }
    ui_scroll_end();
}

/* ==========================================================================
 *  staff and shifts
 * ========================================================================== */

static const char *SHIFT_NAME[3] = { "Early  05:00-13:00",
                                     "Late   13:00-21:00",
                                     "Night  21:00-05:00" };

static void tab_staff(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "DUTY ROSTER", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    ui_text_field(uid("resstaff"), x + w - 274.f, y + 12.f, 254.f, 38.f,
                  a->resSearch, sizeof a->resSearch, "Name or role", IC_SEARCH);

    int onDuty = 0;
    for (int i = 0; i < wo->nStaff; i++) if (wo->staff[i].onDuty) onDuty++;
    char s2[90];
    snprintf(s2, sizeof s2, "%d of %d on duty now", onDuty, wo->nStaff);
    tx_draw(c, s2, x + 20.f, y + 34.f, font_make(TF_UI, 11, TW_REG),
            C_INK_3, AL_L, AV_T);

    float ry = y + 62.f;
    float rowH = 42.f;
    float lh = h - (ry - y) - 14.f;

    int idx[MAX_STAFF], n = 0;
    for (int i = 0; i < wo->nStaff; i++)
        if (ci_has(wo->staff[i].name, a->resSearch) ||
            ci_has(wo->staff[i].role, a->resSearch)) idx[n++] = i;

    float off = ui_scroll_begin(uid("resstafflist"), x + 6.f, ry, w - 12.f, lh,
                                n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Staff *s = &wo->staff[idx[i]];
        float yy = ry + i*rowH - off;
        if (yy + rowH < ry || yy > ry + lh) continue;

        if (i & 1) cv_rrect(c, x + 16.f, yy, w - 32.f, rowH - 4.f, 7.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);

        Color sc = s->onDuty ? C_OK : C_INK_4;
        cv_circle(c, x + 32.f, yy + rowH*0.5f - 2.f, 5.f, sc);

        tx_clipped(c, s->name, x + 46.f, yy + 7.f, w*0.28f,
                   font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
        tx_clipped(c, s->role, x + 46.f, yy + 23.f, w*0.28f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

        tx_clipped(c, SHIFT_NAME[s->shift % 3], x + w*0.42f, yy + rowH*0.5f - 8.f,
                   w*0.24f, font_track(TF_MONO, 11, TW_MED, 0),
                   s->onDuty ? C_INK_2 : C_INK_4, AL_L, AV_T);

        char st[40];
        snprintf(st, sizeof st, "Station %d", s->station);
        tx_draw(c, st, x + w*0.72f, yy + rowH*0.5f - 2.f,
                font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_M);

        ui_badge(x + w - 92.f, yy + rowH*0.5f - 10.f,
                 s->onDuty ? "ON DUTY" : "OFF", HEX(0xFFFFFF),
                 s->onDuty ? C_OK : C_INK_4);
    }
    ui_scroll_end();
}

static void tab_shifts(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "SHIFT COVERAGE", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "staff on each shift against the movements they have to cover",
            x + 20.f, y + 33.f, font_make(TF_UI, 11, TW_REG), C_INK_3,
            AL_L, AV_T);

    /* movements inside each shift window */
    int mv[3] = { 0, 0, 0 }, st[3] = { 0, 0, 0 };
    for (int i = 0; i < wo->nFlights; i++) {
        if (wo->flight[i].state == FS_CANCELLED) continue;
        int m = wo->flight[i].estMin;
        int s = (m >= 300 && m < 780) ? 0 : (m >= 780 && m < 1260) ? 1 : 2;
        mv[s]++;
    }
    for (int i = 0; i < wo->nStaff; i++) st[wo->staff[i].shift % 3]++;

    float ry = y + 62.f;
    float cardH = (h - (ry - y) - 20.f) / 3.f;
    for (int s = 0; s < 3; s++) {
        float yy = ry + s*cardH;
        int nowShift = ((int)wo->clock >= 300 && (int)wo->clock < 780) ? 0
                     : ((int)wo->clock >= 780 && (int)wo->clock < 1260) ? 1 : 2;
        int isNow = (s == nowShift);

        cv_rrect(c, x + 18.f, yy, w - 36.f, cardH - 12.f, 10.f,
                 isNow ? col_alpha(C_V500, .09f) : C_SURF_2);
        if (isNow)
            cv_rrect_line(c, x + 18.f, yy, w - 36.f, cardH - 12.f, 10.f,
                          col_alpha(C_V500, .45f), 1.4f);
        tx_backdrop(isNow ? C_SURF : C_SURF_2);

        tx_draw(c, SHIFT_NAME[s], x + 34.f, yy + 14.f,
                font_track(TF_MONO, 13, TW_BOLD, 0),
                isNow ? C_V700 : C_INK, AL_L, AV_T);
        if (isNow)
            ui_badge(x + 34.f + 190.f, yy + 14.f, "ON NOW", HEX(0xFFFFFF),
                     C_V600);

        char t[110];
        snprintf(t, sizeof t, "%d staff rostered   -   %d movements to cover",
                 st[s], mv[s]);
        tx_draw(c, t, x + 34.f, yy + 38.f, font_make(TF_UI, 12, TW_MED),
                C_INK_2, AL_L, AV_T);

        /*  Movements per person is the number that matters: a shift with
         *  plenty of staff and no aeroplanes is not the same problem as one
         *  with the ratio the other way round.                            */
        float ratio = st[s] ? (float)mv[s] / (float)st[s] : 0.f;
        Color rc = ratio > 3.5f ? C_DANGER : (ratio > 2.5f ? C_WARN : C_OK);
        snprintf(t, sizeof t, "%.1f movements per person", ratio);
        tx_draw(c, t, x + w - 34.f, yy + 38.f, font_make(TF_UI, 12, TW_SEMI),
                rc, AL_R, AV_T);

        float bw = w - 68.f;
        ui_meter(x + 34.f, yy + 60.f, bw, 8.f,
                 cv_clampf(ratio / 4.f, 0.f, 1.f), col_alpha(rc, .5f), rc);

        if (cardH > 100.f) {
            const char *note = ratio > 3.5f
                ? "Under-staffed for this workload -- call in additional cover."
                : (ratio > 2.5f
                   ? "Tight. Watch the check-in queue at the start of the bank."
                   : "Comfortable cover for the movements on this shift.");
            tx_draw(c, note, x + 34.f, yy + 76.f, font_make(TF_UI, 11, TW_REG),
                    C_INK_3, AL_L, AV_T);
        }
    }
}

/* ==========================================================================
 *  special assistance
 * ========================================================================== */

static void tab_assist(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "SPECIAL ASSISTANCE", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    int idx[MAX_PASSENGERS], n = 0;
    for (int i = 0; i < wo->nPax; i++)
        if (wo->pax[i].assist != AS_NONE || wo->pax[i].infant) idx[n++] = i;

    char s2[110];
    snprintf(s2, sizeof s2, "%d passengers have asked for help today  -  "
                            "each one needs a person allocated", n);
    tx_draw(c, s2, x + 20.f, y + 33.f, font_make(TF_UI, 11, TW_REG),
            C_INK_3, AL_L, AV_T);

    /* the tally by code */
    float ry = y + 58.f;
    float cw = (w - 40.f - 5.f*8.f) / 6.f;
    for (int k = 1; k < AS_COUNT; k++) {
        int cnt = 0;
        for (int i = 0; i < wo->nPax; i++) if (wo->pax[i].assist == k) cnt++;
        float bx = x + 20.f + (float)(k-1)*(cw + 8.f);
        cv_rrect(c, bx, ry, cw, 56.f, 9.f,
                 col_alpha(cnt ? C_V500 : C_INK_4, .09f));
        tx_backdrop(C_SURF);
        tx_draw(c, as_code((SpecialAssist)k), bx + 10.f, ry + 8.f,
                font_track(TF_MONO, 11, TW_BOLD, 1),
                cnt ? C_V600 : C_INK_4, AL_L, AV_T);
        char v[12]; snprintf(v, sizeof v, "%d", cnt);
        tx_draw(c, v, bx + cw - 10.f, ry + 6.f,
                font_track(TF_DISPLAY, 18, TW_BOLD, 0),
                cnt ? C_INK : C_INK_4, AL_R, AV_T);
        tx_clipped(c, as_name((SpecialAssist)k), bx + 10.f, ry + 32.f, cw - 20.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);
    }
    ry += 70.f;

    float rowH = 44.f;
    float lh = h - (ry - y) - 14.f;
    float off = ui_scroll_begin(uid("resassist"), x + 6.f, ry, w - 12.f, lh,
                                n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Passenger *p = &wo->pax[idx[i]];
        Flight *f = flight_by_id(wo, p->flight);
        float yy = ry + i*rowH - off;
        if (yy + rowH < ry || yy > ry + lh) continue;

        if (i & 1) cv_rrect(c, x + 16.f, yy, w - 32.f, rowH - 4.f, 7.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);

        int code = p->assist ? p->assist : AS_NONE;
        Color pc = (code == AS_MEDICAL || code == AS_MINOR) ? C_MAGENTA : C_V600;
        cv_rrect(c, x + 24.f, yy + 10.f, 44.f, 20.f, 5.f, col_alpha(pc, .14f));
        tx_draw(c, p->infant && !p->assist ? "INFT"
                                           : as_code((SpecialAssist)code),
                x + 46.f, yy + 20.f, font_track(TF_MONO, 10, TW_BOLD, 1),
                pc, AL_C, AV_M);

        tx_clipped(c, p->name, x + 78.f, yy + 7.f, w*0.30f,
                   font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
        char sub[80];
        snprintf(sub, sizeof sub, "%s  seat %s  %s", p->pnr, p->seat,
                 p->infant ? "travelling with an infant" : "");
        tx_clipped(c, sub, x + 78.f, yy + 24.f, w*0.30f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

        if (f) {
            char fl[70];
            char hm[8]; fmt_hhmm(f->estMin, hm);
            snprintf(fl, sizeof fl, "%s  %s  gate %d  %s", f->no, hm, f->gate,
                     wo->airport[f->airport].city);
            tx_clipped(c, fl, x + w*0.46f, yy + rowH*0.5f - 7.f, w*0.34f,
                       font_make(TF_UI, 11, TW_MED), C_INK_2, AL_L, AV_T);
        }

        /*  How long until they need to be at the gate.  Assistance has to be
         *  in place before boarding starts, not when it does.             */
        if (f) {
            float mins = (float)(f->estMin - 60) - wo->clock;
            char due[40];
            Color dc = C_INK_3;
            if (mins < 0.f)      { snprintf(due, sizeof due, "due now"); dc = C_DANGER; }
            else if (mins < 30.f){ snprintf(due, sizeof due, "in %.0f min", mins); dc = C_WARN; }
            else                 snprintf(due, sizeof due, "in %.0f min", mins);
            tx_draw(c, due, x + w - 24.f, yy + rowH*0.5f - 2.f,
                    font_make(TF_UI, 11, TW_SEMI), dc, AL_R, AV_M);
        }
    }
    ui_scroll_end();

    if (!n) {
        tx_backdrop(C_SURF);
        tx_draw(c, "Nobody has requested assistance today", x + w*0.5f,
                ry + 60.f, font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    }
}

/* ==========================================================================
 *  lost property
 * ========================================================================== */

static void tab_lost(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    LostBook *L = &a->lost;

    float formW = 320.f;
    float listW = w - formW - 16.f;

    /* ---- the register ---------------------------------------------------- */
    ui_card(x, y, listW, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "LOST AND FOUND", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char s2[90];
    snprintf(s2, sizeof s2, "%d open case%s  -  %d returned to their owner",
             lp_open(L), lp_open(L) == 1 ? "" : "s", L->returned);
    tx_draw(c, s2, x + 20.f, y + 33.f, font_make(TF_UI, 11, TW_REG),
            C_INK_3, AL_L, AV_T);

    ui_text_field(uid("reslost"), x + listW - 274.f, y + 12.f, 254.f, 38.f,
                  a->resSearch, sizeof a->resSearch,
                  "Reference, item or owner", IC_SEARCH);

    int idx[MAX_LOST];
    int n = lp_find(L, a->resSearch, idx, MAX_LOST);

    float ry = y + 62.f;
    float rowH = 62.f;
    float lh = h - (ry - y) - 14.f;
    float off = ui_scroll_begin(uid("reslostlist"), x + 6.f, ry, listW - 12.f,
                                lh, n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        LostItem *it = &L->it[idx[i]];
        float yy = ry + i*rowH - off;
        if (yy + rowH < ry || yy > ry + lh) continue;

        Color sc = it->state == LP_RETURNED ? C_OK
                 : (it->state == LP_MATCHED ? C_TEAL
                 : (it->state == LP_FOUND   ? C_V600 : C_WARN));

        if (i & 1) cv_rrect(c, x + 16.f, yy, listW - 32.f, rowH - 6.f, 8.f,
                            C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);
        cv_rrect(c, x + 20.f, yy + 8.f, 3.f, rowH - 22.f, 1.5f, sc);

        tx_draw(c, it->ref, x + 32.f, yy + 8.f,
                font_track(TF_MONO, 11, TW_BOLD, 0), C_INK_3, AL_L, AV_T);
        tx_clipped(c, it->what, x + 96.f, yy + 8.f, listW*0.44f,
                   font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
        char wh[90];
        snprintf(wh, sizeof wh, "%s   -   %s%s", it->where,
                 lp_state_name(it->state),
                 it->owner[0] ? "" : "");
        tx_clipped(c, wh, x + 96.f, yy + 26.f, listW*0.44f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);
        if (it->owner[0])
            tx_clipped(c, it->owner, x + 96.f, yy + 41.f, listW*0.44f,
                       font_make(TF_UI, 10, TW_MED), C_INK_2, AL_L, AV_T);

        ui_badge(x + listW - 224.f, yy + 10.f, lp_state_name(it->state),
                 HEX(0xFFFFFF), sc);

        if (it->state == LP_FOUND || it->state == LP_REPORTED) {
            if (ui_button(uidi("lpm", it->id), x + listW - 224.f, yy + 32.f,
                          92.f, 26.f, "Match", BTN_SOFT))
                lp_match(L, wo, it->id);
        }
        if (it->state != LP_RETURNED) {
            if (ui_button(uidi("lpr", it->id), x + listW - 124.f, yy + 32.f,
                          100.f, 26.f, "Returned", BTN_SUCCESS))
                lp_return(L, wo, it->id);
        }
    }
    ui_scroll_end();

    if (!n) {
        tx_backdrop(C_SURF);
        tx_draw(c, "Nothing matches", x + listW*0.5f, ry + 50.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    }

    /* ---- report something ------------------------------------------------ */
    float fx = x + listW + 16.f;
    ui_card(fx, y, formW, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "REPORT AN ITEM", fx + 18.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float ry2 = y + 44.f;
    tx_draw(c, "WHAT IS IT", fx + 18.f, ry2, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry2 += 17.f;
    ui_text_field(uid("lpwhat"), fx + 18.f, ry2, formW - 36.f, 42.f, a->lpWhat,
                  (int)sizeof a->lpWhat, "Black rucksack, red zip", IC_BOX);
    ry2 += 52.f;

    tx_draw(c, "WHERE", fx + 18.f, ry2, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry2 += 17.f;
    ui_text_field(uid("lpwhere"), fx + 18.f, ry2, formW - 36.f, 42.f, a->lpWhere,
                  (int)sizeof a->lpWhere, "Gate 6 seating", IC_PIN);
    ry2 += 56.f;

    int ok = strlen(a->lpWhat) >= 4;
    if (!ok) {
        tx_draw(c, "Describe the item in a few words", fx + 18.f, ry2 + 10.f,
                font_make(TF_UI, 11, TW_MED), C_INK_4, AL_L, AV_T);
    } else {
        if (ui_button_i(uid("lphand"), fx + 18.f, ry2, formW - 36.f, 40.f,
                        "Handed in at the desk", IC_PLUS, BTN_PRIMARY)) {
            lp_found(L, wo, a->lpWhat, a->lpWhere[0] ? a->lpWhere : "Lost property desk");
            a->lpWhat[0] = 0; a->lpWhere[0] = 0;
            ui_focus_clear();
            ui_toast(TOAST_OK, "Logged", "The item is in the register.");
        }
        if (ui_button_i(uid("lpmiss"), fx + 18.f, ry2 + 48.f, formW - 36.f, 40.f,
                        "A passenger has lost it", IC_ALERT, BTN_OUTLINE)) {
            lp_report(L, wo, a->lpWhat,
                      a->lpWhere[0] ? a->lpWhere : "Unknown",
                      a->auth.name[0] ? a->auth.name : "Walk-in",
                      a->auth.email, 0);
            a->lpWhat[0] = 0; a->lpWhere[0] = 0;
            ui_focus_clear();
            ui_toast(TOAST_WARN, "Reported", "The claim is open at the desk.");
        }
    }
    ry2 += 100.f;

    ui_divider(fx + 18.f, ry2, formW - 36.f);
    ry2 += 14.f;
    tx_draw(c, "MISSING CHECKED BAGS", fx + 18.f, ry2,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    ry2 += 20.f;

    int mis = 0;
    for (int i = 0; i < wo->nBags && ry2 + 30.f < y + h; i++) {
        if (wo->bag[i].state != BG_MISHANDLED) continue;
        mis++;
        Bag *b = &wo->bag[i];
        tx_draw(c, b->tag, fx + 18.f, ry2, font_track(TF_MONO, 11, TW_SEMI, 0),
                C_INK, AL_L, AV_T);
        char wt[24]; snprintf(wt, sizeof wt, "%.1f kg", b->weight);
        tx_draw(c, wt, fx + formW - 100.f, ry2, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_R, AV_T);
        if (ui_button(uidi("lpbag", i), fx + formW - 92.f, ry2 - 6.f, 74.f, 26.f,
                      "Claim", BTN_SOFT)) {
            if (lp_report_bag(L, wo, i, a->auth.email))
                ui_toast(TOAST_WARN, "Claim opened",
                         "The bag has been added to lost property.");
            else
                ui_toast(TOAST_INFO, "Already open",
                         "There is a claim on that bag already.");
        }
        ry2 += 28.f;
    }
    if (!mis)
        tx_draw(c, "No bags are missing right now", fx + 18.f, ry2,
                font_make(TF_UI, 11, TW_REG), C_INK_4, AL_L, AV_T);
}

/* ==========================================================================
 *  the screen
 * ========================================================================== */

void screen_resources(App *a, float x, float y, float w, float h)
{
    float pad = PAD;

    static const struct { const char *lab; int icon; } TAB[RST_COUNT] = {
        { "Aircraft",   IC_PLANE   },
        { "Staff",      IC_USERS   },
        { "Shifts",     IC_CLOCK   },
        { "Assistance", IC_HEART   },
        { "Lost & found", IC_BOX   },
    };

    float cy = y + pad;
    float tw = 146.f;
    for (int i = 0; i < RST_COUNT; i++)
        if (ui_tab(uidi("restab", i), x + pad + i*(tw + 6.f), cy, tw, 40.f,
                   TAB[i].lab, TAB[i].icon, a->resTab == i)) {
            a->resTab = i;
            a->resSearch[0] = 0;         /* a search does not carry across  */
        }

    float bx = x + pad, by = cy + 56.f;
    float bw = w - pad*2.f, bh = h - (by - y) - pad;

    switch (a->resTab) {
    case RST_STAFF:  tab_staff   (a, bx, by, bw, bh); break;
    case RST_SHIFTS: tab_shifts  (a, bx, by, bw, bh); break;
    case RST_ASSIST: tab_assist  (a, bx, by, bw, bh); break;
    case RST_LOST:   tab_lost    (a, bx, by, bw, bh); break;
    default:         tab_aircraft(a, bx, by, bw, bh); break;
    }
}
