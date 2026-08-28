/* ==========================================================================
 *  AURA :: screen_services.c   --   landside services
 *
 *  Everything a passenger deals with either side of the terminal doors, taken
 *  from what the airport actually publishes: ground transport, the two car
 *  parks and their tariffs, the car hire desks on Level 0, the fast track and
 *  lounge products, and the reference facts and telephone numbers.
 *
 *  The figures are not static text.  Car park occupancy, the bus countdown,
 *  hire desk availability and the fast track queue all move with the
 *  simulation clock and the passenger flow model, so the screen reads as an
 *  operational display rather than a brochure.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 *  ground transport
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *mode, *route, *detail;
    int   icon;
    int   everyMin;        /* headway, 0 for on-demand                       */
    int   firstMin, lastMin;/* service hours; 0,0 = round the clock          */
    float fareMin, fareMax;/* rupees                                         */
} Transport;

/*  The scheduled buses do not run through the night -- the last Port Louis
 *  service is at 22:00 and the last Curepipe one at 20:00 -- so their next
 *  departure has to be shown against the real clock, not blindly counted off
 *  it.  Only the taxi rank and the pre-booked transfers are round the clock.
 *  (This is the fix for a night-time board that promised a bus in ten
 *  minutes when the last one had left hours earlier.)                       */
static const Transport TRANSPORT[] = {
    { "Bus",            "Port Louis  -  Airport",
      "Airport Terminal Operations Ltd, via Mahebourg", IC_TRUCK, 30,
      5*60, 22*60, 40.f, 60.f },
    { "Bus",            "Curepipe  -  Airport",
      "Regular service via the Royal Road",             IC_TRUCK, 45,
      5*60 + 30, 20*60, 35.f, 50.f },
    { "Taxi",           "Metered, kerbside",
      "Rank outside Arrivals, Level 0 -- 24 hours",     IC_TRUCK,  0,
      0, 0, 900.f, 2200.f },
    { "Hotel shuttle",  "Grand Baie / Flic-en-Flac / Belle Mare",
      "Pre-booked through the hotel",                   IC_TRUCK,  0,
      0, 0, 0.f, 0.f },
    { "VIP transfer",   "Private vehicle, meet and greet",
      "Booked with the fast track product",             IC_USER,   0,
      0, 0, 2500.f, 4500.f },
};
#define NTRANSPORT ((int)(sizeof TRANSPORT / sizeof TRANSPORT[0]))

/* --------------------------------------------------------------------------
 *  car hire desks, Level 0
 * ------------------------------------------------------------------------- */

static const char *HIRE[] = {
    "Avis", "First", "Europcar", "Dollar", "Budget", "Hertz", "Sixt", "AKD"
};
#define NHIRE ((int)(sizeof HIRE / sizeof HIRE[0]))

/* how busy the landside is, driven by the flow model and the clock */
static float landside_load(App *a)
{
    float pax = a->flow.nHist ? a->flow.history[a->flow.nHist-1] : 12.f;
    float t = cv_clampf(pax / 45.f, 0.f, 1.f);
    return t;
}

/* ==========================================================================
 *  panels
 * ========================================================================== */

static void panel_transport(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "GROUND TRANSPORT", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "to and from the terminal", x + 18.f, y + 31.f,
            font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);

    float ry = y + 54.f;
    for (int i = 0; i < NTRANSPORT; i++) {
        const Transport *t = &TRANSPORT[i];
        if (ry + 58.f > y + h) break;
        if (i & 1) cv_rrect(c, x + 12.f, ry - 4.f, w - 24.f, 56.f, 9.f, C_SURF_2);

        cv_circle(c, x + 34.f, ry + 20.f, 15.f, col_alpha(C_V500, .12f));
        icon_draw(c, t->icon, x + 34.f, ry + 20.f, 16.f, C_V600);

        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);
        tx_draw(c, t->mode, x + 58.f, ry, font_make(TF_UI, 13, TW_SEMI),
                C_INK, AL_L, AV_T);
        tx_clipped(c, t->route, x + 58.f, ry + 18.f, w - 180.f,
                   font_make(TF_UI, 11, TW_MED), C_INK_2, AL_L, AV_T);
        tx_clipped(c, t->detail, x + 58.f, ry + 33.f, w - 180.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

        /* next departure, against the real clock and the service hours */
        char right[40];
        right[0] = 0;
        if (t->everyMin > 0) {
            int now = (int)a->w.clock;
            int running = (now >= t->firstMin && now < t->lastMin);
            if (running) {
                int mins = t->everyMin - (now % t->everyMin);
                char nxt[24]; snprintf(nxt, sizeof nxt, "in %d min", mins);
                tx_draw(c, nxt, x + w - 18.f, ry + 2.f,
                        font_make(TF_UI, 12, TW_BOLD), C_OK, AL_R, AV_T);
                snprintf(right, sizeof right, "every %d min", t->everyMin);
            } else {
                /* closed for the night: show when the first bus runs */
                char hm[8]; fmt_hhmm(t->firstMin, hm);
                tx_draw(c, "not running", x + w - 18.f, ry + 2.f,
                        font_make(TF_UI, 12, TW_BOLD), C_INK_4, AL_R, AV_T);
                snprintf(right, sizeof right, "first bus %s", hm);
            }
        } else {
            tx_draw(c, "on demand", x + w - 18.f, ry + 2.f,
                    font_make(TF_UI, 12, TW_BOLD), C_V600, AL_R, AV_T);
        }
        if (right[0])
            tx_draw(c, right, x + w - 18.f, ry + 20.f,
                    font_make(TF_UI, 10, TW_REG), C_INK_3, AL_R, AV_T);
        if (t->fareMax > 0.f) {
            char fare[40];
            snprintf(fare, sizeof fare, "Rs %.0f - %.0f", t->fareMin, t->fareMax);
            tx_draw(c, fare, x + w - 18.f, ry + 35.f,
                    font_track(TF_MONO, 10, TW_MED, 0), C_INK_3, AL_R, AV_T);
        }
        ry += 58.f;
    }
}


/* --------------------------------------------------------------------------
 *  shops, cafes and restaurants
 *
 *  Opening hours matter more than the list does.  A passenger on the 04:30
 *  European bank wants to know what is actually open at four in the morning,
 *  and most of the terminal is not -- so every entry is drawn against the
 *  live clock and says whether you can walk into it right now.
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *name, *kind, *where;
    int openMin, closeMin;       /* minutes past midnight                   */
    int icon;
} Outlet;

static const Outlet OUTLETS[] = {
    { "Duty Free Mauritius", "Duty free",  "Departures, airside",
      270, 1410, IC_BOX },
    { "Le Bourbon",          "Restaurant", "Departures, Level 1",
      300, 1380, IC_HEART },
    { "Cafe Lux*",           "Cafe",       "Departures, airside",
      240, 1440, IC_HEART },
    { "Sunrise Bakery",      "Bakery",     "Landside, check-in hall",
      240,  900, IC_HEART },
    { "Rum & Cane",          "Spirits",    "Departures, airside",
      330, 1380, IC_BOX },
    { "Ile Maurice Crafts",  "Gifts",      "Departures, airside",
      360, 1320, IC_STAR },
    { "Newslink",            "News, books","Both piers",
      300, 1380, IC_FILE },
    { "Pharmacie de l'Aeroport", "Pharmacy","Landside, arrivals",
      420, 1140, IC_SHIELD },
    { "Currency Exchange",   "Bureau de change", "Arrivals and departures",
      240, 1440, IC_GLOBE },
    { "Wi-Fi & Charging Bar","Facilities", "Departures, Gate 4",
        0, 1440, IC_BOLT },
};
#define NOUTLETS ((int)(sizeof OUTLETS / sizeof OUTLETS[0]))

static int outlet_open(const Outlet *o, float clock)
{
    if (o->openMin == 0 && o->closeMin == 1440) return 1;      /* always     */
    if (o->closeMin > o->openMin)
        return clock >= (float)o->openMin && clock < (float)o->closeMin;
    /* wraps past midnight */
    return clock >= (float)o->openMin || clock < (float)o->closeMin;
}

static void panel_shops(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "SHOPS AND DINING", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    int openNow = 0;
    for (int i = 0; i < NOUTLETS; i++)
        if (outlet_open(&OUTLETS[i], wo->clock)) openNow++;
    char sub[80];
    snprintf(sub, sizeof sub, "%d of %d open right now", openNow, NOUTLETS);
    tx_draw(c, sub, x + 18.f, y + 31.f, font_make(TF_UI, 11, TW_REG),
            C_INK_3, AL_L, AV_T);

    float ry = y + 52.f;
    float rowH = 40.f;
    float lh = h - (ry - y) - 12.f;
    float off = ui_scroll_begin(uid("svshops"), x + 6.f, ry, w - 12.f, lh,
                                NOUTLETS*rowH + 8.f);
    for (int i = 0; i < NOUTLETS; i++) {
        const Outlet *o = &OUTLETS[i];
        float yy = ry + i*rowH - off;
        if (yy + rowH < ry || yy > ry + lh) continue;

        int isOpen = outlet_open(o, wo->clock);
        if (i & 1) cv_rrect(c, x + 12.f, yy, w - 24.f, rowH - 4.f, 8.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);

        Color ic = isOpen ? C_V600 : C_INK_4;
        cv_circle(c, x + 32.f, yy + 18.f, 13.f, col_alpha(ic, .11f));
        icon_draw(c, o->icon, x + 32.f, yy + 18.f, 14.f, ic);

        tx_clipped(c, o->name, x + 52.f, yy + 5.f, w - 150.f,
                   font_make(TF_UI, 12, TW_SEMI),
                   isOpen ? C_INK : C_INK_3, AL_L, AV_T);
        char meta[90];
        snprintf(meta, sizeof meta, "%s  -  %s", o->kind, o->where);
        tx_clipped(c, meta, x + 52.f, yy + 21.f, w - 150.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

        if (isOpen) {
            char until[30];
            if (o->openMin == 0 && o->closeMin == 1440)
                snprintf(until, sizeof until, "24 hours");
            else {
                char hm[8]; fmt_hhmm(o->closeMin, hm);
                snprintf(until, sizeof until, "until %s", hm);
            }
            tx_draw(c, "OPEN", x + w - 18.f, yy + 6.f,
                    font_track(TF_UI, 9, TW_BOLD, 1), C_OK, AL_R, AV_T);
            tx_draw(c, until, x + w - 18.f, yy + 21.f,
                    font_make(TF_UI, 10, TW_REG), C_INK_3, AL_R, AV_T);
        } else {
            char from[30];
            char hm[8]; fmt_hhmm(o->openMin, hm);
            snprintf(from, sizeof from, "opens %s", hm);
            tx_draw(c, "CLOSED", x + w - 18.f, yy + 6.f,
                    font_track(TF_UI, 9, TW_BOLD, 1), C_INK_4, AL_R, AV_T);
            tx_draw(c, from, x + w - 18.f, yy + 21.f,
                    font_make(TF_UI, 10, TW_REG), C_INK_4, AL_R, AV_T);
        }
    }
    ui_scroll_end();
}

static void panel_parking(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "CAR PARKS", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float load = landside_load(a);
    struct { const char *name, *note; int cap; float fill; } P[2] = {
        { "Short stay", "closest to the terminal, hourly tariff", 420,
          cv_clampf(0.34f + load*0.55f, 0.f, 0.99f) },
        { "Long stay",  "several days or weeks, daily tariff",    760,
          cv_clampf(0.52f + load*0.28f, 0.f, 0.99f) },
    };

    float ry = y + 40.f;
    for (int i = 0; i < 2; i++) {
        float used = P[i].fill;
        int spaces = (int)(P[i].cap * (1.f - used));
        Color bar = used > 0.92f ? C_DANGER : (used > 0.78f ? C_WARN : C_OK);

        tx_draw(c, P[i].name, x + 18.f, ry, font_make(TF_UI, 13, TW_SEMI),
                C_INK, AL_L, AV_T);
        char sp[40];
        snprintf(sp, sizeof sp, "%d free", spaces);
        tx_draw(c, sp, x + w - 18.f, ry, font_make(TF_UI, 13, TW_BOLD),
                bar, AL_R, AV_T);
        tx_clipped(c, P[i].note, x + 18.f, ry + 17.f, w - 40.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);
        ui_meter(x + 18.f, ry + 33.f, w - 36.f, 7.f, used,
                 col_alpha(bar, .5f), bar);
        char cap[48];
        snprintf(cap, sizeof cap, "%.0f%% of %d spaces", used*100.f, P[i].cap);
        tx_draw(c, cap, x + 18.f, ry + 44.f, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_L, AV_T);
        ry += 74.f;
    }

    ui_divider(x + 18.f, ry, w - 36.f);
    ry += 12.f;
    tx_draw(c, "TARIFF", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    struct { const char *k, *v; } T[5] = {
        { "First hour",        "Rs 40"  },
        { "Up to 2 hours",     "Rs 70"  },
        { "Up to 4 hours",     "Rs 130" },
        { "Up to 8 hours",     "Rs 220" },
        { "Long stay, per day","Rs 350" },
    };
    for (int i = 0; i < 5; i++) {
        if (ry + 16.f > y + h) break;
        tx_draw(c, T[i].k, x + 18.f, ry, font_make(TF_UI, 11, TW_MED),
                C_INK_2, AL_L, AV_T);
        tx_draw(c, T[i].v, x + w - 18.f, ry,
                font_track(TF_MONO, 11, TW_SEMI, 0), C_INK, AL_R, AV_T);
        ry += 17.f;
    }
}

static void panel_hire(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "CAR HIRE  -  LEVEL 0", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "desks in the arrivals hall car rental booth", x + 18.f, y + 31.f,
            font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);

    float load = landside_load(a);
    float ry = y + 52.f;
    for (int i = 0; i < NHIRE; i++) {
        if (ry + 26.f > y + h) break;
        /* fleet availability falls as the arrivals bank lands */
        uint32_t r = (uint32_t)(i*2654435761u) ^ 0x51u;
        float base = 0.35f + (float)((r >> 8) % 55) / 100.f;
        int avail = (int)(base * 40.f * (1.f - load*0.55f));
        Color ac = avail > 12 ? C_OK : (avail > 4 ? C_WARN : C_DANGER);

        if (i & 1) cv_rrect(c, x + 12.f, ry - 3.f, w - 24.f, 24.f, 6.f, C_SURF_2);
        tx_backdrop((i & 1) ? C_SURF_2 : C_SURF);
        cv_circle(c, x + 24.f, ry + 9.f, 3.f, ac);
        tx_draw(c, HIRE[i], x + 36.f, ry, font_make(TF_UI, 12, TW_MED),
                C_INK, AL_L, AV_T);
        char av[24];
        snprintf(av, sizeof av, "%d cars", avail);
        tx_draw(c, av, x + w - 18.f, ry, font_track(TF_MONO, 11, TW_SEMI, 0),
                ac, AL_R, AV_T);
        ry += 26.f;
    }
}

static void panel_fasttrack(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ui_card(x, y, w, h, R_LG);
    Paint g = paint_linear(x, y, x, y + 74.f, col_alpha(C_MAGENTA, .12f),
                           col_alpha(C_MAGENTA, .0f));
    cv_rrect_p(c, x, y, w, h, R_LG, &g);
    tx_backdrop(C_SURF);
    tx_draw(c, "FAST TRACK & LOUNGE", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_MAGENTA, AL_L, AV_T);

    float wait = a->flow.waitNow;
    float saved = wait > 1.f ? wait - 1.f : 0.f;

    struct { const char *name, *what, *price; Color col; } S[3] = {
        { "Fast Track security", "priority lane at central search", "Rs 1,200", C_MAGENTA },
        { "Meet & greet",        "escort from kerbside to the gate", "Rs 3,400", C_V600 },
        { "Executive lounge",    "airside, before the pier",         "Rs 1,900", C_TEAL },
    };
    float ry = y + 40.f;
    for (int i = 0; i < 3; i++) {
        if (ry + 50.f > y + h) break;
        cv_rrect(c, x + 14.f, ry, w - 28.f, 46.f, 9.f, col_alpha(S[i].col, .07f));
        cv_rrect(c, x + 14.f, ry + 8.f, 3.f, 30.f, 1.5f, S[i].col);
        tx_draw(c, S[i].name, x + 26.f, ry + 6.f, font_make(TF_UI, 12, TW_SEMI),
                C_INK, AL_L, AV_T);
        tx_clipped(c, S[i].what, x + 26.f, ry + 24.f, w - 130.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_3, AL_L, AV_T);
        tx_draw(c, S[i].price, x + w - 26.f, ry + 14.f,
                font_track(TF_MONO, 12, TW_BOLD, 0), S[i].col, AL_R, AV_T);
        ry += 52.f;
    }

    if (ry + 40.f <= y + h) {
        /* A quiet hall genuinely has a sub-minute wait; printing "about 0
         * minutes, saving 0" reads as a broken readout rather than a calm
         * terminal, so the quiet case gets its own wording. */
        char msg[150];
        if (wait < 2.f)
            snprintf(msg, sizeof msg,
                     "Central search is flowing freely at under two minutes. "
                     "Fast track is worth little right now.");
        else
            snprintf(msg, sizeof msg,
                     "Central search is running at about %.0f minutes. Fast "
                     "track holds under a minute, saving roughly %.0f.",
                     wait, saved);
        cv_rrect(c, x + 14.f, ry, w - 28.f, 42.f, 9.f,
                 saved > 8.f ? C_WARN_BG : C_OK_BG);
        tx_backdrop(saved > 8.f ? C_WARN_BG : C_OK_BG);
        tx_para(c, msg, x + 26.f, ry + 8.f, w - 52.f,
                font_make(TF_UI, 10, TW_MED),
                saved > 8.f ? HEX(0x8A5A05) : HEX(0x0A6B42), 1);
    }
}

static void panel_facts(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    ui_panel_dark(x, y, w, h, R_LG);
    tx_backdrop(C_NIGHT_2);
    tx_draw(c, "AIRPORT REFERENCE", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), col_alpha(C_V200, .95f),
            AL_L, AV_T);

    struct { const char *k, *v; } F[9] = {
        { "Name",     "Sir Seewoosagur Ramgoolam Intl" },
        { "IATA",     "MRU" },
        { "ICAO",     "FIMP" },
        { "Location", "Plaine Magnien 51520, Mauritius" },
        { "Distance", "50 km from Port Louis, 45-60 min" },
        { "Runway",   "14/32, 3,390 m asphalt" },
        { "Elevation","186 ft" },
        { "Time zone","UTC+4, no daylight saving" },
        { "Terminals","One passenger terminal, opened 2013" },
    };
    float ry = y + 40.f;
    for (int i = 0; i < 9; i++) {
        if (ry + 17.f > y + h - 96.f) break;
        tx_draw(c, F[i].k, x + 18.f, ry, font_make(TF_UI, 10, TW_MED),
                col_alpha(C_V300, .8f), AL_L, AV_T);
        tx_clipped(c, F[i].v, x + w - 18.f, ry, w - 120.f,
                   font_make(TF_UI, 10, TW_SEMI), col_alpha(HEX(0xFFFFFF), .92f),
                   AL_R, AV_T);
        ry += 17.f;
    }

    ry += 8.f;
    tx_draw(c, "TELEPHONE", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            col_alpha(C_V300, .75f), AL_L, AV_T);
    ry += 16.f;
    struct { const char *k, *v; } N[4] = {
        { "Airport switchboard", "+230 603 6000" },
        { "Flight enquiries",    "+230 603 6300" },
        { "Air Mauritius",       "+230 207 7070" },
        { "Lost property",       "+230 603 6247" },
    };
    for (int i = 0; i < 4; i++) {
        if (ry + 16.f > y + h) break;
        tx_draw(c, N[i].k, x + 18.f, ry, font_make(TF_UI, 10, TW_MED),
                col_alpha(C_V300, .8f), AL_L, AV_T);
        tx_draw(c, N[i].v, x + w - 18.f, ry, font_track(TF_MONO, 10, TW_SEMI, 0),
                C_GOLD, AL_R, AV_T);
        ry += 16.f;
    }
    (void)wo;
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_services(App *a, float x, float y, float w, float h)
{
    float pad = PAD;
    float colW = (w - pad*2.f - 24.f) / 3.f;
    float top = y + pad;
    float fullH = h - pad*2.f;

    /* left: ground transport over car hire */
    panel_transport(a, x + pad, top, colW, fullH*0.55f);
    panel_hire     (a, x + pad, top + fullH*0.55f + 12.f, colW,
                    fullH*0.45f - 12.f);

    /* middle: the car parks over the shops and restaurants */
    panel_parking(a, x + pad + colW + 12.f, top, colW, fullH*0.56f);
    panel_shops  (a, x + pad + colW + 12.f, top + fullH*0.56f + 12.f, colW,
                  fullH*0.44f - 12.f);

    /* right: fast track over the reference card */
    float rx = x + pad + (colW + 12.f)*2.f;
    panel_fasttrack(a, rx, top, colW, fullH*0.52f);
    panel_facts    (a, rx, top + fullH*0.52f + 12.f, colW, fullH*0.48f - 12.f);
}
