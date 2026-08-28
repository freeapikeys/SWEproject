/* ==========================================================================
 *  AURA :: screen_welcome.c   --   the passenger desk
 *
 *  The screen the application opens on.  A passenger types their name and is
 *  shown their own journey: which flight, from which gate, how their bags are
 *  progressing through the hold baggage system, and what happens next on a
 *  timeline anchored to the live clock.
 *
 *  Matching is incremental -- results narrow on every keystroke, with no
 *  search button -- and works on a given name, a surname or a booking
 *  reference.  Names come from the generated Mauritian roster, or from
 *  data/roster.csv when a real list has been placed there locally.
 *
 *  The lower panel answers the questions the airport is most often asked,
 *  taken from its published passenger information.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 *  case-insensitive substring, so "ram" finds "Ramgoolam"
 * ------------------------------------------------------------------------- */

static int ci_contains(const char *hay, const char *needle)
{
    if (!needle || !*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *a = h, *b = needle;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) break;
            a++; b++;
        }
        if (!*b) return 1;
    }
    return 0;
}

/* a match at the start of a name ranks above one buried mid-string */
static int match_rank(const Passenger *p, const char *q)
{
    if (!*q) return 1;
    char first[40];
    snprintf(first, sizeof first, "%s", p->name);
    char *sp = strchr(first, ' ');
    char surname[40];
    snprintf(surname, sizeof surname, "%s", sp ? sp + 1 : first);
    if (sp) *sp = 0;

    if (ci_contains(first, q))   return 4;      /* given name           */
    if (ci_contains(p->pnr, q))  return 4;      /* booking reference    */
    if (ci_contains(surname, q)) return 3;      /* surname              */
    if (ci_contains(p->name, q)) return 2;      /* anywhere in the name */
    return 0;
}

/* ==========================================================================
 *  the passenger's own journey
 * ========================================================================== */

static void draw_journey(App *a, float x, float y, float w, float h,
                         Passenger *p)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    Flight *f = flight_by_id(wo, p->flight);
    if (!f) return;

    ui_card(x, y, w, h, R_LG);
    Color ac = wo->airline[f->airline].col;
    Paint g = paint_linear(x, y, x + w, y, col_alpha(ac, .16f), col_alpha(ac, .02f));
    cv_rrect_p(c, x, y, w, 92.f, R_LG, &g);
    cv_rrect(c, x, y, 4.f, 92.f, 2.f, ac);

    tx_backdrop(C_SURF);
    tx_clipped(c, p->name, x + 22.f, y + 14.f, w - 190.f,
               font_track(TF_DISPLAY, 25, TW_BOLD, 0), C_INK, AL_L, AV_T);
    char sub[110];
    snprintf(sub, sizeof sub, "%s   %s to %s", f->no,
             wo->airline[f->airline].name, wo->airport[f->airport].city);
    tx_clipped(c, sub, x + 22.f, y + 48.f, w - 190.f,
               font_make(TF_UI, 13, TW_MED), C_INK_2, AL_L, AV_T);

    char est[8], sch[8];
    fmt_hhmm(f->estMin, est);
    fmt_hhmm(f->schedMin, sch);
    tx_draw(c, est, x + w - 22.f, y + 14.f,
            font_track(TF_DISPLAY, 27, TW_BOLD, 0),
            f->delayMin >= 15 ? C_DANGER : C_INK, AL_R, AV_T);
    char when[48];
    if (f->delayMin >= 15) snprintf(when, sizeof when, "was %s, delayed", sch);
    else                   snprintf(when, sizeof when, "on schedule");
    tx_draw(c, when, x + w - 22.f, y + 50.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_R, AV_T);

    /* ---- the four things a passenger actually needs --------------------- */
    float ty = y + 106.f;
    float tw = (w - 44.f - 24.f) / 4.f;
    char gateBuf[8]; ordinal_gate(f->gate, gateBuf);
    char bagsTxt[12];
    snprintf(bagsTxt, sizeof bagsTxt, "%d", p->bags);

    struct { const char *k; const char *v; Color col; } K[4] = {
        { "SEAT",    p->seat, C_V600    },
        { "GATE",    gateBuf, C_MAGENTA },
        { "BOOKING", p->pnr,  C_TEAL    },
        { "BAGS",    bagsTxt, C_GOLD    },
    };
    for (int i = 0; i < 4; i++) {
        float bx = x + 22.f + i*(tw + 8.f);
        cv_rrect(c, bx, ty, tw, 58.f, 10.f, col_alpha(K[i].col, .09f));
        tx_draw(c, K[i].k, bx + 12.f, ty + 8.f, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        tx_clipped(c, K[i].v, bx + 12.f, ty + 22.f, tw - 24.f,
                   font_track(TF_DISPLAY, 20, TW_BOLD, 0), K[i].col, AL_L, AV_T);
    }

    /* ---- what happens next ---------------------------------------------- */
    float sy = ty + 74.f;
    tx_draw(c, "YOUR JOURNEY", x + 22.f, sy, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    sy += 20.f;

    struct { const char *label; int minute; int done; } STEP[5] = {
        { "Check-in opens",  f->estMin - 180, 0 },
        { "Bag drop closes", f->estMin -  60, 0 },
        { "Boarding begins", f->estMin -  50, 0 },
        { "Gate closes",     f->estMin -  10, 0 },
        { "Departure",       f->estMin,       0 },
    };
    for (int i = 0; i < 5; i++) STEP[i].done = (wo->clock >= (float)STEP[i].minute);

    float lineX = x + 34.f;
    cv_rect(c, lineX, sy + 8.f, 2.f, 4.f*24.f, C_LINE_2);
    for (int i = 0; i < 5; i++) {
        float ry = sy + i*24.f;
        int isNext = STEP[i].done && (i + 1 >= 5 || !STEP[i+1].done);
        Color dot = STEP[i].done ? (isNext ? C_V600 : C_OK) : C_INK_4;
        cv_circle(c, lineX + 1.f, ry + 8.f, STEP[i].done ? 6.f : 4.5f, dot);
        if (isNext) {
            float pulse = 0.5f + 0.5f*sinf(anim_time()*3.f);
            cv_circle_line(c, lineX + 1.f, ry + 8.f, 9.f + pulse*3.f,
                           col_alpha(C_V600, .5f - pulse*0.25f), 1.6f);
        }
        char hm[8]; fmt_hhmm(STEP[i].minute, hm);
        tx_draw(c, hm, lineX + 18.f, ry + 1.f, font_track(TF_MONO, 11, TW_SEMI, 0),
                STEP[i].done ? C_INK_2 : C_INK_4, AL_L, AV_T);
        tx_draw(c, STEP[i].label, lineX + 64.f, ry + 1.f,
                font_make(TF_UI, 12, STEP[i].done ? TW_SEMI : TW_REG),
                STEP[i].done ? C_INK : C_INK_3, AL_L, AV_T);
    }

    /* ---- the passenger's own bags, live from the register --------------- */
    float by = sy + 5.f*24.f + 8.f;
    if (by + 40.f < y + h) {
        tx_draw(c, "YOUR BAGS", x + 22.f, by, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        by += 20.f;
        int shown = 0;
        for (int i = 0; i < wo->nBags && by + 22.f < y + h; i++) {
            Bag *b = &wo->bag[i];
            if (b->pax != p->id) continue;
            Color bc = b->threat ? C_DANGER
                     : (b->state >= BG_LOADED ? C_OK : C_V600);
            cv_circle(c, x + 28.f, by + 7.f, 4.f, bc);
            tx_draw(c, b->tag, x + 40.f, by, font_track(TF_MONO, 11, TW_SEMI, 0),
                    C_INK, AL_L, AV_T);
            char st[70];
            snprintf(st, sizeof st, "%.1f kg   %s", b->weight, bs_name(b->state));
            tx_clipped(c, st, x + w - 22.f, by, w - 170.f,
                       font_make(TF_UI, 11, TW_MED), bc, AL_R, AV_T);
            by += 22.f; shown++;
        }
        if (!shown) {
            tx_draw(c, "No checked bags on this booking", x + 40.f, by,
                    font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);
            by += 22.f;      /* advance, or the advice box lands on top of it */
        }
    }

    /* ---- how busy it will be --------------------------------------------
     *  The thing a passenger actually wants to know before they set off: is
     *  it going to be a crush, and how long is security.  The crowd level is
     *  the number of passengers on flights departing within three quarters
     *  of an hour of theirs -- the people they will be sharing the terminal
     *  and the queue with.                                                  */
    if (by + 66.f < y + h) {
        int nearby = 0;
        for (int i = 0; i < wo->nFlights; i++) {
            Flight *g = &wo->flight[i];
            if (g->arrival || g->state == FS_CANCELLED) continue;
            int d = g->estMin - f->estMin;
            if (d < 0) d = -d;
            if (d <= 45) nearby += g->pax;
        }
        float frac = cv_clampf((float)nearby / 700.f, 0.05f, 1.f);
        const char *lvl; Color lc;
        if      (frac < 0.30f) { lvl = "Quiet";     lc = C_OK;     }
        else if (frac < 0.55f) { lvl = "Moderate";  lc = C_TEAL;   }
        else if (frac < 0.80f) { lvl = "Busy";      lc = C_WARN;   }
        else                   { lvl = "Very busy"; lc = C_DANGER; }

        float secWait = a->flow.waitNow;
        if (secWait < 1.f) secWait = 1.f;

        tx_draw(c, "HOW BUSY IT WILL BE", x + 22.f, by,
                font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
        tx_draw(c, lvl, x + w - 22.f, by, font_make(TF_UI, 11, TW_BOLD),
                lc, AL_R, AV_T);
        by += 16.f;
        ui_meter(x + 22.f, by, w - 44.f, 7.f, frac, col_alpha(lc, .5f), lc);
        by += 13.f;
        char cl[110];
        snprintf(cl, sizeof cl,
                 "about %d travelling around your time  -  security queue ~%.0f min",
                 nearby, secWait);
        tx_draw(c, cl, x + 22.f, by, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_L, AV_T);
        by += 18.f;
    }

    /* ---- live advice ----------------------------------------------------- */
    if (by + 56.f < y + h) {
        by += 6.f;
        float wait = a->flow.waitNow;
        if (wait < 1.f) wait = 1.f;
        float toGo = (float)f->estMin - wo->clock;
        char airsideBy[8];
        fmt_hhmm(f->estMin - 40, airsideBy);

        char adv[190];
        Color bg = C_OK_BG, fg = HEX(0x0A6B42);
        if (f->state >= FS_PUSHBACK && f->state <= FS_DEPARTED) {
            snprintf(adv, sizeof adv,
                     "This flight has left the stand. Please speak to the "
                     "airline desk in the departures hall.");
            bg = C_DANGER_BG; fg = HEX(0x9B2226);
        } else if (f->state == FS_CLOSED) {
            snprintf(adv, sizeof adv,
                     "The gate is closed. Please speak to the airline desk.");
            bg = C_DANGER_BG; fg = HEX(0x9B2226);
        } else if (f->state == FS_FINAL) {
            snprintf(adv, sizeof adv,
                     "Final call. Go straight to gate %s now.", gateBuf);
            bg = C_WARN_BG; fg = HEX(0x8A5A05);
        } else if (f->state == FS_BOARDING) {
            snprintf(adv, sizeof adv,
                     "Boarding at gate %s. Central search is running at about "
                     "%.0f minutes.", gateBuf, wait);
            bg = C_WARN_BG; fg = HEX(0x8A5A05);
        } else {
            snprintf(adv, sizeof adv,
                     "%d minutes to departure. Central search is about %.0f "
                     "minutes, so be through it by %s.",
                     (int)(toGo > 0.f ? toGo : 0.f), wait, airsideBy);
        }
        cv_rrect(c, x + 22.f, by, w - 44.f, 46.f, 10.f, bg);
        tx_backdrop(bg);
        tx_para(c, adv, x + 34.f, by + 8.f, w - 68.f,
                font_make(TF_UI, 11, TW_SEMI), fg, 1);
    }
}

/* ==========================================================================
 *  frequently asked, from the airport's published information
 * ========================================================================== */

typedef struct { const char *q, *a; } Faq;

static const Faq FAQ[] = {
 { "How many airports are there in Mauritius?",
   "Two. Sir Seewoosagur Ramgoolam International (MRU) at Plaisance handles "
   "effectively all international traffic, and Sir Gaetan Duval (RRG) on "
   "Rodrigues takes the inter-island ATR service." },
 { "What is the official name of MRU airport?",
   "Sir Seewoosagur Ramgoolam International Airport. IATA code MRU, ICAO code "
   "FIMP, at Plaine Magnien 51520 in the south east of the island." },
 { "How early should I arrive for an international flight?",
   "Three hours before departure for long-haul and two hours for regional. "
   "Check-in opens three hours ahead; bag drop closes an hour before." },
 { "How long before my flight can I check in with Air Mauritius?",
   "Desks open three hours before departure and close 60 minutes before for "
   "long-haul, 45 minutes for regional and Rodrigues services." },
 { "How far is the airport from the city centre?",
   "About 50 km from Port Louis. Allow 45 to 60 minutes by car, and longer "
   "during the morning peak." },
 { "How big is Mauritius airport?",
   "One passenger terminal opened in 2013, a single runway 14/32 of 3,390 m, "
   "and capacity for roughly four million passengers a year." },
 { "How many terminals does the airport have?",
   "One passenger terminal, with departures on the upper level and arrivals "
   "and baggage reclaim below." },
 { "What shops are at the airport?",
   "Duty free, local craft and gift outlets, a pharmacy and convenience "
   "stores, plus cafes and restaurants both landside and airside." },
 { "Does the airport have free wifi?",
   "Yes. Free wireless internet is available throughout the terminal." },
};
#define NFAQ ((int)(sizeof FAQ / sizeof FAQ[0]))

static void draw_faq(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "FREQUENTLY ASKED", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "select a question to open it", x + 18.f, y + 31.f,
            font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

    Font qf = font_make(TF_UI, 12, TW_SEMI);
    Font af = font_make(TF_UI, 11, TW_REG);
    float inner = w - 62.f;

    /* measure the accordion at its current open state so it can scroll */
    float total = 6.f;
    for (int i = 0; i < NFAQ; i++) {
        float open = anim_get(uidi("faq", i));
        total += 38.f + open * (float)(tx_para_h(c, FAQ[i].a, inner, af, 2) + 14.f);
    }

    float ly = y + 50.f, lh = h - 60.f;
    if (lh < 40.f) return;
    float off = ui_scroll_begin(uid("faqscroll"), x + 6.f, ly, w - 12.f, lh, total);

    float ry = ly - off;
    for (int i = 0; i < NFAQ; i++) {
        float *st = ui_state(uidi("faqopen", i), 0.f);
        float open = anim_to(uidi("faq", i), *st, 13.f);
        float ah = open * (float)(tx_para_h(c, FAQ[i].a, inner, af, 2) + 14.f);

        if (ry + 34.f + ah > ly - 40.f && ry < ly + lh + 40.f) {
            int hov = ui_hit(x + 12.f, ry, w - 24.f, 34.f);
            if (hov) ui_cursor(1);
            if (hov && a->in.pressed) *st = (*st > 0.5f) ? 0.f : 1.f;

            Color bg = col_mix(C_SURF_2, C_V100, open);
            if (hov) bg = col_mix(bg, C_V200, .45f);
            cv_rrect(c, x + 12.f, ry, w - 24.f, 34.f, 8.f, bg);
            tx_backdrop(bg);
            tx_clipped(c, FAQ[i].q, x + 24.f, ry + 9.f, w - 68.f, qf,
                       col_mix(C_INK, C_V700, open), AL_L, AV_T);

            /* the plus closes into a minus as the answer opens */
            float px = x + w - 30.f, py = ry + 17.f;
            cv_rect(c, px - 6.f, py - 1.f, 12.f, 2.f, C_V600);
            if (open < 0.98f)
                cv_rect(c, px - 1.f, py - 6.f + open*5.f, 2.f,
                        12.f - open*10.f, C_V600);

            if (ah > 1.f) {
                cv_clip_push(c, x + 12.f, ry + 34.f, w - 24.f, ah);
                tx_backdrop(C_SURF);
                tx_para(c, FAQ[i].a, x + 30.f, ry + 40.f, inner, af, C_INK_2, 2);
                cv_clip_pop(c);
            }
        }
        ry += 38.f + ah + 4.f;
    }
    ui_scroll_end();
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_welcome(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    float leftW = (w - pad*2.f - 16.f) * 0.46f;

    /*  Sign somebody in and the first thing they should see is their own
     *  flight, not an empty search.  On first view we look for a booking
     *  under the account's name and select it; done once, so the passenger
     *  can then look up anyone else without being dragged back to their own.
     *  A freshly created account with no booking simply finds nothing, which
     *  the panel says plainly rather than looking broken.                   */
    int isTraveller = (a->auth.kind == ACC_TRAVELLER && a->auth.name[0]);
    if (!a->welcomeMatched && a->auth.name[0] && a->selPax == 0 &&
        !a->searchPax[0]) {
        a->welcomeMatched = 1;
        for (int i = 0; i < wo->nPax; i++)
            if (ci_contains(wo->pax[i].name, a->auth.name) &&
                ci_contains(a->auth.name, wo->pax[i].name)) {   /* exact-ish */
                a->selPax = wo->pax[i].id;
                a->selFlight = wo->pax[i].flight;
                break;
            }
    }

    /* first name for the greeting */
    char first[24] = "there";
    if (a->auth.name[0]) {
        snprintf(first, sizeof first, "%s", a->auth.name);
        char *sp = strchr(first, ' ');
        if (sp) *sp = 0;
    }

    /* ---- the search itself ----------------------------------------------- */
    float hy = y + pad;
    ui_card(x + pad, hy, leftW, 150.f, R_LG);
    Paint hg = paint_linear(x + pad, hy, x + pad + leftW, hy,
                            col_alpha(C_V500, .13f), col_alpha(C_MAGENTA, .04f));
    cv_rrect_p(c, x + pad, hy, leftW, 150.f, R_LG, &hg);

    tx_backdrop(C_SURF);
    char eyebrow[48];
    snprintf(eyebrow, sizeof eyebrow, "WELCOME%s%s",
             a->auth.name[0] ? ", " : " TO PLAISANCE",
             a->auth.name[0] ? "" : "");
    tx_draw(c, eyebrow, x + pad + 22.f, hy + 18.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char title[40];
    if (a->auth.name[0]) snprintf(title, sizeof title, "Hello, %s", first);
    else                 snprintf(title, sizeof title, "Find your flight");
    tx_clipped(c, title, x + pad + 22.f, hy + 36.f, leftW - 44.f,
               font_make(TF_DISPLAY, 24, TW_SEMI), C_INK, AL_L, AV_T);
    tx_draw(c, isTraveller ? "Your flight is below -- or look up anyone"
                           : "Type a name, or a booking reference",
            x + pad + 22.f, hy + 70.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_L, AV_T);

    ui_text_field(uid("welcomesearch"), x + pad + 22.f, hy + 92.f,
                  leftW - 44.f, 44.f, a->searchPax, sizeof a->searchPax,
                  "e.g. Ramgoolam", IC_SEARCH);

    /* ---- live results ---------------------------------------------------- */
    float ry0 = hy + 164.f;
    float rh  = h - (ry0 - y) - pad;
    ui_card(x + pad, ry0, leftW, rh, R_LG);

    int idx[96], rank[96], n = 0;
    for (int i = 0; i < wo->nPax && n < 96; i++) {
        int r = match_rank(&wo->pax[i], a->searchPax);
        if (!r) continue;
        idx[n] = i; rank[n] = r; n++;
    }
    for (int i = 1; i < n; i++) {                 /* best matches first */
        int ki = idx[i], kr = rank[i], j = i - 1;
        while (j >= 0 && rank[j] < kr) {
            idx[j+1] = idx[j]; rank[j+1] = rank[j]; j--;
        }
        idx[j+1] = ki; rank[j+1] = kr;
    }

    tx_backdrop(C_SURF);
    char hdr[56];
    snprintf(hdr, sizeof hdr, "%d MATCHING PASSENGER%s", n, n == 1 ? "" : "S");
    tx_draw(c, a->searchPax[0] ? hdr : "TRAVELLING TODAY",
            x + pad + 18.f, ry0 + 15.f, font_track(TF_UI, 10, TW_BOLD, 2),
            C_V600, AL_L, AV_T);

    float lY = ry0 + 40.f, lH = rh - 50.f;
    float rowH = 52.f;
    float off = ui_scroll_begin(uid("welclist"), x + pad + 6.f, lY,
                                leftW - 12.f, lH, n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Passenger *p = &wo->pax[idx[i]];
        float py = lY + i*rowH - off;
        if (py + rowH < lY || py > lY + lH) continue;

        uint64_t id = uidi("welcrow", p->id);
        int hov = ui_hit(x + pad + 12.f, py, leftW - 24.f, rowH - 6.f);
        float e = ui_hover_f(id, hov);
        int sel = (a->selPax == p->id);
        if (hov) ui_cursor(1);
        if (hov && a->in.pressed) { a->selPax = p->id; a->selFlight = p->flight; }

        Color bg = sel ? C_V100 : col_mix(C_SURF, C_V50, e);
        cv_rrect(c, x + pad + 12.f, py, leftW - 24.f, rowH - 6.f, 10.f, bg);
        if (sel)
            cv_rrect(c, x + pad + 12.f, py + 8.f, 3.f, rowH - 22.f, 1.5f, C_V600);

        cv_circle(c, x + pad + 38.f, py + 23.f, 14.f, col_alpha(C_V400, .18f));
        char ini[2] = { p->name[0], 0 };
        tx_backdrop(bg);
        tx_draw(c, ini, x + pad + 38.f, py + 23.f, font_make(TF_UI, 13, TW_BOLD),
                C_V700, AL_C, AV_M);

        tx_clipped(c, p->name, x + pad + 60.f, py + 8.f, leftW - 150.f,
                   font_make(TF_UI, 13, TW_SEMI), C_INK, AL_L, AV_T);
        Flight *f = flight_by_id(wo, p->flight);
        char sub[80];
        snprintf(sub, sizeof sub, "%s  to %s", f ? f->no : "----",
                 f ? wo->airport[f->airport].city : "");
        tx_clipped(c, sub, x + pad + 60.f, py + 27.f, leftW - 150.f,
                   font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);
        if (f) {
            char hm[8]; fmt_hhmm(f->estMin, hm);
            tx_draw(c, hm, x + pad + leftW - 20.f, py + 8.f,
                    font_track(TF_MONO, 13, TW_BOLD, 0),
                    f->delayMin >= 15 ? C_DANGER : C_INK_2, AL_R, AV_T);
        }
    }
    ui_scroll_end();

    if (!n) {
        tx_backdrop(C_SURF);
        icon_draw(c, IC_SEARCH, x + pad + leftW*0.5f, ry0 + rh*0.5f - 16.f,
                  26.f, C_INK_4);
        tx_draw(c, "No passenger of that name is travelling today",
                x + pad + leftW*0.5f, ry0 + rh*0.5f + 12.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    }

    /* ---- right: the selected journey, then the FAQ ------------------------ */
    float rx = x + pad + leftW + 16.f;
    float rw = w - pad*2.f - leftW - 16.f;
    float jh = (h - pad*2.f) * 0.66f;

    Passenger *sp = NULL;
    for (int i = 0; i < wo->nPax; i++)
        if (wo->pax[i].id == a->selPax) { sp = &wo->pax[i]; break; }

    if (sp) {
        draw_journey(a, rx, y + pad, rw, jh, sp);
    } else {
        ui_card(rx, y + pad, rw, jh, R_LG);
        tx_backdrop(C_SURF);
        icon_draw(c, IC_TICKET, rx + rw*0.5f, y + pad + jh*0.5f - 34.f, 34.f,
                  C_INK_4);
        /*  A signed-in traveller whose auto-match found nothing gets told
         *  why, rather than a generic prompt that looks like a dead end.   */
        if (isTraveller && a->welcomeMatched) {
            char m[110];
            snprintf(m, sizeof m, "No booking found under %s", a->auth.name);
            tx_draw(c, m, rx + rw*0.5f, y + pad + jh*0.5f,
                    font_make(TF_UI, 13, TW_SEMI), C_INK_2, AL_C, AV_M);
            tx_draw(c, "Search above by the name printed on your ticket",
                    rx + rw*0.5f, y + pad + jh*0.5f + 22.f,
                    font_make(TF_UI, 11, TW_REG), C_INK_3, AL_C, AV_M);
        } else {
            tx_draw(c, "Select a passenger to see their journey",
                    rx + rw*0.5f, y + pad + jh*0.5f,
                    font_make(TF_UI, 13, TW_MED), C_INK_2, AL_C, AV_M);
            tx_draw(c, "gate, seat, bags and what happens next",
                    rx + rw*0.5f, y + pad + jh*0.5f + 22.f,
                    font_make(TF_UI, 11, TW_REG), C_INK_3, AL_C, AV_M);
        }
    }

    draw_faq(a, rx, y + pad + jh + 12.f, rw, h - pad*2.f - jh - 12.f);
}
