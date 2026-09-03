/* ==========================================================================
 *  AURA :: main.c
 *
 *  Airport Unified Resource Administration
 *  Sir Seewoosagur Ramgoolam International Airport (MRU / FIMP)
 *  Plaisance, Mauritius
 *
 *  Trois Freres Systems Ltd.  --  SIS 2075 Software Engineering 1
 *
 *  A native Win32 application.  No frameworks, no web view, no external
 *  libraries: one window, one 32-bit back buffer, and a software rasteriser.
 * ========================================================================== */

#include "app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static App  g_app;
static HWND g_hwnd;
static int  g_running = 1;
static int  g_winW = 1440, g_winH = 900;

/* ==========================================================================
 *  navigation model
 * ========================================================================== */

/*  Fifteen destinations will not fit a flat list on a laptop screen, so the
 *  sidebar is grouped and scrolls.  `section` is the heading this entry sits
 *  under, or NULL to continue the one above.                               */
typedef struct { const char *label, *sub; int icon; const char *section; } NavItem;

static const NavItem NAV[SC_COUNT] = {
    { "Welcome",      "find your flight",      IC_USER,     "PASSENGER"  },
    { "Movements",    "arrivals & departures", IC_PLANE,    NULL         },
    { "Services",     "landside & transport",  IC_TRUCK,    NULL         },
    { "Reviews",      "what passengers say",   IC_STAR,     NULL         },

    { "Operations",   "flights, gates, runway",IC_GAUGE,    "OPERATIONS" },
    { "Airfield",     "live movements",        IC_RADAR,    NULL         },
    { "Check-in",     "desks & boarding",      IC_TICKET,   NULL         },
    { "Baggage",      "handling system",       IC_LUGGAGE,  NULL         },
    { "Terminal Flow","queues & search",       IC_USERS,    NULL         },

    { "Emergency",    "incidents & alerts",    IC_ALERT,    "SAFETY"     },
    { "Surveillance", "computer vision",       IC_SCAN,     NULL         },

    { "AI Suite",     "five engines",          IC_SPARK,    "INSIGHT"    },
    { "Analytics",    "reports & totals",      IC_CHART,    NULL         },

    { "Resources",    "fleet, staff, property",IC_LAYERS,   "ADMIN"      },
    { "Records",      "files & journal",       IC_DATABASE, NULL         },
};

/*  Which screens a traveller may see.  Everything else is the operations
 *  floor and the administrative record, which is staff only.  The sidebar,
 *  the screen dispatch and the number-key shortcuts all consult this one
 *  function, so there is a single definition of what a passenger can reach. */
static int screen_is_passenger(int sc)
{
    return sc == SC_WELCOME || sc == SC_BOARD ||
           sc == SC_SERVICES || sc == SC_REVIEWS;
}

/* ==========================================================================
 *  shared drawing helpers
 * ========================================================================== */

/*  A top-down airliner.  Proportions follow a real planform: swept wings set
 *  a little behind the mid point, engines under the wing, a tailplane and a
 *  fin.  `span` is the wingspan in pixels and everything else scales off it,
 *  so an ATR and an A350 look like what they are.                           */
void draw_aircraft(Canvas *c, float cx, float cy, float hd, float span,
                   Color body, Color accent, int strobeOn, float detail)
{
    float s   = span * 0.5f;             /* half span                       */
    float len = span * 0.92f;            /* fuselage length                 */
    float ch  = cosf(hd), sh = sinf(hd);

    /* local (forward, right) -> screen */
    #define PX(f,r) (cx + (f)*ch - (r)*sh)
    #define PY(f,r) (cy + (f)*sh + (r)*ch)

    float hl = len * 0.5f;
    float fw = span * 0.086f;            /* fuselage half width             */

    /* Wings: a proper trapezoid with a long root chord and a swept leading
     * edge.  Getting the root chord right is what stops a top-down airliner
     * reading as a dragonfly at small sizes. */
    Path wing; path_reset(&wing);
    path_move(&wing, PX( hl*0.34f, fw*0.85f), PY( hl*0.34f, fw*0.85f));
    path_line(&wing, PX(-hl*0.34f, s),        PY(-hl*0.34f, s));
    path_line(&wing, PX(-hl*0.56f, s),        PY(-hl*0.56f, s));
    path_line(&wing, PX(-hl*0.16f, fw*0.95f), PY(-hl*0.16f, fw*0.95f));
    path_line(&wing, PX(-hl*0.16f,-fw*0.95f), PY(-hl*0.16f,-fw*0.95f));
    path_line(&wing, PX(-hl*0.56f,-s),        PY(-hl*0.56f,-s));
    path_line(&wing, PX(-hl*0.34f,-s),        PY(-hl*0.34f,-s));
    path_line(&wing, PX( hl*0.34f,-fw*0.85f), PY( hl*0.34f,-fw*0.85f));
    path_close(&wing);
    cv_fill_col(c, &wing, body);

    /* tailplane */
    float tw = s * 0.36f;
    Path tail; path_reset(&tail);
    path_move(&tail, PX(-hl*0.76f,  fw*0.7f), PY(-hl*0.76f,  fw*0.7f));
    path_line(&tail, PX(-hl*0.96f,  tw),      PY(-hl*0.96f,  tw));
    path_line(&tail, PX(-hl*1.06f,  tw),      PY(-hl*1.06f,  tw));
    path_line(&tail, PX(-hl*0.92f,  fw*0.7f), PY(-hl*0.92f,  fw*0.7f));
    path_line(&tail, PX(-hl*0.92f, -fw*0.7f), PY(-hl*0.92f, -fw*0.7f));
    path_line(&tail, PX(-hl*1.06f, -tw),      PY(-hl*1.06f, -tw));
    path_line(&tail, PX(-hl*0.96f, -tw),      PY(-hl*0.96f, -tw));
    path_line(&tail, PX(-hl*0.76f, -fw*0.7f), PY(-hl*0.76f, -fw*0.7f));
    path_close(&tail);
    cv_fill_col(c, &tail, body);

    /* fuselage: nose cone, parallel body, tail cone */
    Path fus; path_reset(&fus);
    path_move(&fus, PX(hl, 0.f), PY(hl, 0.f));
    path_cubic(&fus, PX(hl*0.80f, fw*0.85f), PY(hl*0.80f, fw*0.85f),
                     PX(hl*0.55f, fw),       PY(hl*0.55f, fw),
                     PX(hl*0.30f, fw),       PY(hl*0.30f, fw));
    path_line(&fus,  PX(-hl*0.72f, fw*0.92f), PY(-hl*0.72f, fw*0.92f));
    path_cubic(&fus, PX(-hl*0.95f, fw*0.55f), PY(-hl*0.95f, fw*0.55f),
                     PX(-hl*1.05f, fw*0.20f), PY(-hl*1.05f, fw*0.20f),
                     PX(-hl*1.10f, 0.f),      PY(-hl*1.10f, 0.f));
    path_cubic(&fus, PX(-hl*1.05f,-fw*0.20f), PY(-hl*1.05f,-fw*0.20f),
                     PX(-hl*0.95f,-fw*0.55f), PY(-hl*0.95f,-fw*0.55f),
                     PX(-hl*0.72f,-fw*0.92f), PY(-hl*0.72f,-fw*0.92f));
    path_line(&fus,  PX(hl*0.30f, -fw),       PY(hl*0.30f, -fw));
    path_cubic(&fus, PX(hl*0.55f,-fw),        PY(hl*0.55f,-fw),
                     PX(hl*0.80f,-fw*0.85f),  PY(hl*0.80f,-fw*0.85f),
                     PX(hl, 0.f),             PY(hl, 0.f));
    path_close(&fus);
    cv_fill_col(c, &fus, col_lighten(body, 0.10f));

    if (detail > 0.45f) {
        /* engines */
        float ex = -hl*0.24f, er = s*0.42f, el = span*0.13f, ew = span*0.048f;
        for (int i = 0; i < 2; i++) {
            float r = (i ? er : -er);
            Path eng; path_reset(&eng);
            path_move(&eng, PX(ex+el*0.5f, r-ew), PY(ex+el*0.5f, r-ew));
            path_line(&eng, PX(ex+el*0.5f, r+ew), PY(ex+el*0.5f, r+ew));
            path_line(&eng, PX(ex-el*0.5f, r+ew), PY(ex-el*0.5f, r+ew));
            path_line(&eng, PX(ex-el*0.5f, r-ew), PY(ex-el*0.5f, r-ew));
            path_close(&eng);
            cv_fill_col(c, &eng, col_darken(body, 0.28f));
        }
        /* fin, drawn as a shadow off the tail so it reads from above */
        Path fin; path_reset(&fin);
        path_move(&fin, PX(-hl*0.68f, 0.f),        PY(-hl*0.68f, 0.f));
        path_line(&fin, PX(-hl*1.06f, span*0.055f),PY(-hl*1.06f, span*0.055f));
        path_line(&fin, PX(-hl*1.06f,-span*0.055f),PY(-hl*1.06f,-span*0.055f));
        path_close(&fin);
        cv_fill_col(c, &fin, accent);
        /* cockpit glass */
        cv_circle(c, PX(hl*0.74f, 0.f), PY(hl*0.74f, 0.f), span*0.035f,
                  col_darken(body, 0.45f));
    }

    if (strobeOn && detail > 0.35f) {
        cv_glow(c, PX(-hl*0.60f, s), PY(-hl*0.60f, s), span*0.16f, C_DANGER, .85f);
        cv_glow(c, PX(-hl*0.60f,-s), PY(-hl*0.60f,-s), span*0.16f, C_OK, .85f);
        cv_circle(c, PX(-hl*0.60f, s), PY(-hl*0.60f, s), span*0.026f, HEX(0xFFFFFF));
        cv_circle(c, PX(-hl*0.60f,-s), PY(-hl*0.60f,-s), span*0.026f, HEX(0xFFFFFF));
    }
    #undef PX
    #undef PY
}

void draw_stat_tile(Canvas *c, float x, float y, float w, float h,
                    const char *label, const char *value, const char *sub,
                    int icon, Color accent)
{
    ui_card(x, y, w, h, R_LG);
    Paint g = paint_linear(x, y, x, y + h, col_alpha(accent, .10f),
                           col_alpha(accent, .00f));
    cv_rrect_p(c, x, y, w, h, R_LG, &g);
    cv_rrect(c, x, y, 3.f, h, 1.5f, accent);

    tx_backdrop(C_SURF);
    if (icon) {
        cv_circle(c, x + w - 30.f, y + 28.f, 15.f, col_alpha(accent, .14f));
        icon_draw(c, icon, x + w - 30.f, y + 28.f, 17.f, accent);
    }
    tx_draw(c, label, x + 18.f, y + 15.f, font_track(TF_UI, 10, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);

    /* The value is sized from the tile, and the caption is only drawn when
     * there is genuinely room left under it -- these tiles are reused at
     * several heights and a fixed layout collides at the smaller ones. */
    Font vf = font_make(TF_DISPLAY, h >= 88.f ? 30 : 24, TW_BOLD);
    float vy = y + 31.f;
    tx_draw(c, value, x + 17.f, vy, vf, C_INK, AL_L, AV_T);

    float vh = (float)tx_height(c, vf);
    Font sf = font_make(TF_UI, 11, TW_MED);
    float sh = (float)tx_height(c, sf);
    if (sub && *sub && (vy + vh + sh + 6.f) <= y + h)
        tx_clipped(c, sub, x + 18.f, y + h - sh - 7.f, w - 34.f,
                   sf, C_INK_3, AL_L, AV_T);
}

void draw_spark(Canvas *c, float x, float y, float w, float h,
                const float *v, int n, Color col, int fill)
{
    if (n < 2) return;
    float lo = v[0], hi = v[0];
    for (int i = 1; i < n; i++) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
    if (hi - lo < 1e-5f) hi = lo + 1.f;

    Path p; path_reset(&p);
    for (int i = 0; i < n; i++) {
        float px = x + w * i / (n - 1);
        float py = y + h - (v[i] - lo) / (hi - lo) * h;
        if (i == 0) path_move(&p, px, py); else path_line(&p, px, py);
    }
    if (fill) {
        Path f; path_reset(&f);
        path_move(&f, x, y + h);
        for (int i = 0; i < n; i++) {
            float px = x + w * i / (n - 1);
            float py = y + h - (v[i] - lo) / (hi - lo) * h;
            path_line(&f, px, py);
        }
        path_line(&f, x + w, y + h);
        path_close(&f);
        Paint g = paint_linear(x, y, x, y + h, col_alpha(col, .30f),
                               col_alpha(col, .02f));
        cv_fill(c, &f, &g, 1.f);
    }
    cv_stroke_col(c, &p, col, 2.f);
}

const char *ordinal_gate(int gate, char *buf)
{
    if (gate <= 0) { sprintf(buf, "--"); return buf; }
    if (gate >= 21) sprintf(buf, "R%d", gate - 20);
    else            sprintf(buf, "%d", gate);
    return buf;
}

/* ==========================================================================
 *  chrome
 * ========================================================================== */

static void draw_sidebar(App *a, float w, float h)
{
    Canvas *c = &a->cv;
    Paint bg = paint_linear(0, 0, 0, h, C_V900, C_V950);
    cv_rect_p(c, 0, 0, SIDEBAR_W, h, &bg);

    /* a slow aurora behind the brand */
    float t = anim_time();
    for (int i = 0; i < 3; i++) {
        float px = 40.f + sinf(t*0.21f + i*2.1f) * 90.f;
        float py = 120.f + cosf(t*0.17f + i*1.7f) * 90.f + i*40.f;
        cv_glow(c, px, py, 150.f, i == 1 ? C_MAGENTA : C_V500, 0.16f);
    }
    cv_rect(c, SIDEBAR_W - 1.f, 0, 1.f, h, col_alpha(C_V400, .22f));

    tx_backdrop(C_V900);

    /* brand mark */
    float bx = 24.f, by = 30.f;
    draw_logo(c, bx + 20.f, by + 20.f, 16.f);

    tx_draw(c, "AURA", bx + 48.f, by - 2.f, font_track(TF_DISPLAY, 25, TW_BOLD, 3),
            HEX(0xFFFFFF), AL_L, AV_T);
    tx_draw(c, "PLAISANCE OPS", bx + 49.f, by + 26.f,
            font_track(TF_UI, 9, TW_SEMI, 2), col_alpha(C_V300, .95f), AL_L, AV_T);

    /* navigation -- grouped, and scrolled because it no longer fits */
    float navTop = 100.f;
    float navH   = h - 200.f - navTop;
    if (navH < 120.f) navH = 120.f;

    /*  A traveller sees only the passenger screens; the operations floor,
     *  safety, the engines and the administrative record are staff only.
     *  This is the fix for "even travellers are getting access to admin
     *  resources" -- the entries are not merely hidden, they are unreachable,
     *  because the dispatch and the keyboard shortcuts check the same rule. */
    int staff = (a->auth.kind == ACC_STAFF);

    const float IH = 40.f, GAP = 2.f, HDR = 24.f;
    float contentH = 6.f;
    for (int i = 0; i < SC_COUNT; i++) {
        if (!staff && !screen_is_passenger(i)) continue;
        contentH += (NAV[i].section ? HDR : 0.f) + IH + GAP;
    }

    float noff = ui_scroll_begin(uid("navscroll"), 4.f, navTop,
                                 SIDEBAR_W - 8.f, navH, contentH);
    float ny = navTop + 2.f - noff;

    for (int i = 0; i < SC_COUNT; i++) {
        if (!staff && !screen_is_passenger(i)) continue;   /* role filter */
        if (NAV[i].section) {
            if (ny + HDR > navTop && ny < navTop + navH)
                tx_draw_a(c, NAV[i].section, 22.f, ny + HDR*0.5f + 2.f,
                          font_track(TF_UI, 9, TW_BOLD, 2), C_V300,
                          AL_L, AV_M, 0.55f);
            ny += HDR;
        }
        if (ny + IH < navTop || ny > navTop + navH) { ny += IH + GAP; continue; }

        uint64_t id = uidi("nav", i);
        int active = (a->screen == i);
        int hov = ui_hit(10.f, ny, SIDEBAR_W - 20.f, IH);
        float e = ui_hover_f(id, hov);
        float sel = anim_to(id ^ 7ULL, active ? 1.f : 0.f, 14.f);
        if (hov) ui_cursor(1);

        if (sel > 0.01f || e > 0.01f)
            cv_rrect(c, 10.f, ny, SIDEBAR_W - 20.f, IH, 11.f,
                     col_alpha(HEX(0xFFFFFF), 0.06f*e + 0.10f*sel));
        if (sel > 0.01f) {
            cv_rrect(c, 10.f, ny + IH*0.5f - 11.f*sel, 3.f, 22.f*sel, 1.5f,
                     C_V300);
            cv_glow(c, 38.f, ny + IH*0.5f, 30.f, C_V400, 0.26f*sel);
        }
        Color fgc = col_mix(col_alpha(C_V200, .82f), HEX(0xFFFFFF),
                            sel*0.9f + e*0.2f);
        icon_draw(c, NAV[i].icon, 38.f, ny + IH*0.5f, 18.f, fgc);
        tx_draw(c, NAV[i].label, 58.f, ny + IH*0.5f - 7.f,
                font_make(TF_UI, 13, sel > .5f ? TW_SEMI : TW_MED), fgc,
                AL_L, AV_M);
        tx_draw_a(c, NAV[i].sub, 58.f, ny + IH*0.5f + 8.f,
                  font_make(TF_UI, 9, TW_REG), C_V300, AL_L, AV_M,
                  0.55f + sel*0.3f);

        /*  A badge on the screens that are asking for attention, so a gate
         *  clash or a live incident is visible from whichever screen you
         *  happen to be on.                                               */
        int badge = 0;
        if (i == SC_EMERGENCY) badge = emg_active(&a->emg);
        if (i == SC_OPS) {
            GateConflict gc[64];
            badge = ops_gate_conflicts(&a->w, gc, 64);
        }
        if (badge > 0) {
            char bt[8]; snprintf(bt, sizeof bt, "%d", badge > 99 ? 99 : badge);
            cv_circle(c, SIDEBAR_W - 26.f, ny + IH*0.5f, 9.f, C_DANGER);
            tx_backdrop(C_DANGER);
            tx_draw(c, bt, SIDEBAR_W - 26.f, ny + IH*0.5f,
                    font_make(TF_UI, 10, TW_BOLD), HEX(0xFFFFFF), AL_C, AV_M);
            tx_backdrop(C_V950);
        }

        if (hov && a->in.pressed) { a->screen = i; a->screenFade = 0.f; }
        ny += IH + GAP;
    }
    ui_scroll_end();

    /* clock + simulation transport */
    float cy = h - 196.f;
    cv_rrect(c, 14.f, cy, SIDEBAR_W - 28.f, 92.f, 14.f,
             col_alpha(HEX(0x000000), .22f));
    cv_rrect_line(c, 14.f, cy, SIDEBAR_W - 28.f, 92.f, 14.f,
                  col_alpha(C_V400, .22f), 1.f);

    char clk[16]; fmt_hhmmss(a->w.clock, clk);
    tx_backdrop(C_V950);
    tx_draw(c, "LOCAL TIME  UTC+4", 28.f, cy + 12.f,
            font_track(TF_UI, 9, TW_SEMI, 1), col_alpha(C_V300, .8f), AL_L, AV_T);
    tx_draw(c, clk, 28.f, cy + 26.f, font_track(TF_DISPLAY, 27, TW_BOLD, 1),
            HEX(0xFFFFFF), AL_L, AV_T);

    char ds[72];
    snprintf(ds, sizeof ds, "%s %d %s %d", weekday_name(a->w.weekday),
             a->w.day, month_name(a->w.month), a->w.year);
    tx_clipped(c, ds, 28.f, cy + 58.f, SIDEBAR_W - 58.f,
               font_make(TF_UI, 11, TW_MED), col_alpha(C_V200, .85f),
               AL_L, AV_T);

    /*  There is no transport control here, on purpose.  The clock is the
     *  machine's own clock: it cannot be paused, wound on or run at a
     *  multiple of real time, so a pause button would be an outright lie
     *  about what the application does.  What sits here instead is the
     *  indicator that the feed is live.                                    */
    float lp = 0.5f + 0.5f*sinf(anim_time()*2.2f);
    tx_draw(c, "LIVE", SIDEBAR_W - 26.f, cy + 22.f,
            font_track(TF_UI, 9, TW_BOLD, 1), col_alpha(C_V200, .92f),
            AL_R, AV_M);
    cv_circle(c, SIDEBAR_W - 64.f, cy + 22.f, 3.4f + lp*1.4f,
              col_alpha(C_OK, .5f + .5f*lp));

    /* assistant launcher */
    float ay = h - 88.f;
    uint64_t cid = uid("chatbtn");
    int chov = ui_hit(14.f, ay, SIDEBAR_W - 28.f, 50.f);
    float ce = ui_hover_f(cid, chov);
    if (chov) ui_cursor(1);
    Paint cg = paint_linear(14.f, ay, SIDEBAR_W - 14.f, ay + 50.f,
                            col_mix(C_V600, C_V500, ce),
                            col_mix(C_MAGENTA, col_lighten(C_MAGENTA,.15f), ce));
    cv_shadow(c, 14.f, ay + 5.f, SIDEBAR_W - 28.f, 50.f, 14.f, 16.f,
              RGBA(180, 40, 200, 90));
    cv_rrect_p(c, 14.f, ay, SIDEBAR_W - 28.f, 50.f, 14.f, &cg);
    float pulse = 0.5f + 0.5f*sinf(anim_time()*2.2f);
    cv_circle(c, 40.f, ay + 25.f, 13.f, col_alpha(HEX(0xFFFFFF), .16f + .10f*pulse));
    icon_draw(c, IC_SPARK, 40.f, ay + 25.f, 16.f, HEX(0xFFFFFF));
    tx_backdrop(C_V600);
    tx_draw(c, "Ask AURA", 60.f, ay + 17.f, font_make(TF_UI, 14, TW_SEMI),
            HEX(0xFFFFFF), AL_L, AV_T);
    tx_draw_a(c, "assistant", 60.f, ay + 32.f, font_make(TF_UI, 10, TW_REG),
              HEX(0xFFFFFF), AL_L, AV_T, .75f);
    if (chov && a->in.pressed) { a->chatDock = !a->chatDock; ui_capture_mouse(); }

    /* who is signed in, and the way out */
    if (a->auth.name[0]) {
        char who[70];
        snprintf(who, sizeof who, "%s  -  %s", a->auth.name,
                 a->auth.kind == ACC_STAFF ? "staff" : "traveller");
        tx_backdrop(C_V950);
        tx_clipped(c, who, 20.f, h - 30.f, SIDEBAR_W - 74.f,
                   font_make(TF_UI, 10, TW_MED), col_alpha(C_V200, .8f),
                   AL_L, AV_T);
        if (ui_icon_btn(uid("signout"), SIDEBAR_W - 46.f, h - 36.f, 28.f,
                        IC_LOCK, BTN_GHOST)) {
            auth_sign_out(&a->auth);
            a->welcomeMatched = 0; a->selPax = 0;
            store_journal(&a->w, "Signed out");
        }
    } else {
        tx_draw_a(c, "Trois Freres Systems", SIDEBAR_W*0.5f, h - 26.f,
                  font_track(TF_UI, 9, TW_MED, 1), C_V300, AL_C, AV_T, .55f);
    }
    (void)w;
}

static void draw_topbar(App *a, float x, float w)
{
    Canvas *c = &a->cv;
    cv_rect(c, x, 0, w, TOPBAR_H, C_SURF);
    cv_rect(c, x, TOPBAR_H - 1.f, w, 1.f, C_LINE);

    tx_backdrop(C_SURF);
    tx_draw(c, NAV[a->screen].label, x + PAD, 16.f,
            font_make(TF_DISPLAY, 22, TW_SEMI), C_INK, AL_L, AV_T);
    tx_draw(c, "Sir Seewoosagur Ramgoolam International  -  MRU / FIMP",
            x + PAD, 42.f, font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);

    /* weather chip */
    float wx = x + w - 372.f;
    int wicon = a->w.wxKind == 0 ? IC_SUN : (a->w.wxKind <= 2 ? IC_CLOUD : IC_RAIN);
    cv_rrect(c, wx, 15.f, 176.f, 38.f, 10.f, C_SURF_2);
    icon_draw(c, wicon, wx + 21.f, 34.f, 18.f, C_V600);
    char wt[40];
    snprintf(wt, sizeof wt, "%.0f C   %03d/%02dkt", a->w.tempC,
             (int)a->w.windDir, (int)a->w.windKt);
    tx_backdrop(C_SURF_2);
    tx_draw(c, wt, wx + 38.f, 24.f, font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
    char wr[40];
    snprintf(wr, sizeof wr, "RWY %02d  -  %s", a->w.activeRunway, wx_name(a->w.wxKind));
    tx_clipped(c, wr, wx + 38.f, 38.f, 130.f, font_make(TF_UI, 10, TW_REG),
               C_INK_3, AL_L, AV_T);

    /* live movement counters */
    float mx = x + w - 186.f;
    int air = 0;
    for (int i = 0; i < a->w.nFlights; i++) {
        FlightState s = a->w.flight[i].state;
        if (s == FS_TAXI_OUT || s == FS_LINEUP || s == FS_APPROACH ||
            s == FS_LANDED || s == FS_TAXI_IN || s == FS_PUSHBACK) air++;
    }
    cv_rrect(c, mx, 15.f, 82.f, 38.f, 10.f, air ? C_OK_BG : C_SURF_2);
    float pulse = 0.5f + 0.5f*sinf(anim_time()*3.f);
    cv_circle(c, mx + 16.f, 34.f, 4.f + (air ? pulse*1.6f : 0.f),
              air ? C_OK : C_INK_4);
    char ac[16]; snprintf(ac, sizeof ac, "%d", air);
    tx_backdrop(air ? C_OK_BG : C_SURF_2);
    tx_draw(c, ac, mx + 30.f, 24.f, font_make(TF_DISPLAY, 15, TW_BOLD),
            air ? C_OK : C_INK_3, AL_L, AV_T);
    tx_draw(c, "moving", mx + 30.f, 40.f, font_make(TF_UI, 9, TW_MED),
            C_INK_3, AL_L, AV_T);

    if (ui_icon_btn(uid("saveall"), x + w - 90.f, 16.f, 36.f, IC_SAVE, BTN_SOFT)) {
        StoreResult r = store_save_all(&a->w);
        char m[120];
        snprintf(m, sizeof m, "%d records written to %s", r.records, store_root());
        ui_toast(r.ok ? TOAST_OK : TOAST_ERR, "Records saved", m);
        world_log(&a->w, LG_OK, "Operational snapshot written to disk");
    }
    if (ui_icon_btn(uid("helpbtn"), x + w - 48.f, 16.f, 36.f, IC_INFO, BTN_GHOST))
        a->showHelp = !a->showHelp;
}

/* ==========================================================================
 *  chat dock  (implemented in screen_ai.c, reused here as an overlay)
 * ========================================================================== */

static void draw_chat_dock(App *a, float sw, float sh)
{
    float t = anim_to(uid("chatdock"), a->chatDock ? 1.f : 0.f, 14.f);
    if (t < 0.005f) return;

    Canvas *c = &a->cv;
    float w = 404.f, h = sh - TOPBAR_H - 34.f;
    float x = sw - w - 18.f + (1.f - ease_out_cubic(t)) * (w + 30.f);
    float y = TOPBAR_H + 17.f;

    cv_shadow(c, x, y + 8.f, w, h, R_XL, 34.f, RGBA(40, 10, 90, 80));
    cv_rrect(c, x, y, w, h, R_XL, C_SURF);
    cv_rrect_line(c, x, y, w, h, R_XL, C_LINE, 1.f);

    ui_layer_push();
    draw_chat_panel(a, x, y, w, h, 1);
    ui_layer_pop();
}

/* ==========================================================================
 *  help overlay
 * ========================================================================== */

static void draw_help(App *a, float sw, float sh)
{
    float t = anim_to(uid("help"), a->showHelp ? 1.f : 0.f, 15.f);
    if (t < 0.004f) return;
    Canvas *c = &a->cv;
    cv_rect(c, 0, 0, sw, sh, col_alpha(C_V950, 0.55f * t));

    float w = 620.f, h = 524.f;
    float x = (sw - w)*0.5f, y = (sh - h)*0.5f + (1.f - ease_out_back(t))*28.f;
    ui_layer_push();
    cv_shadow(c, x, y + 10.f, w, h, R_XL, 40.f, RGBA(20,5,60,120));
    cv_rrect(c, x, y, w, h, R_XL, C_SURF);

    tx_backdrop(C_SURF);
    tx_draw(c, "AURA", x + 30.f, y + 26.f, font_track(TF_DISPLAY, 26, TW_BOLD, 2),
            C_INK, AL_L, AV_T);
    tx_draw(c, "Airport Unified Resource Administration", x + 30.f, y + 58.f,
            font_make(TF_UI, 13, TW_MED), C_V600, AL_L, AV_T);

    const char *body =
      "A native Windows application written in C for SIS 2075 Software "
      "Engineering 1. Everything on screen is drawn by a software rasteriser "
      "written from scratch -- there is no game engine, no web view and no "
      "third-party graphics library anywhere in the build.\n\n"
      "Five decision-support engines run locally and train when the "
      "application starts: an intent-classifying assistant, a logistic "
      "regression delay model, a multilayer perceptron for hold baggage "
      "screening, a simulated annealing stand allocator, and a Holt "
      "smoothing forecaster feeding an M/M/c queue model.\n\n"
      "The clock is the machine's own clock, shifted to Mauritius time. "
      "Nothing here is played back or fast forwarded: there is no speed "
      "control and no pause, and what the screens show is what the airport "
      "is doing at this moment. The movement programme covers ninety-two "
      "days and is generated from the date, so every day has its own "
      "schedule and the board still works three months out.\n\n"
      "Fifteen screens in five groups: what a passenger needs, what the "
      "operations floor needs, safety, the decision-support engines, and the "
      "administrative record. Gate conflicts and live incidents raise a badge "
      "on the sidebar so they are visible from anywhere.\n\n"
      "Keyboard:  1-9 and 0 reach the first ten screens   A the assistant   "
      "S save all records   L sign out   ESC close panels";
    tx_para(c, body, x + 30.f, y + 92.f, w - 60.f,
            font_make(TF_UI, 13, TW_REG), C_INK_2, 5);

    ui_divider(x + 30.f, y + h - 74.f, w - 60.f);
    tx_draw(c, "Trois Freres Systems Ltd.  -  Plaisance, Mauritius",
            x + 30.f, y + h - 54.f, font_make(TF_UI, 11, TW_MED), C_INK_3,
            AL_L, AV_T);
    if (ui_button(uid("helpclose"), x + w - 130.f, y + h - 62.f, 100.f, 38.f,
                  "Close", BTN_PRIMARY))
        a->showHelp = 0;
    ui_layer_pop();
}

/* ==========================================================================
 *  boot sequence
 * ========================================================================== */

static void draw_boot(App *a, float sw, float sh)
{
    Canvas *c = &a->cv;
    Paint bg = paint_linear(0, 0, sw, sh, C_V950, C_V800);
    cv_rect_p(c, 0, 0, sw, sh, &bg);

    float t = a->bootT;
    float cx = sw*0.5f, cy = sh*0.5f - 30.f;

    /* radar sweep behind the mark */
    for (int i = 0; i < 4; i++) {
        float r = 90.f + i*58.f;
        cv_circle_line(c, cx, cy, r, col_alpha(C_V400, .10f), 1.f);
    }
    float sweep = t * 2.4f;
    for (int i = 0; i < 42; i++) {
        float a0 = sweep - i*0.030f;
        float fade = (1.f - i/42.f);
        cv_line(c, cx, cy, cx + cosf(a0)*270.f, cy + sinf(a0)*270.f,
                col_alpha(C_V300, .16f*fade*fade), 2.4f);
    }
    for (int i = 0; i < 7; i++) {
        float a0 = 0.9f + i*0.87f, rr = 120.f + i*22.f;
        float blip = sinf(t*2.f - i)*0.5f + 0.5f;
        cv_circle(c, cx + cosf(a0)*rr, cy + sinf(a0)*rr, 2.6f,
                  col_alpha(C_MAGENTA, .30f + .55f*blip));
    }

    /* brand */
    float in = ease_out_cubic(cv_clampf(t/0.7f, 0.f, 1.f));
    if (in > 0.12f)
        draw_logo(c, cx - 70.f, cy, 30.f * (0.7f + 0.3f*in));

    tx_backdrop(C_V900);
    tx_draw_a(c, "AURA", cx - 22.f, cy - 34.f,
              font_track(TF_DISPLAY, 62, TW_BOLD, 6), HEX(0xFFFFFF), AL_L, AV_T, in);
    tx_draw_a(c, "AIRPORT UNIFIED RESOURCE ADMINISTRATION", cx, cy + 62.f,
              font_track(TF_UI, 11, TW_SEMI, 4), C_V200, AL_C, AV_T,
              tl_stage(t, 0.45f, 0.5f));
    tx_draw_a(c, "Sir Seewoosagur Ramgoolam International  -  Plaisance, Mauritius",
              cx, cy + 84.f, font_make(TF_UI, 12, TW_REG), C_V300, AL_C, AV_T,
              tl_stage(t, 0.6f, 0.5f) * 0.8f);

    /* what is actually happening, reported honestly */
    const char *steps[] = {
        "Loading airline, aircraft and destination tables",
        "Generating the day's movement programme",
        "Training delay regression by gradient descent",
        "Training baggage classifier by backpropagation",
        "Priming flow forecaster and stand allocator",
        "Operations core ready"
    };
    float sy = cy + 140.f;
    float prog = cv_clampf((t - 0.5f) / 1.85f, 0.f, 1.f);
    int stage = (int)(prog * 6.f);
    if (stage > 5) stage = 5;

    cv_rrect(c, cx - 190.f, sy, 380.f, 5.f, 2.5f, col_alpha(HEX(0xFFFFFF), .14f));
    Paint pg = paint_linear(cx-190.f, sy, cx+190.f, sy, C_V300, C_MAGENTA);
    if (prog > 0.f) cv_rrect_p(c, cx - 190.f, sy, 380.f*prog, 5.f, 2.5f, &pg);

    for (int i = 0; i <= stage && i < 6; i++) {
        float al = i == stage ? 1.f : 0.42f;
        tx_draw_a(c, steps[i], cx, sy + 22.f + (i - stage)*0.f,
                  font_make(TF_UI, 12, i == stage ? TW_SEMI : TW_REG),
                  i == stage ? HEX(0xFFFFFF) : C_V300, AL_C, AV_T, al);
        if (i < stage) break;
    }
    if (stage < 5) ui_spinner(cx, sy + 62.f, 11.f, C_V200);

    tx_draw_a(c, "Trois Freres Systems Ltd.", cx, sh - 46.f,
              font_track(TF_UI, 10, TW_MED, 2), C_V300, AL_C, AV_T, 0.6f);
}

/* ==========================================================================
 *  the passenger queues
 *
 *  Security and boarding are both queues in the textbook sense, and both are
 *  driven here from the live flight programme: passengers join when their
 *  flight reaches the right point in its day, and are called forward at the
 *  rate the desks can actually process them.  Everything about the ordering
 *  lives in queue.c -- this only decides who joins and how fast they leave.
 * ========================================================================== */

static void serve_queue(App *a, PaxQueue *q, float perPaxSec, float stepSec,
                        int markSecurity)
{
    World *wo = &a->w;
    float rate = (float)q->desks * (60.f / perPaxSec);      /* pax per minute */
    q->servedFrac += rate * (stepSec / 60.f);

    while (q->servedFrac >= 1.f) {
        q->servedFrac -= 1.f;
        float wait = 0.f; int pri = 0;
        int pax = pq_next(q, wo->clock, &wait, &pri);
        if (pax < 0) { q->servedFrac = 0.f; break; }
        for (int i = 0; i < wo->nPax; i++)
            if (wo->pax[i].id == pax) {
                if (markSecurity) wo->pax[i].security = 1;
                else              wo->pax[i].boarded  = 1;
                break;
            }
    }
}

static void update_queues(App *a, float dtSec)
{
    World *wo = &a->w;
    a->queueTimer += dtSec;
    if (a->queueTimer < 0.25f) return;
    float step = a->queueTimer;
    a->queueTimer = 0.f;

    for (int i = 0; i < wo->nPax; i++) {
        Passenger *p = &wo->pax[i];
        Flight *f = flight_by_id(wo, p->flight);
        if (!f || f->arrival || f->state == FS_CANCELLED) continue;
        float toGo = (float)f->estMin - wo->clock;

        /*  Security: passengers turn up from about two and a half hours
         *  before departure and stop arriving once the gate is closing.
         *  They trickle in rather than all at once, so the lane fills the
         *  way a real one does.                                          */
        if (!p->queuedSec && toGo < 150.f && toGo > 25.f) {
            wo->rng = wo->rng * 1664525u + 1013904223u;
            if ((wo->rng >> 16) % 100u < 6u) {
                int priority = p->fastTrack || p->assist != AS_NONE;
                if (pq_join(&a->qSecurity, p->id, priority, wo->clock))
                    p->queuedSec = 1;
            }
        }

        /*  Boarding: only once the flight is actually calling passengers,
         *  and only for those who are already through security.          */
        if (!p->queuedGate && p->security &&
            (f->state == FS_BOARDING || f->state == FS_FINAL)) {
            int priority = p->fastTrack || p->assist != AS_NONE || p->loyalty >= 2;
            if (pq_join(&a->qBoarding, p->id, priority, wo->clock))
                p->queuedGate = 1;
        }
    }

    /*  Twelve seconds a passenger through a screening lane, five seconds at
     *  a boarding gate scanner.  Both are near enough what the equipment
     *  actually manages.                                                  */
    serve_queue(a, &a->qSecurity, 12.f, step, 1);
    serve_queue(a, &a->qBoarding,  5.f, step, 0);
}

/* ==========================================================================
 *  frame
 * ========================================================================== */

/*  Midnight.  world_generate_day() has already replaced the movement
 *  programme by the time this runs, so everything that was reasoning about
 *  yesterday's flights is pointed at today's.  The trained weights are kept
 *  -- the models do not need retraining, only re-running.                  */
static void new_operating_day(App *a)
{
    ai_delay_run_all(&a->delay, &a->w);
    vision_init(&a->vision, &a->w);
    ai_flow_observe(&a->flow, &a->w);
    ai_flow_forecast(&a->flow, &a->w);

    /*  The notifier and the queues are about today's flights, so both are
     *  reset rather than carried over -- otherwise the first scan of the new
     *  day announces a gate change for every movement at once.            */
    nt_init(&a->notify);
    nt_scan(&a->notify, &a->w);
    pq_init(&a->qSecurity, "Security screening", 5);
    pq_init(&a->qBoarding, "Boarding gate", 2);
    an_build(&a->report, &a->w);

    a->selFlight = 0;
    a->selBag    = 0;
    a->selPax    = 0;
    a->selStand  = -1;
    a->cvFocus   = -1;
    a->bagFocus  = -1;
    a->inspectOpen = 0;

    store_journal(&a->w, "Operating day rollover");
    char m[110];
    snprintf(m, sizeof m, "%d movements for %s %d %s",
             a->w.nFlights, weekday_name(a->w.weekday), a->w.day,
             month_name(a->w.month));
    ui_toast(TOAST_INFO, "New operating day", m);
}

static void app_frame(App *a)
{
    Canvas *c = &a->cv;
    float sw = (float)c->w, sh = (float)c->h;

    anim_begin_frame(a->dt);
    ui_begin(c, &a->in);

    if (!a->booted) {
        a->bootT += a->dt;
        if (a->bootT > 1.0f && !a->w.nFlights) {
            /* the models really are trained here, on this thread */
            world_init(&a->w);
            store_set_root("data");
            ai_chat_init(&a->chat);
            ai_delay_train(&a->delay, &a->w);
            ai_delay_run_all(&a->delay, &a->w);
            a->bagnet = ai_bagnet();
            ai_bagnet_train(a->bagnet);
            sim_warm_baggage(&a->w, 200.f);
            vision_init(&a->vision, &a->w);
            ai_flow_observe(&a->flow, &a->w);
            ai_flow_forecast(&a->flow, &a->w);
            auth_init(&a->auth, "data");
            emg_init(&a->emg);
            nt_init(&a->notify);
            nt_scan(&a->notify, &a->w);      /* record the baseline, silently */
            rv_init(&a->reviews, &a->w);
            rv_load(&a->reviews, &a->w, "data/reviews.csv");  /* posts survive restarts */
            lp_init(&a->lost, &a->w);
            pq_init(&a->qSecurity, "Security screening", 5);
            pq_init(&a->qBoarding, "Boarding gate", 2);
            an_build(&a->report, &a->w);
            a->rvStars = 5;
            a->rvShowAll = 1;
            store_journal(&a->w, "AURA session started");
        }
        if (a->bootT > 2.6f || (a->in.pressed && a->bootT > 1.4f)) {
            a->booted = 1;
            ui_toast(TOAST_OK, "Operations core ready",
                     "All five engines trained and running locally.");
        }
        draw_boot(a, sw, sh);
        ui_end();
        return;
    }

    long dayBefore = a->w.epochDay;
    sim_tick(&a->w, a->dt);
    if (a->w.epochDay != dayBefore) new_operating_day(a);

    /*  Nothing but the gate is drawn until somebody has signed in.  The
     *  world still ticks behind it, so the clock and the programme are
     *  current the moment the console opens rather than catching up.       */
    if (!auth_is_open(&a->auth)) {
        screen_login(a, sw, sh);
        ui_end();
        return;
    }

    ai_chat_update(&a->w, &a->chat, a->dt);
    update_queues(a, a->dt);

    /*  The notifier watches for gate moves, delays and status changes.  Twice
     *  a second is plenty -- a passenger cannot read faster than that, and
     *  scanning every flight sixty times a second is wasted work.         */
    a->notifyTimer += a->dt;
    if (a->notifyTimer > 0.5f) {
        a->notifyTimer = 0.f;
        nt_scan(&a->notify, &a->w);
    }
    /* the vision pipeline only runs while its screen is open */
    if (a->screen == SC_VISION) vision_tick(&a->vision, &a->w, a->dt);
    ps_update(&a->fx, a->dt, 260.f, 0.9f);

    a->flowTimer += a->dt;
    if (a->flowTimer > 1.4f) {
        a->flowTimer = 0.f;
        ai_flow_observe(&a->flow, &a->w);
        ai_flow_forecast(&a->flow, &a->w);
        ai_delay_run_all(&a->delay, &a->w);
    }

    ui_set_blocking(a->showHelp);

    Paint bg = paint_linear(0, 0, 0, sh, C_BG, C_V50);
    cv_rect_p(c, 0, 0, sw, sh, &bg);

    float cx = SIDEBAR_W, cw = sw - SIDEBAR_W;
    draw_topbar(a, cx, cw);

    /*  A traveller can never land on a staff screen, however they got there
     *  -- a stale selection from a previous session, a keyboard shortcut, or
     *  a role change.  Redirect to their own screen rather than drawing it. */
    if (a->auth.kind != ACC_STAFF && !screen_is_passenger(a->screen))
        a->screen = SC_WELCOME;

    /*  Reviews are reloaded from disk each time the screen is opened, so a
     *  review posted by another account (or in an earlier session) appears
     *  without a restart.  While the screen is open it is not reloaded, so a
     *  review just posted here stays put.                                   */
    if (a->screen != SC_REVIEWS) a->reviewsReloadPending = 1;

    /* screens fade and lift very slightly when switched */
    a->screenFade = anim_to(uid("scfade"), 1.f, 9.f);
    if (a->prevScreen != a->screen) {
        anim_set(uid("scfade"), 0.f);
        a->prevScreen = a->screen;
    }
    float f = ease_out_cubic(a->screenFade);
    float lift = (1.f - f) * 14.f;

    cv_clip_push(c, cx, TOPBAR_H, cw, sh - TOPBAR_H);
    float sy = TOPBAR_H + lift, shh = sh - TOPBAR_H;
    switch (a->screen) {
    case SC_WELCOME:   screen_welcome  (a, cx, sy, cw, shh); break;
    case SC_BOARD:     screen_board    (a, cx, sy, cw, shh); break;
    case SC_SERVICES:  screen_services (a, cx, sy, cw, shh); break;
    case SC_REVIEWS:   screen_reviews  (a, cx, sy, cw, shh); break;
    case SC_OPS:       screen_ops      (a, cx, sy, cw, shh); break;
    case SC_AIRFIELD:  screen_airfield (a, cx, sy, cw, shh); break;
    case SC_CHECKIN:   screen_checkin  (a, cx, sy, cw, shh); break;
    case SC_BAGGAGE:   screen_baggage  (a, cx, sy, cw, shh); break;
    case SC_FLOW:      screen_flow     (a, cx, sy, cw, shh); break;
    case SC_EMERGENCY: screen_emergency(a, cx, sy, cw, shh); break;
    case SC_VISION:    screen_vision   (a, cx, sy, cw, shh); break;
    case SC_AI:        screen_ai       (a, cx, sy, cw, shh); break;
    case SC_ANALYTICS: screen_analytics(a, cx, sy, cw, shh); break;
    case SC_RESOURCES: screen_resources(a, cx, sy, cw, shh); break;
    case SC_RECORDS:   screen_records  (a, cx, sy, cw, shh); break;
    default: break;
    }
    cv_clip_pop(c);

    draw_sidebar(a, sw, sh);
    if (a->screen != SC_AI) draw_chat_dock(a, sw, sh);
    draw_help(a, sw, sh);
    ui_toasts_draw(sw, sh);

    ui_end();
}

/* ==========================================================================
 *  win32 plumbing
 * ========================================================================== */

static void key_shortcuts(App *a, int vk)
{
    /*  Ten keys, fifteen screens.  The digits reach the first ten in sidebar
     *  order; the rest are a click away, which is the honest trade rather
     *  than inventing a second modifier nobody will remember.  A traveller's
     *  digits only reach their own screens -- the role rule is enforced here
     *  too, not only in the sidebar.                                       */
    int target = -1;
    if (vk >= '1' && vk <= '9') target = vk - '1';
    if (vk == '0' && SC_COUNT > 9) target = 9;
    if (target >= 0 && target < SC_COUNT) {
        if (a->auth.kind == ACC_STAFF || screen_is_passenger(target))
            a->screen = target;
        return;
    }
    switch (vk) {
    case 'A': a->chatDock = !a->chatDock; break;
    case 'L':
        auth_sign_out(&a->auth);
        a->welcomeMatched = 0; a->selPax = 0;
        store_journal(&a->w, "Operator signed out");
        ui_toast(TOAST_INFO, "Console locked", "Sign in again to continue.");
        break;
    case 'S': {
        StoreResult r = store_save_all(&a->w);
        char m[120];
        snprintf(m, sizeof m, "%d records written", r.records);
        ui_toast(r.ok ? TOAST_OK : TOAST_ERR, "Records saved", m);
    } break;
    case VK_ESCAPE:
        if (a->showHelp) a->showHelp = 0;
        else if (a->inspectOpen) a->inspectOpen = 0;
        else if (a->chatDock) a->chatDock = 0;
        else ui_focus_clear();
        break;
    default: break;
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    App *a = &g_app;
    switch (m) {
    case WM_DESTROY: g_running = 0; PostQuitMessage(0); return 0;
    case WM_ERASEBKGND: return 1;

    case WM_SIZE: {
        int w = LOWORD(lp), hh = HIWORD(lp);
        if (w > 0 && hh > 0) {
            g_winW = w; g_winH = hh;
            cv_resize(&a->cv, w, hh);
        }
    } return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = 1180;
        mm->ptMinTrackSize.y = 740;
    } return 0;

    case WM_MOUSEMOVE:
        a->in.mx = (float)(short)LOWORD(lp);
        a->in.my = (float)(short)HIWORD(lp);
        return 0;

    case WM_LBUTTONDOWN:
        a->in.down = 1; a->in.pressed = 1;
        SetCapture(h);
        return 0;
    case WM_LBUTTONUP:
        a->in.down = 0; a->in.released = 1;
        ReleaseCapture();
        return 0;
    case WM_LBUTTONDBLCLK: a->in.dbl = 1; return 0;
    case WM_RBUTTONDOWN:   a->in.rdown = 1; a->in.rpressed = 1; return 0;
    case WM_RBUTTONUP:     a->in.rdown = 0; return 0;

    case WM_MOUSEWHEEL:
        a->in.wheel += (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.f;
        return 0;

    case WM_CHAR:
        if (a->in.nchars < 31) a->in.chars[a->in.nchars++] = (unsigned short)wp;
        return 0;

    case WM_KEYDOWN: {
        int vk = (int)wp;
        if (vk < 256) { a->in.keyHit[vk] = 1; a->in.keyDown[vk] = 1; }
        a->in.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        a->in.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        /*  Shortcuts are dead while the console is locked, and while ANY
         *  text field has the caret.  Enumerating fields by name was the
         *  old way and it was a bug waiting to happen: the review box and
         *  every field added since were not on the list, so typing a review
         *  that contained an "l" fired the sign-out shortcut and threw the
         *  passenger back to the login screen.  One check covers them all. */
        if (auth_is_open(&a->auth) && !ui_any_field_focused())
            key_shortcuts(a, vk);
    } return 0;

    case WM_KEYUP:
        if ((int)wp < 256) a->in.keyDown[(int)wp] = 0;
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, ui_cursor_hand() ? IDC_HAND : IDC_ARROW));
            return 1;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        cv_present(&a->cv, dc, 0, 0);
        EndPaint(h, &ps);
    } return 0;
    }
    return DefWindowProc(h, m, wp, lp);
}

/*  The window icon -- the little picture in the taskbar and the title bar.
 *
 *  It is not a .ico file on disk: the brief rules out external assets and
 *  libraries, so the icon is drawn at run time by the same rasteriser that
 *  draws everything else, into an off-screen buffer, and handed to Windows as
 *  a bitmap.  The badge gives the colour; a second pass draws the same badge
 *  shape in white on black to get a smooth, anti-aliased rounded-corner alpha
 *  mask, so the icon has clean transparent corners instead of a hard square.
 * ------------------------------------------------------------------------- */
static void icon_badge(Canvas *ic, int sz)
{
    float s = sz*0.5f, cx = s, cy = s;
    float m  = (float)sz * 0.045f;
    float bs = s - m;                         /* badge half-extent           */
    float rad = (float)sz * 0.28f;

    Paint g = paint_linear(cx - bs, cy - bs, cx + bs, cy + bs, C_V500, C_MAGENTA);
    cv_rrect_p(ic, cx - bs, cy - bs, bs*2.f, bs*2.f, rad, &g);
    cv_glow(ic, cx - bs*0.4f, cy - bs*0.45f, bs*1.1f, HEX(0xFFFFFF), 0.10f);

    float r = bs * 0.66f;
    Path body; path_reset(&body);
    path_move (&body, cx - r*0.62f, cy + r*0.10f);
    path_line (&body, cx + r*0.66f, cy - r*0.60f);
    path_line (&body, cx + r*0.02f, cy + r*0.66f);
    path_line (&body, cx - r*0.10f, cy + r*0.12f);
    path_close(&body);
    cv_fill_col(ic, &body, HEX(0xFFFFFF));
    Path fold; path_reset(&fold);
    path_move (&fold, cx - r*0.10f, cy + r*0.12f);
    path_line (&fold, cx + r*0.02f, cy + r*0.66f);
    path_line (&fold, cx + r*0.66f, cy - r*0.60f);
    path_close(&fold);
    cv_fill_col(ic, &fold, col_alpha(C_V700, .32f));
}

static HICON make_app_icon(int sz)
{
    Canvas col, msk;
    if (!cv_create(&col, sz, sz)) return NULL;
    if (!cv_create(&msk, sz, sz)) { cv_destroy(&col); return NULL; }

    /* colour pass on the brand purple, so any anti-aliased edge blends toward
     * the badge rather than fringing against a key colour */
    cv_clear(&col, C_V600);
    icon_badge(&col, sz);

    /* alpha pass: the same rounded-square shape, white on black */
    cv_clear(&msk, HEX(0x000000));
    {
        float s = sz*0.5f, m = (float)sz*0.045f, bs = s - m, rad = (float)sz*0.28f;
        cv_rrect(&msk, m, m, bs*2.f, bs*2.f, rad, HEX(0xFFFFFF));
    }
    cv_sync(&col); cv_sync(&msk);

    /* a 32-bit top-down ARGB DIB, alpha taken from the mask's luminance */
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = sz;
    bi.bmiHeader.biHeight      = -sz;
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HDC screen = GetDC(NULL);
    HBITMAP colorBmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!colorBmp) { cv_destroy(&col); cv_destroy(&msk); return NULL; }

    uint32_t *dst = (uint32_t *)bits;
    for (int i = 0; i < sz*sz; i++) {
        uint32_t c8 = col.px[i];              /* 0xFFrrggbb from the canvas   */
        uint32_t a  = msk.px[i] & 0xFFu;       /* mask luminance -> alpha      */
        /* premultiply so the corners are clean under the icon compositor */
        uint32_t rr = (((c8 >> 16) & 0xFFu) * a) / 255u;
        uint32_t gg = (((c8 >> 8)  & 0xFFu) * a) / 255u;
        uint32_t bb = (( c8        & 0xFFu) * a) / 255u;
        dst[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
    }

    /* a mono AND mask is still required; the alpha channel does the real work */
    HBITMAP maskBmp = CreateBitmap(sz, sz, 1, 1, NULL);

    ICONINFO ii;
    memset(&ii, 0, sizeof ii);
    ii.fIcon    = TRUE;
    ii.hbmColor = colorBmp;
    ii.hbmMask  = maskBmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(colorBmp);
    DeleteObject(maskBmp);
    cv_destroy(&col);
    cv_destroy(&msk);
    return icon;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev; (void)cmd;
    SetProcessDPIAware();

    /*  Big for the taskbar and Alt-Tab, small for the title-bar corner.  */
    HICON iconBig = make_app_icon(64);
    HICON iconSm  = make_app_icon(32);

    WNDCLASSEX wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize        = sizeof wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_OWNDC;
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = hInst;
    wc.hIcon         = iconBig;
    wc.hIconSm       = iconSm;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = "AURA_MRU";
    RegisterClassEx(&wc);

    RECT r = { 0, 0, g_winW, g_winH };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
    if (sy < 0) sy = 0;

    g_hwnd = CreateWindowEx(0, "AURA_MRU",
        "AURA  -  Airport Unified Resource Administration  |  MRU Plaisance",
        WS_OVERLAPPEDWINDOW, sx, sy, ww, wh, NULL, NULL, hInst, NULL);
    if (!g_hwnd) return 1;

    /* the class icon is usually enough, but set it on the window too so the
     * taskbar button and Alt-Tab both pick up the AURA mark immediately */
    if (iconBig) SendMessage(g_hwnd, WM_SETICON, ICON_BIG,   (LPARAM)iconBig);
    if (iconSm)  SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSm);

    memset(&g_app, 0, sizeof g_app);
    tx_init();
    if (!cv_create(&g_app.cv, g_winW, g_winH)) return 1;
    ps_init(&g_app.fx, 0xC0FFEEu);
    g_app.fieldZoom = 1.f;
    g_app.fieldLabels = 1;
    g_app.fieldTrails = 1;
    g_app.bagAutoInject = 1;
    g_app.bagIdentify   = 1;
    g_app.screen = SC_WELCOME;
    g_app.prevScreen = -1;
    g_app.selStand = -1;
    g_app.cvFocus  = -1;
    g_app.cvIds = g_app.cvTrails = g_app.cvHeatmap = 1;

    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    LARGE_INTEGER freq, prevT;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prevT);

    HDC dc = GetDC(g_hwnd);
    MSG msg;
    while (g_running) {
        in_new_frame(&g_app.in);
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!g_running) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - prevT.QuadPart) / (float)freq.QuadPart;
        prevT = now;
        if (dt > 0.08f) dt = 0.08f;
        g_app.dt = dt;

        app_frame(&g_app);
        cv_present(&g_app.cv, dc, 0, 0);

        /* hold roughly 60 frames a second without spinning a core flat */
        LARGE_INTEGER after;
        QueryPerformanceCounter(&after);
        float used = (float)(after.QuadPart - now.QuadPart) / (float)freq.QuadPart;
        float budget = 1.f/60.f;
        if (used < budget) {
            int ms = (int)((budget - used) * 1000.f);
            if (ms > 0) Sleep((DWORD)ms);
        }
    }

    ReleaseDC(g_hwnd, dc);
    tx_shutdown();
    cv_destroy(&g_app.cv);
    return 0;
}
