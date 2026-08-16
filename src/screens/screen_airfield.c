/* ==========================================================================
 *  AURA :: screen_airfield.c   --   the live airfield
 *
 *  A top-down movement display of Plaisance drawn entirely from the geometry
 *  in model.c: one runway 14/32, the parallel taxiway, the link taxiways, the
 *  terminal frontage and eighteen stands.  Aircraft are placed by sampling
 *  the same taxi routings the simulation drives them along, so what you see
 *  is the state of the operation rather than an animation playing over it.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* map transform ----------------------------------------------------------- */
static float g_mx, g_my, g_ms;

static float FX(float x) { return g_mx + x * g_ms; }
static float FY(float y) { return g_my + y * g_ms; }

static void map_setup(App *a, float x, float y, float w, float h)
{
    float sx = w / FIELD_W, sy = h / FIELD_H;
    g_ms = (sx < sy ? sx : sy) * a->fieldZoom;
    g_mx = x + (w - FIELD_W * g_ms) * 0.5f + a->fieldPanX;
    g_my = y + (h - FIELD_H * g_ms) * 0.5f + a->fieldPanY;
}

/* --------------------------------------------------------------------------
 *  paved surfaces
 * ------------------------------------------------------------------------- */

static void draw_surfaces(App *a, Canvas *c)
{
    World *w = &a->w;
    float rx0, ry0, rx1, ry1;
    field_runway_ends(w, &rx0, &ry0, &rx1, &ry1);

    float dx = rx1 - rx0, dy = ry1 - ry0;
    float L = sqrtf(dx*dx + dy*dy);
    float ux = dx/L, uy = dy/L;
    float nx = -uy, ny = ux;

    /* --- infield: the grass between the runway strip and the apron ------ */
    Path inf; path_reset(&inf);
    path_move(&inf, FX(40.f),  FY(ry0 + ny*30.f - 6.f));
    path_line(&inf, FX(985.f), FY(ry1 + ny*30.f - 6.f));
    path_line(&inf, FX(985.f), FY(638.f));
    path_line(&inf, FX(40.f),  FY(638.f));
    path_close(&inf);
    cv_fill_col(c, &inf, HEX(0x1A0D36));

    /* --- apron slab: everything from the taxiway down to the terminal ---- */
    Path ap; path_reset(&ap);
    path_move(&ap, FX(28.f + nx*78.f),  FY(ry0 + ny*78.f));
    path_line(&ap, FX(1000.f),          FY(ry1 + ny*78.f));
    path_line(&ap, FX(1000.f),          FY(634.f));
    path_line(&ap, FX(28.f),            FY(634.f));
    path_close(&ap);
    cv_fill_col(c, &ap, HEX(0x241147));

    /* apron edge line */
    cv_line(c, FX(46.f + nx*78.f), FY(ry0 + ny*78.f),
               FX(980.f), FY(ry1 + ny*78.f), col_alpha(C_V400, .18f), 1.2f);

    /* --- link taxiways: right-angled stubs onto the parallel taxiway ---- */
    for (int i = 0; i < w->nStands; i++) {
        Stand *s = &w->stand[i];
        if (s->kind == ST_CARGO) continue;
        Path lk; path_reset(&lk);
        path_move(&lk, FX(s->entryX), FY(s->entryY));
        path_line(&lk, FX(s->x), FY(s->y - 30.f));
        path_line(&lk, FX(s->x), FY(s->y));
        /* paved strip, then a thin lead-in line: reads as ground marking
         * rather than as a bright bar across the apron */
        cv_stroke_col(c, &lk, HEX(0x2A1552), 11.f * g_ms);
        cv_stroke_col(c, &lk, col_alpha(C_GOLD, .20f), 1.f);
    }

    /* --- parallel taxiway ---------------------------------------------- */
    float tw = 15.f;
    Path tx; path_reset(&tx);
    path_move(&tx, FX(rx0 + nx*100.f - ux*40.f), FY(ry0 + ny*100.f - uy*40.f));
    path_line(&tx, FX(rx1 + nx*100.f + ux*40.f), FY(ry1 + ny*100.f + uy*40.f));
    cv_stroke_col(c, &tx, HEX(0x33195F), tw * g_ms);

    /* taxiway centreline */
    Path tc; path_reset(&tc);
    path_move(&tc, FX(rx0 + nx*100.f - ux*40.f), FY(ry0 + ny*100.f - uy*40.f));
    path_line(&tc, FX(rx1 + nx*100.f + ux*40.f), FY(ry1 + ny*100.f + uy*40.f));
    cv_stroke_col(c, &tc, col_alpha(C_GOLD, .45f), 1.5f);
    tx_backdrop(C_NIGHT);
    if (g_ms > 0.5f)
        tx_draw(c, "TWY ALPHA", FX(rx0 + nx*100.f + ux*120.f) ,
                FY(ry0 + ny*100.f + uy*120.f) - 12.f,
                font_track(TF_UI, 9, TW_BOLD, 2), col_alpha(C_GOLD, .55f),
                AL_L, AV_M);

    /* --- runway --------------------------------------------------------- */
    float rw = 26.f;
    Path rp; path_reset(&rp);
    path_move(&rp, FX(rx0 + nx*rw), FY(ry0 + ny*rw));
    path_line(&rp, FX(rx1 + nx*rw), FY(ry1 + ny*rw));
    path_line(&rp, FX(rx1 - nx*rw), FY(ry1 - ny*rw));
    path_line(&rp, FX(rx0 - nx*rw), FY(ry0 - ny*rw));
    path_close(&rp);
    cv_fill_col(c, &rp, HEX(0x150A2E));
    cv_stroke_col(c, &rp, col_alpha(HEX(0xFFFFFF), .16f), 1.4f);

    /* centreline dashes */
    int dashes = 26;
    for (int i = 0; i < dashes; i++) {
        float t0 = (i + 0.22f)/dashes, t1 = (i + 0.78f)/dashes;
        cv_line(c, FX(rx0 + dx*t0), FY(ry0 + dy*t0),
                   FX(rx0 + dx*t1), FY(ry0 + dy*t1),
                col_alpha(HEX(0xFFFFFF), .40f), 1.8f);
    }
    /* threshold bars at both ends */
    for (int end = 0; end < 2; end++) {
        float base = end ? 0.985f : 0.015f;
        for (int k = -3; k <= 3; k++) {
            float off = k * 6.2f;
            float bx = rx0 + dx*base + nx*off, by = ry0 + dy*base + ny*off;
            cv_line(c, FX(bx - ux*13.f), FY(by - uy*13.f),
                       FX(bx + ux*13.f), FY(by + uy*13.f),
                    col_alpha(HEX(0xFFFFFF), .55f), 2.2f);
        }
    }
    /* runway designators */
    tx_backdrop(C_NIGHT);
    float d0x = FX(rx0 + dx*0.055f), d0y = FY(ry0 + dy*0.055f);
    float d1x = FX(rx0 + dx*0.945f), d1y = FY(ry0 + dy*0.945f);
    Font rf = font_track(TF_DISPLAY, (int)(13.f*g_ms > 9.f ? 13.f*g_ms : 9.f),
                         TW_BOLD, 1);
    tx_draw(c, "14", d0x, d0y, rf, col_alpha(HEX(0xFFFFFF), .72f), AL_C, AV_M);
    tx_draw(c, "32", d1x, d1y, rf, col_alpha(HEX(0xFFFFFF), .72f), AL_C, AV_M);

    /* runway edge lights, brighter on the threshold in use */
    for (int i = 0; i <= 30; i++) {
        float t = (float)i/30.f;
        for (int s = -1; s <= 1; s += 2) {
            float ex = rx0 + dx*t + nx*rw*s, ey = ry0 + dy*t + ny*rw*s;
            float blink = 0.75f + 0.25f*sinf(anim_time()*2.f + i*0.4f);
            cv_circle(c, FX(ex), FY(ey), 1.5f, col_alpha(C_GOLD, .55f*blink));
        }
    }
    float apEnd = (w->activeRunway == 14) ? 0.f : 1.f;
    for (int k = 0; k < 6; k++) {
        float t = apEnd + (apEnd > 0.5f ? 1.f : -1.f) * (0.02f + k*0.012f);
        float ex = rx0 + dx*t, ey = ry0 + dy*t;
        float ph = sinf(anim_time()*6.f - k*0.6f);
        cv_glow(c, FX(ex), FY(ey), 9.f, HEX(0xFFFFFF), ph > 0.4f ? .55f : .08f);
    }
}

/* --------------------------------------------------------------------------
 *  terminal and stands
 * ------------------------------------------------------------------------- */

static void draw_terminal(App *a, Canvas *c)
{
    World *w = &a->w;

    /* the terminal block with its curved landside frontage */
    Path t; path_reset(&t);
    path_move(&t, FX(238.f), FY(528.f));
    path_line(&t, FX(812.f), FY(528.f));
    path_cubic(&t, FX(846.f), FY(534.f), FX(858.f), FY(556.f),
                   FX(852.f), FY(586.f));
    path_line(&t, FX(198.f), FY(586.f));
    path_cubic(&t, FX(192.f), FY(556.f), FX(204.f), FY(534.f),
                   FX(238.f), FY(528.f));
    path_close(&t);
    Paint tg = paint_linear(FX(200.f), FY(528.f), FX(200.f), FY(586.f),
                            HEX(0x3A1E72), HEX(0x2A1355));
    cv_fill(c, &t, &tg, 1.f);
    cv_stroke_col(c, &t, col_alpha(C_V300, .30f), 1.4f);

    /* the roof ribs give it the scale of a real building */
    for (int i = 0; i < 22; i++) {
        float x = 240.f + i*26.f;
        if (x > 830.f) break;
        cv_line(c, FX(x), FY(530.f), FX(x), FY(584.f),
                col_alpha(C_V400, .16f), 1.f);
    }
    tx_backdrop(HEX(0x30186A));
    if (g_ms > 0.55f)
        tx_draw(c, "PASSENGER TERMINAL", FX(525.f), FY(560.f),
                font_track(TF_UI, (int)(8.f + g_ms*3.f), TW_SEMI, 3),
                col_alpha(C_V200, .75f), AL_C, AV_M);

    /* stands */
    for (int i = 0; i < w->nStands; i++) {
        Stand *s = &w->stand[i];
        int sel = (a->selStand == i);
        float sx = FX(s->x), sy = FY(s->y);

        /* who is on it right now? */
        Flight *occ = NULL;
        for (int k = 0; k < w->nFlights; k++) {
            Flight *f = &w->flight[k];
            if (f->stand != i) continue;
            float d = (float)f->estMin - w->clock;
            if (f->arrival  && d < 6.f  && d > -95.f) { occ = f; break; }
            if (!f->arrival && d > -3.f && d < 105.f) { occ = f; break; }
        }

        Color mark = occ ? col_alpha(C_MAGENTA, .55f) : col_alpha(C_V300, .28f);
        float box = (s->kind == ST_CONTACT ? 30.f : 24.f) * g_ms;

        /* stand marking: a T shape, as painted on a real apron */
        cv_line(c, sx - box*0.5f, sy + box*0.42f, sx + box*0.5f, sy + box*0.42f,
                mark, 1.6f);
        cv_line(c, sx, sy - box*0.5f, sx, sy + box*0.42f, mark, 1.6f);

        if (sel) {
            cv_circle_line(c, sx, sy, box*0.86f, C_V300, 1.8f);
            cv_glow(c, sx, sy, box*1.5f, C_V400, .30f);
        }

        /* airbridge stub for contact stands */
        if (s->kind == ST_CONTACT) {
            cv_line(c, sx, sy + box*0.5f, sx, FY(528.f),
                    col_alpha(C_V400, occ ? .55f : .22f), 3.2f * g_ms);
            if (occ)
                cv_circle(c, sx, FY(528.f), 3.f*g_ms, col_alpha(C_V300, .8f));
        }

        if (a->fieldLabels && g_ms > 0.5f) {
            tx_backdrop(C_NIGHT);
            tx_draw(c, s->name, sx, sy + box*0.72f,
                    font_track(TF_UI, (int)(7.f + g_ms*3.4f), TW_BOLD, 1),
                    occ ? col_alpha(C_V200, .95f) : col_alpha(C_V300, .55f),
                    AL_C, AV_T);
        }

        /* click target */
        if (ui_hit(sx - box, sy - box, box*2.f, box*2.f)) {
            ui_cursor(1);
            if (a->in.pressed) {
                a->selStand = (a->selStand == i) ? -1 : i;
                if (occ) a->selFlight = occ->id;
            }
        }
    }
}

/* --------------------------------------------------------------------------
 *  ground vehicles -- small, but they are what makes an apron feel alive
 * ------------------------------------------------------------------------- */

static void draw_vehicles(App *a, Canvas *c)
{
    float t = anim_time();
    for (int i = 0; i < 9; i++) {
        float phase = t*0.055f + i*0.37f;
        float u = phase - floorf(phase);
        float lane = 424.f + (i % 3) * 9.f;
        float x = 90.f + u * 860.f;
        if (i & 1) x = 950.f - u * 860.f;
        float y = lane + sinf(t*0.7f + i)*2.f;
        Color col = (i % 3 == 0) ? C_GOLD : (i % 3 == 1 ? C_V300 : C_TEAL);
        float vs = 3.4f * g_ms;
        cv_rrect(c, FX(x) - vs, FY(y) - vs*0.55f, vs*2.f, vs*1.1f, vs*0.4f,
                 col_alpha(col, .75f));
        if ((int)(t*4.f + i) & 1)
            cv_circle(c, FX(x) + ((i&1)? -vs : vs), FY(y), 1.4f,
                      col_alpha(C_GOLD, .9f));
    }
}

/* --------------------------------------------------------------------------
 *  aircraft
 * ------------------------------------------------------------------------- */

static void draw_aircraft_layer(App *a, Canvas *c)
{
    World *w = &a->w;
    float pts[64];
    int   n;

    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (f->stand < 0) continue;

        float t = sim_flight_path(w, f, pts, 32, &n);
        float px, py, hd;
        int moving = (t >= 0.f && n >= 2);

        if (moving) {
            path_sample(pts, n, t, &px, &py, &hd);
        } else {
            /* parked, if this movement is at its stand right now */
            float d = (float)f->estMin - w->clock;
            int parked = f->arrival ? (d < -8.f && d > -95.f)
                                    : (d > 0.f && d < 100.f);
            if (!parked) continue;
            Stand *s = &w->stand[f->stand];
            px = s->x; py = s->y; hd = s->heading;
        }

        float span = w->actype[f->acType].wingspan * 1.05f * g_ms;
        if (span < 11.f) span = 11.f;
        Color body = HEX(0xE9E4F7);
        Color acc  = w->airline[f->airline].col;
        int sel = (a->selFlight == f->id);

        /* the routing this aircraft is following */
        if (moving && a->fieldTrails) {
            Path tr; path_reset(&tr);
            for (int k = 0; k < n; k++) path_line(&tr, FX(pts[k*2]), FY(pts[k*2+1]));
            cv_stroke_col(c, &tr, col_alpha(sel ? C_V300 : C_V500, sel ? .40f : .16f),
                          sel ? 2.2f : 1.4f);
        }

        /* jet wash / touchdown smoke */
        if (moving && t > 0.02f && t < 0.99f) {
            float back = t - 0.012f;
            if (back > 0.f) {
                float bx, by, bh;
                path_sample(pts, n, back, &bx, &by, &bh);
                cv_glow(c, FX(bx), FY(by), span*0.55f, C_V400, .12f);
            }
        }

        if (sel) cv_glow(c, FX(px), FY(py), span*1.5f, C_V300, .34f);
        cv_shadow(c, FX(px) - span*0.5f, FY(py) - span*0.28f + 3.f,
                  span, span*0.56f, span*0.28f, 7.f, RGBA(0,0,0,70));

        draw_aircraft(c, FX(px), FY(py), hd, span, body, acc,
                      moving && f->strobe, g_ms);

        if (a->fieldLabels && g_ms > 0.45f) {
            char lbl[40];
            snprintf(lbl, sizeof lbl, "%s", f->no);
            float ly = FY(py) - span*0.62f - 9.f;
            Font lf = font_track(TF_UI, (int)(8.f + g_ms*2.6f), TW_BOLD, 0);
            int lw = tx_width(c, lbl, lf) + 10;
            cv_rrect(c, FX(px) - lw*0.5f, ly - 8.f, (float)lw, 15.f, 4.f,
                     col_alpha(sel ? C_V600 : C_NIGHT_3, sel ? .95f : .78f));
            tx_backdrop(sel ? C_V600 : C_NIGHT_3);
            tx_draw(c, lbl, FX(px), ly - 1.f, lf,
                    sel ? HEX(0xFFFFFF) : col_alpha(C_V200, .95f), AL_C, AV_M);
        }

        float hitR = span*0.6f + 8.f;
        if (ui_hit(FX(px)-hitR, FY(py)-hitR, hitR*2.f, hitR*2.f)) {
            ui_cursor(1);
            if (a->in.pressed) { a->selFlight = f->id; a->selStand = f->stand; }
        }
    }
}

/* --------------------------------------------------------------------------
 *  overlays: compass, wind, scale
 * ------------------------------------------------------------------------- */

static void draw_map_overlays(App *a, Canvas *c, float x, float y, float w, float h)
{
    World *wo = &a->w;

    /* compass rose */
    float cx = x + w - 62.f, cy = y + 62.f;
    cv_circle(c, cx, cy, 30.f, col_alpha(C_NIGHT, .62f));
    cv_circle_line(c, cx, cy, 30.f, col_alpha(C_V400, .35f), 1.f);
    for (int i = 0; i < 8; i++) {
        float ang = (float)(i * M_PI / 4.0) - 1.5708f;
        float r0 = i % 2 ? 22.f : 18.f;
        cv_line(c, cx + cosf(ang)*r0, cy + sinf(ang)*r0,
                   cx + cosf(ang)*28.f, cy + sinf(ang)*28.f,
                col_alpha(C_V300, i % 2 ? .3f : .6f), 1.2f);
    }
    cv_tri(c, cx, cy - 22.f, cx - 5.f, cy + 5.f, cx + 5.f, cy + 5.f, C_DANGER);
    tx_backdrop(C_NIGHT);
    tx_draw(c, "N", cx, cy - 38.f, font_make(TF_UI, 10, TW_BOLD),
            col_alpha(C_V200, .9f), AL_C, AV_M);

    /* wind arrow */
    float wx = x + w - 62.f, wy = y + 148.f;
    cv_circle(c, wx, wy, 30.f, col_alpha(C_NIGHT, .62f));
    cv_circle_line(c, wx, wy, 30.f, col_alpha(C_V400, .35f), 1.f);
    float wa = (wo->windDir + 180.f) * (float)M_PI / 180.f - 1.5708f;
    float wl = 12.f + wo->windKt*0.7f;
    if (wl > 26.f) wl = 26.f;
    cv_line_round(c, wx - cosf(wa)*wl*0.6f, wy - sinf(wa)*wl*0.6f,
                     wx + cosf(wa)*wl, wy + sinf(wa)*wl, C_TEAL, 3.f);
    cv_tri(c, wx + cosf(wa)*(wl+7.f), wy + sinf(wa)*(wl+7.f),
              wx + cosf(wa+2.5f)*8.f, wy + sinf(wa+2.5f)*8.f,
              wx + cosf(wa-2.5f)*8.f, wy + sinf(wa-2.5f)*8.f, C_TEAL);
    char wtxt[24]; snprintf(wtxt, sizeof wtxt, "%02dkt", (int)wo->windKt);
    tx_draw(c, wtxt, wx, wy + 42.f, font_make(TF_UI, 10, TW_SEMI),
            col_alpha(C_V200, .9f), AL_C, AV_M);

    /* scale bar: 500 m of runway is a known length */
    float mPerUnit = 3390.f / 810.f;
    float px500 = (500.f / mPerUnit) * g_ms;
    float bx = x + 22.f, by = y + h - 28.f;
    cv_line(c, bx, by, bx + px500, by, col_alpha(C_V200, .7f), 2.f);
    cv_line(c, bx, by - 4.f, bx, by + 4.f, col_alpha(C_V200, .7f), 2.f);
    cv_line(c, bx + px500, by - 4.f, bx + px500, by + 4.f, col_alpha(C_V200, .7f), 2.f);
    tx_draw(c, "500 m", bx + px500*0.5f, by - 14.f, font_make(TF_UI, 10, TW_MED),
            col_alpha(C_V200, .7f), AL_C, AV_M);
}

/* ==========================================================================
 *  right hand panel
 * ========================================================================== */

static void draw_movement_panel(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "LIVE MOVEMENTS", x + 18.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    int moving = 0;
    for (int i = 0; i < wo->nFlights; i++) {
        FlightState s = wo->flight[i].state;
        if (s >= FS_PUSHBACK && s <= FS_LINEUP) moving++;
        if (s == FS_APPROACH || s == FS_LANDED || s == FS_TAXI_IN) moving++;
    }
    char cnt[24]; snprintf(cnt, sizeof cnt, "%d active", moving);
    tx_draw(c, cnt, x + w - 18.f, y + 16.f, font_make(TF_UI, 11, TW_SEMI),
            C_INK_3, AL_R, AV_T);

    float ly = y + 42.f;
    float listH = h - 50.f;

    /* collect anything that is airborne or on the move, plus what is next */
    int idx[26], n = 0;
    for (int i = 0; i < wo->nFlights && n < 26; i++) {
        Flight *f = &wo->flight[i];
        float d = fabsf((float)f->estMin - wo->clock);
        int active = (f->state >= FS_PUSHBACK && f->state <= FS_TAXI_IN);
        if (active || d < 70.f) idx[n++] = i;
    }
    /* nearest first */
    for (int i = 1; i < n; i++) {
        int k = idx[i], j = i - 1;
        float dk = fabsf((float)wo->flight[k].estMin - wo->clock);
        while (j >= 0 && fabsf((float)wo->flight[idx[j]].estMin - wo->clock) > dk) {
            idx[j+1] = idx[j]; j--;
        }
        idx[j+1] = k;
    }

    float rowH = 54.f;
    float off = ui_scroll_begin(uid("afmov"), x + 6.f, ly, w - 12.f, listH,
                                n * rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Flight *f = &wo->flight[idx[i]];
        float ry = ly + i*rowH - off;
        if (ry > ly + listH || ry + rowH < ly) continue;
        draw_flight_row(a, x + 10.f, ry, w - 24.f, rowH - 6.f, f,
                        a->selFlight == f->id, 0);
    }
    ui_scroll_end();
}

static void draw_selected_card(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    Flight *f = a->selFlight ? flight_by_id(wo, a->selFlight) : NULL;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);

    if (!f) {
        icon_draw(c, IC_PLANE, x + w*0.5f, y + h*0.5f - 16.f, 30.f, C_INK_4);
        tx_draw(c, "Select an aircraft or stand", x + w*0.5f, y + h*0.5f + 12.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
        return;
    }

    Color ac = wo->airline[f->airline].col;
    Paint g = paint_linear(x, y, x + w, y, col_alpha(ac, .13f), col_alpha(ac, .02f));
    cv_rrect_p(c, x, y, w, 56.f, R_LG, &g);
    cv_rrect(c, x, y, 4.f, 56.f, 2.f, ac);

    tx_draw(c, f->no, x + 18.f, y + 12.f, font_track(TF_DISPLAY, 22, TW_BOLD, 1),
            C_INK, AL_L, AV_T);
    char sub[80];
    snprintf(sub, sizeof sub, "%s  %s  %s", wo->airline[f->airline].iata,
             wo->actype[f->acType].code, f->reg);
    tx_draw(c, sub, x + 18.f, y + 38.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_L, AV_T);

    Color sc = fs_color(f->state);
    ui_pill(x + w - 18.f - (float)(tx_width(c, fs_name(f->state),
            font_make(TF_UI,12,TW_SEMI)) + 26), y + 16.f, 26.f,
            fs_name(f->state), sc, col_alpha(sc, .14f));

    float ry = y + 70.f;
    char sch[8], est[8];
    fmt_hhmm(f->schedMin, sch);
    fmt_hhmm(f->estMin, est);

    struct { const char *k; char v[42]; } rows[6];
    int nr = 0;
    snprintf(rows[nr].v, 42, "%s %s", f->arrival ? "from" : "to",
             wo->airport[f->airport].city);
    rows[nr++].k = "Route";
    snprintf(rows[nr].v, 42, "%s   est %s%s", sch, est,
             f->delayMin ? "  (late)" : "");
    rows[nr++].k = "Times";
    snprintf(rows[nr].v, 42, "%s%s", f->stand >= 0 ? wo->stand[f->stand].name : "--",
             f->stand >= 0 && wo->stand[f->stand].kind == ST_CONTACT
             ? "  (contact)" : "  (remote)");
    rows[nr++].k = "Stand";
    snprintf(rows[nr].v, 42, "%d of %d seats", f->pax, f->paxCap);
    rows[nr++].k = "Load";
    if (!f->arrival) {
        snprintf(rows[nr].v, 42, "%d checked in, %d loaded", f->checkedIn,
                 f->bagsLoaded);
        rows[nr++].k = "Progress";
    } else {
        snprintf(rows[nr].v, 42, "belt %s", f->belt);
        rows[nr++].k = "Reclaim";
    }

    for (int i = 0; i < nr; i++) {
        tx_draw(c, rows[i].k, x + 18.f, ry, font_make(TF_UI, 11, TW_MED),
                C_INK_3, AL_L, AV_T);
        tx_clipped(c, rows[i].v, x + 88.f, ry, w - 106.f,
                   font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
        ry += 23.f;
    }

    if (!f->arrival && ry < y + h - 46.f) {
        ry += 4.f;
        tx_draw(c, "DELAY RISK", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        char pc[16]; snprintf(pc, sizeof pc, "%.0f%%", f->delayRisk*100.f);
        Color rc = f->delayRisk > .6f ? C_DANGER
                 : (f->delayRisk > .3f ? C_WARN : C_OK);
        tx_draw(c, pc, x + w - 18.f, ry - 2.f, font_make(TF_DISPLAY, 15, TW_BOLD),
                rc, AL_R, AV_T);
        ui_meter(x + 18.f, ry + 16.f, w - 36.f, 6.f, f->delayRisk,
                 col_alpha(rc, .55f), rc);
    }
}

/* ==========================================================================
 *  entry point
 * ========================================================================== */

void screen_airfield(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    float pad = PAD;
    float rightW = 336.f;
    float mapW = w - rightW - pad*3.f;
    float mapH = h - pad*2.f;
    float mapX = x + pad, mapY = y + pad;

    /* --- map panel -------------------------------------------------------- */
    ui_panel_dark(mapX, mapY, mapW, mapH, R_LG);
    cv_clip_push(c, mapX + 1.f, mapY + 1.f, mapW - 2.f, mapH - 2.f);

    /* subtle grid so panning reads */
    for (float gx = mapX; gx < mapX + mapW; gx += 46.f)
        cv_line(c, gx, mapY, gx, mapY + mapH, col_alpha(C_V400, .045f), 1.f);
    for (float gy = mapY; gy < mapY + mapH; gy += 46.f)
        cv_line(c, mapX, gy, mapX + mapW, gy, col_alpha(C_V400, .045f), 1.f);

    map_setup(a, mapX, mapY, mapW, mapH);

    /* drag to pan */
    uint64_t panId = uid("fieldpan");
    float *dragging = ui_state(panId, 0.f);
    if (ui_hit(mapX, mapY, mapW, mapH)) {
        if (a->in.pressed) *dragging = 1.f;
        if (a->in.wheel != 0.f) {
            a->fieldZoom *= (a->in.wheel > 0.f ? 1.12f : 0.89f);
            a->fieldZoom = cv_clampf(a->fieldZoom, 0.8f, 3.2f);
        }
    }
    if (!a->in.down) *dragging = 0.f;
    if (*dragging > 0.5f) {
        a->fieldPanX += a->in.mx - a->in.pmx;
        a->fieldPanY += a->in.my - a->in.pmy;
    }

    draw_surfaces(a, c);
    draw_terminal(a, c);
    draw_vehicles(a, c);
    draw_aircraft_layer(a, c);
    draw_map_overlays(a, c, mapX, mapY, mapW, mapH);

    /* header strip over the map */
    Paint hg = paint_linear(mapX, mapY, mapX, mapY + 54.f,
                            col_alpha(C_NIGHT, .92f), col_alpha(C_NIGHT, .0f));
    cv_rect_p(c, mapX, mapY, mapW, 54.f, &hg);
    tx_backdrop(C_NIGHT);
    tx_draw(c, "PLAISANCE  -  AIRFIELD MOVEMENT DISPLAY", mapX + 18.f, mapY + 14.f,
            font_track(TF_UI, 11, TW_BOLD, 2), col_alpha(HEX(0xFFFFFF), .92f),
            AL_L, AV_T);
    char sub[90];
    snprintf(sub, sizeof sub, "Runway %02d in use   -   %d stands   -   %s",
             a->w.activeRunway, a->w.nStands, wx_name(a->w.wxKind));
    tx_draw(c, sub, mapX + 18.f, mapY + 31.f, font_make(TF_UI, 11, TW_MED),
            col_alpha(C_V200, .75f), AL_L, AV_T);

    cv_clip_pop(c);

    /* map controls */
    float bx = mapX + mapW - 150.f, by = mapY + mapH - 44.f;
    if (ui_icon_btn(uid("aflab"), bx, by, 32.f, IC_TAG,
                    a->fieldLabels ? BTN_PRIMARY : BTN_DARK)) a->fieldLabels ^= 1;
    if (ui_icon_btn(uid("aftr"), bx + 38.f, by, 32.f, IC_LAYERS,
                    a->fieldTrails ? BTN_PRIMARY : BTN_DARK)) a->fieldTrails ^= 1;
    if (ui_icon_btn(uid("afrst"), bx + 76.f, by, 32.f, IC_REFRESH, BTN_DARK)) {
        a->fieldZoom = 1.f; a->fieldPanX = a->fieldPanY = 0.f;
    }

    /* --- right column ----------------------------------------------------- */
    float rx = x + w - rightW - pad;
    float cardH = 268.f;
    draw_selected_card(a, rx, y + pad, rightW, cardH);
    draw_movement_panel(a, rx, y + pad + cardH + 14.f, rightW,
                        h - pad*2.f - cardH - 14.f);
}
