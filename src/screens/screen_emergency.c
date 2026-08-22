/* ==========================================================================
 *  AURA :: screen_emergency.c   --   incidents and announcements
 *
 *  When something goes wrong an airport has to do two things at once: get the
 *  right team to the right place, and tell the passengers.  Both are on this
 *  screen because both are the same job done badly if they are separated.
 *
 *  Every incident carries a severity that decides where it sits in the list,
 *  what the recommended response is, and how long it may go unacknowledged
 *  before the target is breached.  The clock on an unacknowledged critical
 *  incident runs in red from the moment it is raised.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static Color level_colour(EmgLevel l)
{
    switch (l) {
    case EMG_CRITICAL: return C_DANGER;
    case EMG_MAJOR:    return C_WARN;
    case EMG_MINOR:    return C_INFO;
    default:           return C_INK_3;
    }
}

static int kind_icon(EmgKind k)
{
    switch (k) {
    case EMG_FIRE:     return IC_BOLT;
    case EMG_MEDICAL:  return IC_HEART;
    case EMG_SECURITY: return IC_SHIELD;
    case EMG_BAGGAGE:  return IC_LUGGAGE;
    case EMG_AIRCRAFT: return IC_PLANE;
    default:           return IC_WIND;
    }
}

/* --------------------------------------------------------------------------
 *  the incident list
 * ------------------------------------------------------------------------- */

static void panel_incidents(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    EmergencyLog *L = &a->emg;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);

    int active = emg_active(L);
    int worst  = emg_worst(L);
    Color hc = active ? level_colour((EmgLevel)(worst < 0 ? EMG_ADVISORY : worst))
                      : C_OK;

    float pulse = 0.5f + 0.5f*sinf(anim_time()*3.2f);
    cv_circle(c, x + 28.f, y + 26.f, 14.f,
              col_alpha(hc, active ? .14f + .12f*pulse : .12f));
    icon_draw(c, active ? IC_ALERT : IC_SHIELD, x + 28.f, y + 26.f, 16.f, hc);

    char t[70];
    snprintf(t, sizeof t, active ? "%d active incident%s" : "All clear",
             active, active == 1 ? "" : "s");
    tx_draw(c, t, x + 50.f, y + 14.f, font_track(TF_DISPLAY, 19, TW_BOLD, 0),
            C_INK, AL_L, AV_T);
    char s2[110];
    snprintf(s2, sizeof s2, "%d raised today  -  %d resolved  -  average "
                            "response %.1f min",
             L->totalRaised, L->totalResolved, emg_avg_response(L));
    tx_clipped(c, s2, x + 50.f, y + 38.f, w - 70.f,
               font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);

    /*  Ordered by severity first and age second, which is triage order.  A
     *  critical incident raised a minute ago outranks a minor one raised an
     *  hour ago, and the list must say so without anybody having to read it
     *  carefully.                                                          */
    int order[MAX_EMERGENCY];
    int n = 0;
    for (int i = 0; i < L->n; i++) order[n++] = i;
    for (int i = 1; i < n; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0) {
            Emergency *A = &L->e[order[j]], *B = &L->e[k];
            int ar = (A->state == EMS_RESOLVED) * 100 + (int)A->level;
            int br = (B->state == EMS_RESOLVED) * 100 + (int)B->level;
            if (ar > br || (ar == br && A->raisedMin < B->raisedMin)) {
                order[j+1] = order[j]; j--;
            } else break;
        }
        order[j+1] = k;
    }

    float ly = y + 62.f, lh = h - 74.f;
    float rowH = 104.f;
    float off = ui_scroll_begin(uid("emglist"), x + 6.f, ly, w - 12.f, lh,
                                n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Emergency *e = &L->e[order[i]];
        float ry = ly + i*rowH - off;
        if (ry + rowH < ly || ry > ly + lh) continue;

        int done = (e->state == EMS_RESOLVED);
        Color lc = level_colour(e->level);
        int sel  = (a->emgSel == e->id);

        int hov = ui_hit(x + 12.f, ry, w - 24.f, rowH - 8.f);
        if (hov) { ui_cursor(1); if (a->in.pressed) a->emgSel = e->id; }

        Color bg = done ? C_SURF_2
                 : col_alpha(lc, sel ? .14f : (hov ? .10f : .06f));
        cv_rrect(c, x + 12.f, ry, w - 24.f, rowH - 8.f, 10.f, bg);
        cv_rrect(c, x + 12.f, ry, 3.f, rowH - 8.f, 1.5f,
                 done ? C_INK_4 : lc);

        tx_backdrop(bg);
        icon_draw(c, kind_icon(e->kind), x + 34.f, ry + 20.f, 16.f,
                  done ? C_INK_4 : lc);
        tx_draw(c, emg_kind_name(e->kind), x + 50.f, ry + 11.f,
                font_make(TF_UI, 13, TW_SEMI), done ? C_INK_3 : C_INK,
                AL_L, AV_T);

        ui_badge(x + w - 108.f, ry + 10.f, emg_level_name(e->level),
                 HEX(0xFFFFFF), done ? C_INK_4 : lc);

        tx_clipped(c, e->where, x + 50.f, ry + 30.f, w - 170.f,
                   font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);
        tx_para(c, e->detail, x + 34.f, ry + 46.f, w - 60.f,
                font_make(TF_UI, 11, TW_REG), done ? C_INK_4 : C_INK_2, 2);

        /* the response clock */
        char clk[70];
        if (done) {
            float took = e->clearedMin - e->raisedMin;
            if (took < 0.f) took += 1440.f;
            snprintf(clk, sizeof clk, "%s  -  cleared in %.0f min",
                     emg_state_name(e->state), took);
            tx_draw(c, clk, x + 34.f, ry + rowH - 26.f,
                    font_make(TF_UI, 10, TW_MED), C_OK, AL_L, AV_T);
        } else {
            float age = wo->clock - e->raisedMin;
            if (age < 0.f) age += 1440.f;
            int target = emg_target_minutes(e->level);
            int late   = (e->state == EMS_RAISED && age > (float)target);
            snprintf(clk, sizeof clk, "%s  -  %.0f min ago  (target %d min)",
                     emg_state_name(e->state), age, target);
            tx_draw(c, clk, x + 34.f, ry + rowH - 26.f,
                    font_make(TF_UI, 10, late ? TW_BOLD : TW_MED),
                    late ? C_DANGER : C_INK_3, AL_L, AV_T);
        }

        /* the one action that makes sense next */
        float bw = 96.f, bx = x + w - bw - 20.f;
        if (e->state == EMS_RAISED) {
            if (ui_button(uidi("emgack", e->id), bx, ry + rowH - 42.f, bw, 30.f,
                          "Acknowledge", BTN_PRIMARY))
                emg_ack(L, wo, e->id);
        } else if (e->state == EMS_ACKNOWLEDGED) {
            if (ui_button(uidi("emgrsp", e->id), bx, ry + rowH - 42.f, bw, 30.f,
                          "On scene", BTN_OUTLINE))
                emg_respond(L, wo, e->id);
        } else if (e->state == EMS_RESPONDING) {
            if (ui_button(uidi("emgres", e->id), bx, ry + rowH - 42.f, bw, 30.f,
                          "Stand down", BTN_SUCCESS))
                emg_resolve(L, wo, e->id);
        }
    }
    ui_scroll_end();

    if (!n) {
        icon_draw(c, IC_SHIELD, x + w*0.5f, y + h*0.5f - 12.f, 34.f,
                  col_alpha(C_OK, .45f));
        tx_backdrop(C_SURF);
        tx_draw(c, "No incidents have been raised today", x + w*0.5f,
                y + h*0.5f + 22.f, font_make(TF_UI, 12, TW_MED), C_INK_3,
                AL_C, AV_M);
    }
}

/* --------------------------------------------------------------------------
 *  raise one, and the protocol for the selected incident
 * ------------------------------------------------------------------------- */

static void panel_raise(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "RAISE AN INCIDENT", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float ry = y + 38.f;
    float cw = (w - 36.f - 12.f) / 3.f;
    for (int k = 0; k < EMG_KIND_COUNT; k++) {
        float bx = x + 18.f + (k % 3)*(cw + 6.f);
        float by = ry + (k / 3)*44.f;
        int on = (a->emgNewKind == k);
        Color lc = level_colour(emg_default_level((EmgKind)k));
        if (ui_hit(bx, by, cw, 38.f)) {
            ui_cursor(1);
            if (a->in.pressed) a->emgNewKind = k;
        }
        cv_rrect(c, bx, by, cw, 38.f, 9.f,
                 on ? col_alpha(lc, .16f) : C_SURF_2);
        if (on) cv_rrect_line(c, bx, by, cw, 38.f, 9.f, col_alpha(lc, .6f), 1.4f);
        icon_draw(c, kind_icon((EmgKind)k), bx + 18.f, by + 19.f, 15.f,
                  on ? lc : C_INK_3);
        tx_backdrop(on ? C_SURF : C_SURF_2);
        tx_clipped(c, emg_kind_name((EmgKind)k), bx + 32.f, by + 12.f,
                   cw - 40.f, font_make(TF_UI, 11, on ? TW_SEMI : TW_MED),
                   on ? C_INK : C_INK_2, AL_L, AV_T);
    }
    ry += 96.f;

    EmgLevel lv = emg_default_level((EmgKind)a->emgNewKind);
    char lvt[110];
    snprintf(lvt, sizeof lvt, "%s  -  acknowledge within %d min",
             emg_level_name(lv), emg_target_minutes(lv));
    ui_pill(x + 18.f, ry, 24.f, lvt, HEX(0xFFFFFF), level_colour(lv));
    ry += 36.f;

    if (ui_button_i(uid("emgraise"), x + 18.f, ry, w - 36.f, 42.f,
                    "Raise this incident", IC_ALERT, BTN_DANGER)) {
        static const char *WHERE[EMG_KIND_COUNT] = {
            "Terminal, Level 1", "Departures gate area", "Security lane 3",
            "Check-in island B", "Stand A4", "Apron"
        };
        static const char *WHAT[EMG_KIND_COUNT] = {
            "Reported by a member of staff, zone alarm active",
            "Passenger requires immediate medical attention",
            "Reported breach, area not yet secured",
            "Item left unattended, owner not located",
            "Crew have declared a problem on the ground",
            "Conditions have made apron work unsafe"
        };
        int id = emg_raise(&a->emg, wo, (EmgKind)a->emgNewKind, lv,
                           WHERE[a->emgNewKind], WHAT[a->emgNewKind], 0);
        a->emgSel = id;
        char m[120];
        snprintf(m, sizeof m, "%s at %s", emg_kind_name((EmgKind)a->emgNewKind),
                 WHERE[a->emgNewKind]);
        nt_push(&a->notify, wo, NT_EMERGENCY, 0, 0, m);
        ui_toast(TOAST_ERR, "Incident raised", m);
    }
    ry += 50.f;

    if (ui_button_i(uid("emgdrill"), x + 18.f, ry, w - 36.f, 34.f,
                    "Run a random drill", IC_REFRESH, BTN_SOFT)) {
        int id = emg_raise_random(&a->emg, wo);
        a->emgSel = id;
        ui_toast(TOAST_WARN, "Drill", "An incident has been injected.");
    }
    ry += 48.f;

    /* ---- the protocol for whichever incident is selected ---------------- */
    ui_divider(x + 18.f, ry, w - 36.f);
    ry += 14.f;

    Emergency *sel = NULL;
    for (int i = 0; i < a->emg.n; i++)
        if (a->emg.e[i].id == a->emgSel) { sel = &a->emg.e[i]; break; }
    if (!sel && a->emg.n) sel = &a->emg.e[a->emg.n - 1];

    if (!sel) {
        tx_draw(c, "Select an incident to see the response protocol",
                x + 18.f, ry, font_make(TF_UI, 11, TW_REG), C_INK_4,
                AL_L, AV_T);
        return;
    }

    tx_draw(c, "RESPONSE PROTOCOL", x + 18.f, ry,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    ry += 18.f;
    char hd[80];
    snprintf(hd, sizeof hd, "%s  -  %s", emg_kind_name(sel->kind), sel->where);
    tx_clipped(c, hd, x + 18.f, ry, w - 36.f, font_make(TF_UI, 12, TW_SEMI),
               C_INK, AL_L, AV_T);
    ry += 22.f;
    tx_para(c, emg_protocol(sel->kind), x + 18.f, ry, w - 36.f,
            font_make(TF_UI, 11, TW_REG), C_INK_2, 6);
    ry += 84.f;

    if (sel->responder[0] && ry + 20.f < y + h) {
        char rp[80];
        snprintf(rp, sizeof rp, "Assigned to %s", sel->responder);
        tx_clipped(c, rp, x + 18.f, ry, w - 36.f, font_make(TF_UI, 11, TW_SEMI),
                   C_V600, AL_L, AV_T);
        ry += 18.f;
    }
    if (sel->evacuate && ry + 20.f < y + h)
        tx_draw(c, "Area evacuated", x + 18.f, ry,
                font_make(TF_UI, 11, TW_BOLD), C_DANGER, AL_L, AV_T);
}

/* --------------------------------------------------------------------------
 *  what the passengers have been told
 * ------------------------------------------------------------------------- */

static void panel_notifications(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    NotifyCentre *N = &a->notify;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "PASSENGER NOTIFICATIONS", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char s2[90];
    snprintf(s2, sizeof s2, "%d sent, raised automatically by the system",
             N->sent);
    tx_clipped(c, s2, x + 18.f, y + 32.f, w - 36.f,
               font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);

    /*  One announcement goes to every passenger on the flight, so the raw
     *  list is the same sentence repeated a dozen times.  That is right for
     *  the passenger's own notification list and useless here, so identical
     *  messages are collapsed into one row carrying the recipient count. */
    int uniq[MAX_NOTIFY], count[MAX_NOTIFY];
    int show = 0;
    for (int i = N->count - 1; i >= 0 && show < MAX_NOTIFY; i--) {
        Notification *m = &N->n[i];
        int dup = -1;
        for (int k = 0; k < show; k++) {
            Notification *o = &N->n[uniq[k]];
            if (o->kind == m->kind && o->flight == m->flight &&
                strcmp(o->text, m->text) == 0) { dup = k; break; }
        }
        if (dup >= 0) { count[dup]++; continue; }
        uniq[show] = i;
        count[show] = 1;
        show++;
    }

    float ly = y + 54.f, lh = h - 66.f;
    float rowH = 56.f;
    float off = ui_scroll_begin(uid("ntlist"), x + 6.f, ly, w - 12.f, lh,
                                show*rowH + 8.f);
    for (int i = 0; i < show; i++) {
        Notification *m = &N->n[uniq[i]];
        float ry = ly + i*rowH - off;
        if (ry + rowH < ly || ry > ly + lh) continue;

        Color kc;
        switch (m->kind) {
        case NT_GATE:      kc = C_MAGENTA; break;
        case NT_DELAY:     kc = C_WARN;    break;
        case NT_BOARDING:  kc = C_OK;      break;
        case NT_FINAL:     kc = C_WARN;    break;
        case NT_CANCELLED: kc = C_DANGER;  break;
        case NT_EMERGENCY: kc = C_DANGER;  break;
        default:           kc = C_V600;    break;
        }

        if (i & 1) cv_rrect(c, x + 12.f, ry, w - 24.f, rowH - 6.f, 8.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);

        cv_circle(c, x + 26.f, ry + 16.f, 4.f, kc);
        tx_draw(c, nt_kind_name(m->kind), x + 38.f, ry + 9.f,
                font_track(TF_UI, 9, TW_BOLD, 1), kc, AL_L, AV_T);

        char hm[8]; fmt_hhmm((int)m->atMin, hm);
        tx_draw(c, hm, x + w - 20.f, ry + 9.f,
                font_track(TF_MONO, 10, TW_MED, 0), C_INK_4, AL_R, AV_T);
        if (count[i] > 1) {
            char n2[24];
            snprintf(n2, sizeof n2, "to %d", count[i]);
            tx_draw(c, n2, x + w - 58.f, ry + 9.f,
                    font_make(TF_UI, 10, TW_SEMI), C_V600, AL_R, AV_T);
        }

        tx_para(c, m->text, x + 26.f, ry + 24.f, w - 46.f,
                font_make(TF_UI, 11, TW_MED), C_INK_2, 2);
    }
    ui_scroll_end();

    if (!show) {
        tx_backdrop(C_SURF);
        tx_draw(c, "Nothing to tell anybody yet", x + w*0.5f, y + h*0.5f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
        tx_draw(c, "Change a gate or delay a flight on Operations",
                x + w*0.5f, y + h*0.5f + 20.f, font_make(TF_UI, 11, TW_REG),
                C_INK_4, AL_C, AV_M);
    }
    (void)wo;
}

/* ==========================================================================
 *  the screen
 * ========================================================================== */

void screen_emergency(App *a, float x, float y, float w, float h)
{
    float pad = PAD;
    float bx = x + pad, by = y + pad;
    float bw = w - pad*2.f, bh = h - pad*2.f;

    float leftW  = bw*0.44f;
    float midW   = bw*0.30f;
    float rightW = bw - leftW - midW - 32.f;

    panel_incidents    (a, bx, by, leftW, bh);
    panel_raise        (a, bx + leftW + 16.f, by, midW, bh);
    panel_notifications(a, bx + leftW + midW + 32.f, by, rightW, bh);
}
