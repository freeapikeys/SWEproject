/* ==========================================================================
 *  AURA :: screen_baggage.c   --   the baggage handling system
 *
 *  A live schematic of the hold baggage system: four check-in inputs merge
 *  onto the main line, run through the screening tunnel, and are then either
 *  diverted to manual search or released to the sortation loop and out to the
 *  make-up carousels.
 *
 *  Every bag on this screen is a real record in the register.  Its position
 *  comes from sampling the same belt geometry the simulation advances it
 *  along, and the decision at the diverter is the output of the neural
 *  classifier in ai_bagscan.c -- when a bag is inside the tunnel you can
 *  watch the seven scan features and the score it produces.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* panel design space; everything below is expressed in these units */
#define BP_W 1000.f
#define BP_H  540.f

static float b_ox, b_oy, b_s;
static float BX(float x) { return b_ox + x * b_s; }
static float BY(float y) { return b_oy + y * b_s; }

/* --------------------------------------------------------------------------
 *  belt geometry
 * ------------------------------------------------------------------------- */

static const float L_CHECKIN[4][8] = {
    {  34.f,  82.f, 210.f,  82.f, 268.f, 130.f, 300.f, 186.f },
    {  34.f, 148.f, 210.f, 148.f, 262.f, 168.f, 300.f, 186.f },
    {  34.f, 216.f, 210.f, 216.f, 262.f, 204.f, 300.f, 186.f },
    {  34.f, 282.f, 210.f, 282.f, 268.f, 242.f, 300.f, 186.f },
};
static const float L_SCREEN[]  = { 300.f, 186.f, 620.f, 186.f };
static const float L_HELD[]    = { 620.f, 186.f, 664.f, 186.f, 664.f,  92.f,
                                   742.f,  92.f };
static const float L_SORT[]    = { 620.f, 186.f, 700.f, 186.f, 900.f, 186.f,
                                   946.f, 216.f, 946.f, 296.f, 900.f, 330.f,
                                   700.f, 330.f, 654.f, 296.f, 654.f, 236.f,
                                   700.f, 210.f, 800.f, 210.f };
static const float L_MAKEUP[3][6] = {
    { 800.f, 210.f, 700.f, 300.f, 690.f, 430.f },
    { 800.f, 210.f, 810.f, 320.f, 812.f, 430.f },
    { 800.f, 210.f, 910.f, 310.f, 934.f, 430.f },
};

static void lane_for(Bag *b, const float **pts, int *n)
{
    switch (b->state) {
    case BG_CHECKIN: *pts = L_CHECKIN[b->lane & 3]; *n = 4; break;
    case BG_SCREEN:  *pts = L_SCREEN;  *n = 2; break;
    case BG_HELD:    *pts = L_HELD;    *n = 4; break;
    case BG_SORT:    *pts = L_SORT;    *n = 11; break;
    case BG_MAKEUP:  *pts = L_MAKEUP[b->id % 3]; *n = 3; break;
    default:         *pts = NULL;      *n = 0; break;
    }
}

/* --------------------------------------------------------------------------
 *  drawing
 * ------------------------------------------------------------------------- */

static void belt(Canvas *c, const float *p, int n, float wide, Color col,
                 int chevrons, float speed)
{
    Path path; path_reset(&path);
    for (int i = 0; i < n; i++) path_line(&path, BX(p[i*2]), BY(p[i*2+1]));
    cv_stroke_col(c, &path, col, wide * b_s);

    /* side rails */
    cv_stroke_col(c, &path, col_alpha(HEX(0x000000), .22f), wide * b_s * 0.16f);

    if (!chevrons) return;
    /* moving cleats along the belt read as motion even with no bag on it */
    float total = 0.f;
    for (int i = 0; i < n - 1; i++) {
        float dx = p[i*2+2] - p[i*2], dy = p[i*2+3] - p[i*2+1];
        total += sqrtf(dx*dx + dy*dy);
    }
    int marks = (int)(total / 26.f);
    if (marks < 2) marks = 2;
    if (marks > 60) marks = 60;
    float phase = anim_time() * speed;
    for (int i = 0; i < marks; i++) {
        float t = ((float)i / marks + phase - floorf((float)i/marks + phase));
        float x, y, hd;
        path_sample(p, n, t, &x, &y, &hd);
        float nx = -sinf(hd), ny = cosf(hd);
        float hw = wide * 0.34f;
        cv_line(c, BX(x - nx*hw), BY(y - ny*hw), BX(x + nx*hw), BY(y + ny*hw),
                col_alpha(HEX(0xFFFFFF), .10f), 1.6f);
    }
}

static void draw_bag(Canvas *c, float x, float y, float sz, Color col,
                     int flagged, float wobble, int selected)
{
    float tilt = sinf(wobble) * 0.09f;
    float w = sz, h = sz * 0.74f;
    float ox = sinf(wobble*1.3f) * sz * 0.04f;

    if (selected) cv_glow(c, x, y, sz*1.7f, C_V300, .5f);
    cv_shadow(c, x - w*0.5f, y - h*0.5f + sz*0.30f, w, h, sz*0.20f, 5.f,
              RGBA(0,0,0,90));

    /* body */
    cv_rrect(c, x - w*0.5f + ox, y - h*0.5f + tilt*4.f, w, h, sz*0.18f, col);
    /* lid highlight */
    cv_rrect(c, x - w*0.5f + ox + sz*0.08f, y - h*0.5f + tilt*4.f + sz*0.07f,
             w - sz*0.16f, h*0.34f, sz*0.10f, col_lighten(col, .22f));
    /* handle */
    cv_rrect(c, x - sz*0.16f + ox, y - h*0.5f + tilt*4.f - sz*0.13f,
             sz*0.32f, sz*0.14f, sz*0.07f, col_darken(col, .30f));
    /* tag */
    cv_circle(c, x + w*0.34f + ox, y + h*0.16f, sz*0.10f,
              flagged ? C_DANGER : HEX(0xF3EEFE));

    if (flagged) {
        float pulse = 0.5f + 0.5f*sinf(anim_time()*7.f);
        cv_circle_line(c, x, y, sz*0.86f, col_alpha(C_DANGER, .45f + .45f*pulse),
                       1.8f);
    }
}

static Color bag_colour(Bag *b)
{
    static const uint32_t PAL[6] = {
        0x6E35DC, 0x2E9E8F, 0xC0562B, 0x3B6FBF, 0x9A2F86, 0x4B5563
    };
    return HEX(PAL[b->colour % 6]);
}

/* --------------------------------------------------------------------------
 *  the machine room
 * ------------------------------------------------------------------------- */

static void draw_bhs(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;

    ui_panel_dark(x, y, w, h, R_LG);
    cv_clip_push(c, x + 1.f, y + 1.f, w - 2.f, h - 2.f);

    float sx = (w - 24.f) / BP_W, sy = (h - 24.f) / BP_H;
    b_s = sx < sy ? sx : sy;
    b_ox = x + (w - BP_W*b_s)*0.5f;
    b_oy = y + (h - BP_H*b_s)*0.5f;

    /* floor grid */
    for (float gx = 0.f; gx <= BP_W; gx += 50.f)
        cv_line(c, BX(gx), BY(0.f), BX(gx), BY(BP_H), col_alpha(C_V400, .045f), 1.f);
    for (float gy = 0.f; gy <= BP_H; gy += 50.f)
        cv_line(c, BX(0.f), BY(gy), BX(BP_W), BY(gy), col_alpha(C_V400, .045f), 1.f);

    /* ---- belts ----------------------------------------------------------- */
    for (int i = 0; i < 4; i++)
        belt(c, L_CHECKIN[i], 4, 16.f, HEX(0x2E1760), 1, 0.16f);
    belt(c, L_SORT,   11, 19.f, HEX(0x33195F), 1, 0.09f);
    belt(c, L_SCREEN,  2, 20.f, HEX(0x2E1760), 1, 0.14f);
    belt(c, L_HELD,    4, 14.f, HEX(0x4A1330), 1, 0.13f);
    for (int i = 0; i < 3; i++)
        belt(c, L_MAKEUP[i], 3, 14.f, HEX(0x2A1556), 1, 0.11f);

    tx_backdrop(C_NIGHT_2);
    Font lbl = font_track(TF_UI, (int)(7.f + b_s*4.f), TW_BOLD, 1);
    Font small = font_make(TF_UI, (int)(6.f + b_s*4.f), TW_MED);

    /* ---- check-in inputs ------------------------------------------------- */
    for (int i = 0; i < 4; i++) {
        float dx = BX(34.f), dy = BY(L_CHECKIN[i][1]);
        cv_rrect(c, dx - 30.f*b_s, dy - 22.f*b_s, 46.f*b_s, 44.f*b_s,
                 6.f*b_s, HEX(0x3A1E76));
        cv_rrect_line(c, dx - 30.f*b_s, dy - 22.f*b_s, 46.f*b_s, 44.f*b_s,
                      6.f*b_s, col_alpha(C_V300, .3f), 1.f);
        char t[24]; snprintf(t, sizeof t, "IN %d", i + 1);
        tx_draw(c, t, dx - 7.f*b_s, dy, lbl, col_alpha(C_V200, .9f), AL_C, AV_M);
    }

    /* ---- screening tunnel ------------------------------------------------ */
    float tx0 = BX(392.f), tw = 150.f*b_s, ty0 = BY(140.f), th = 92.f*b_s;
    cv_rrect(c, tx0, ty0, tw, th, 8.f*b_s, HEX(0x1B0F3E));
    cv_rrect_line(c, tx0, ty0, tw, th, 8.f*b_s, col_alpha(C_TEAL, .45f), 1.6f);
    /* the scan curtain sweeping across the tunnel */
    float sweep = fmodf(anim_time()*0.55f, 1.f);
    float sxp = tx0 + tw*sweep;
    cv_rect(c, sxp - 1.f, ty0 + 4.f, 2.4f, th - 8.f, col_alpha(C_TEAL, .8f));
    cv_glow(c, sxp, ty0 + th*0.5f, 26.f*b_s, C_TEAL, .30f);
    tx_draw(c, "HOLD BAGGAGE SCREENING", tx0 + tw*0.5f, ty0 - 12.f*b_s,
            lbl, col_alpha(C_TEAL, .9f), AL_C, AV_M);
    tx_draw(c, "LEVEL 1  -  AUTOMATIC", tx0 + tw*0.5f, ty0 + th + 12.f*b_s,
            small, col_alpha(C_V300, .6f), AL_C, AV_M);

    /* ---- diverter -------------------------------------------------------- */
    float dvx = BX(620.f), dvy = BY(186.f);
    cv_circle(c, dvx, dvy, 15.f*b_s, HEX(0x3A1E76));
    cv_circle_line(c, dvx, dvy, 15.f*b_s, col_alpha(C_V300, .5f), 1.4f);
    float arm = anim_time()*1.1f;
    cv_line(c, dvx, dvy, dvx + cosf(arm)*11.f*b_s, dvy + sinf(arm)*11.f*b_s,
            col_alpha(C_GOLD, .85f), 2.2f);
    tx_draw(c, "DIVERTER", dvx, dvy + 28.f*b_s, small, col_alpha(C_V300,.7f),
            AL_C, AV_M);

    /* ---- manual search bay ---------------------------------------------- */
    float ibx = BX(742.f), iby = BY(92.f);
    cv_rrect(c, ibx - 8.f*b_s, iby - 30.f*b_s, 120.f*b_s, 60.f*b_s, 8.f*b_s,
             HEX(0x4A1330));
    cv_rrect_line(c, ibx - 8.f*b_s, iby - 30.f*b_s, 120.f*b_s, 60.f*b_s,
                  8.f*b_s, col_alpha(C_DANGER, .5f), 1.4f);
    int held = sim_bags_in_state(wo, BG_HELD);
    char hb[32]; snprintf(hb, sizeof hb, "MANUAL SEARCH   %d", held);
    tx_draw(c, hb, ibx + 52.f*b_s, iby, lbl,
            held ? HEX(0xFF9A9A) : col_alpha(C_V300,.7f), AL_C, AV_M);
    if (held) {
        float pulse = 0.5f + 0.5f*sinf(anim_time()*5.f);
        cv_glow(c, ibx + 52.f*b_s, iby, 60.f*b_s, C_DANGER, .10f + .16f*pulse);
    }

    /* ---- sortation loop label ------------------------------------------- */
    tx_draw(c, "SORTATION LOOP", BX(800.f), BY(258.f), lbl,
            col_alpha(C_V200, .55f), AL_C, AV_M);

    /* ---- make-up carousels ---------------------------------------------- */
    static const float CAR[3][2] = { {690.f,430.f}, {812.f,430.f}, {934.f,430.f} };
    for (int i = 0; i < 3; i++) {
        float cx = BX(CAR[i][0]), cy = BY(CAR[i][1]), r = 44.f*b_s;
        cv_circle(c, cx, cy, r, HEX(0x2A1556));
        cv_circle_line(c, cx, cy, r, col_alpha(C_V300, .35f), 1.6f);
        cv_circle_line(c, cx, cy, r*0.55f, col_alpha(C_V300, .18f), 1.2f);
        /* rotating cleats */
        float rot = anim_time()*0.55f + i;
        for (int k = 0; k < 12; k++) {
            float ang = rot + (float)k*0.5236f;
            cv_line(c, cx + cosf(ang)*r*0.58f, cy + sinf(ang)*r*0.58f,
                       cx + cosf(ang)*r*0.94f, cy + sinf(ang)*r*0.94f,
                    col_alpha(HEX(0xFFFFFF), .07f), 1.6f);
        }
        char t[24]; snprintf(t, sizeof t, "MU%d", i + 1);
        tx_draw(c, t, cx, cy, lbl, col_alpha(C_V200, .8f), AL_C, AV_M);

        /* ULD stack beside the carousel */
        for (int u = 0; u < 3; u++) {
            float ux = cx + r + 12.f*b_s + u*17.f*b_s, uy = cy + 10.f*b_s;
            cv_rrect(c, ux, uy - 14.f*b_s, 14.f*b_s, 22.f*b_s, 2.f*b_s,
                     HEX(0x554070));
            cv_rrect(c, ux, uy - 14.f*b_s, 14.f*b_s, 5.f*b_s, 2.f*b_s,
                     HEX(0x6A5290));
        }
    }

    /* ---- bags ------------------------------------------------------------ */
    Bag *inTunnel = NULL;
    float bagSz = 17.f * b_s;
    for (int i = 0; i < wo->nBags; i++) {
        Bag *b = &wo->bag[i];
        if (b->state >= BG_LOADED) continue;
        if (b->releaseMin <= 0.f || wo->clock < b->releaseMin) continue;

        const float *lp; int ln;
        lane_for(b, &lp, &ln);
        if (!lp) continue;

        float px, py, hd;
        path_sample(lp, ln, b->t, &px, &py, &hd);
        int sel = (a->selBag == b->id);
        draw_bag(c, BX(px), BY(py), bagSz, bag_colour(b), b->threat,
                 b->wobble, sel);

        if (b->state == BG_SCREEN && b->t > 0.30f && b->t < 0.72f && !inTunnel)
            inTunnel = b;

        float hr = bagSz*0.8f;
        if (ui_hit(BX(px)-hr, BY(py)-hr, hr*2.f, hr*2.f)) {
            ui_cursor(1);
            if (a->in.pressed) a->selBag = b->id;
        }
    }

    /* bags already loaded rest on the carousels */
    for (int i = 0; i < 3; i++) {
        int cnt = 0;
        for (int k = 0; k < wo->nBags; k++)
            if (wo->bag[k].state == BG_LOADED && (wo->bag[k].id % 3) == i) cnt++;
        int show = cnt > 9 ? 9 : cnt;
        float cx = BX(CAR[i][0]), cy = BY(CAR[i][1]), r = 44.f*b_s;
        for (int k = 0; k < show; k++) {
            float ang = anim_time()*0.55f + i + k*0.7f;
            cv_circle(c, cx + cosf(ang)*r*0.76f, cy + sinf(ang)*r*0.76f,
                      bagSz*0.28f, col_alpha(C_V300, .75f));
        }
    }

    /* ---- live scan readout ---------------------------------------------- */
    if (inTunnel) {
        float px = BX(430.f), py = BY(258.f), pw = 190.f*b_s;
        cv_rrect(c, px, py, pw, 116.f*b_s, 8.f*b_s, col_alpha(C_NIGHT, .92f));
        cv_rrect_line(c, px, py, pw, 116.f*b_s, 8.f*b_s,
                      col_alpha(C_TEAL, .45f), 1.2f);
        tx_backdrop(C_NIGHT);
        char t[40]; snprintf(t, sizeof t, "SCANNING  %s", inTunnel->tag);
        tx_draw(c, t, px + 10.f*b_s, py + 8.f*b_s, small, C_TEAL, AL_L, AV_T);

        float feat[BAG_IN];
        ai_bag_features(wo, inTunnel, feat);
        float score = ai_bagnet_eval(a->bagnet, feat, NULL, NULL);
        for (int k = 0; k < BAG_IN; k++) {
            float ry = py + (22.f + k*11.f)*b_s;
            cv_rect(c, px + 10.f*b_s, ry, (pw - 60.f*b_s), 4.f*b_s,
                    col_alpha(C_V400, .18f));
            cv_rect(c, px + 10.f*b_s, ry, (pw - 60.f*b_s)*feat[k], 4.f*b_s,
                    col_mix(C_TEAL, C_DANGER, feat[k]));
        }
        char sc[32]; snprintf(sc, sizeof sc, "%.2f", score);
        tx_draw(c, sc, px + pw - 10.f*b_s, py + 46.f*b_s,
                font_track(TF_DISPLAY, (int)(11.f + b_s*8.f), TW_BOLD, 0),
                score > BAG_THRESHOLD ? C_DANGER : C_OK, AL_R, AV_M);
        tx_draw(c, score > BAG_THRESHOLD ? "DIVERT" : "CLEAR", px + pw - 10.f*b_s,
                py + 68.f*b_s, small, score > BAG_THRESHOLD ? C_DANGER : C_OK,
                AL_R, AV_M);
    }

    cv_clip_pop(c);
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_baggage(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    /* ---- statistics row -------------------------------------------------- */
    float tw = (w - pad*2.f - 3*12.f) / 4.f;
    float ty = y + pad;
    char v[40], s[60];

    int inSys = sim_active_bags(wo);
    snprintf(v, sizeof v, "%d", inSys);
    snprintf(s, sizeof s, "%d in the register today", wo->nBags);
    draw_stat_tile(c, x + pad, ty, tw, 92.f, "IN SYSTEM", v, s, IC_LUGGAGE, C_V600);

    int scr = sim_bags_in_state(wo, BG_SCREEN) + sim_bags_in_state(wo, BG_SORT);
    snprintf(v, sizeof v, "%d", scr);
    snprintf(s, sizeof s, "screening and sortation");
    draw_stat_tile(c, x + pad + (tw+12.f), ty, tw, 92.f, "IN PROCESS", v, s,
                   IC_SCAN, C_TEAL);

    int held = sim_bags_in_state(wo, BG_HELD);
    snprintf(v, sizeof v, "%d", held);
    snprintf(s, sizeof s, "%d alerts raised today", wo->securityAlerts);
    draw_stat_tile(c, x + pad + (tw+12.f)*2.f, ty, tw, 92.f, "HELD FOR SEARCH",
                   v, s, IC_SHIELD, held ? C_DANGER : C_INK_3);

    int loaded = sim_bags_in_state(wo, BG_LOADED);
    snprintf(v, sizeof v, "%d", loaded);
    snprintf(s, sizeof s, "%d mishandled", wo->bagsMishandled);
    draw_stat_tile(c, x + pad + (tw+12.f)*3.f, ty, tw, 92.f, "LOADED", v, s,
                   IC_BOX, C_OK);

    /* ---- machine room + side panel --------------------------------------- */
    float my = ty + 104.f;
    float sideW = 320.f;
    float mw = w - pad*2.f - sideW - 14.f;
    float mh = h - (my - y) - pad;
    draw_bhs(a, x + pad, my, mw, mh);

    float rx = x + pad + mw + 14.f;

    /* trace a bag */
    ui_card(rx, my, sideW, 168.f, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "BAGGAGE TRACING", rx + 16.f, my + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    if (ui_text_field(uid("bagsearch"), rx + 16.f, my + 36.f, sideW - 32.f, 40.f,
                      a->searchBag, sizeof a->searchBag,
                      "Bag tag, e.g. MK483920", IC_TAG)) {
        Bag *b = bag_by_tag(wo, a->searchBag);
        if (b) a->selBag = b->id;
    }

    Bag *sel = NULL;
    for (int i = 0; i < wo->nBags; i++)
        if (wo->bag[i].id == a->selBag) { sel = &wo->bag[i]; break; }

    if (sel) {
        Flight *f = flight_by_id(wo, sel->flight);
        Passenger *p = NULL;
        for (int i = 0; i < wo->nPax; i++)
            if (wo->pax[i].id == sel->pax) { p = &wo->pax[i]; break; }
        tx_draw(c, sel->tag, rx + 16.f, my + 86.f,
                font_track(TF_MONO, 16, TW_BOLD, 1), C_INK, AL_L, AV_T);
        char l1[80];
        /*  Twenty-three kilogrammes is the usual free allowance and thirty-two
         *  is the point at which a single bag may not be lifted by hand at
         *  all -- it has to be repacked, not just paid for.                */
        const char *over = sel->weight > 32.f ? "  OVER 32 kg -- must be repacked"
                         : (sel->weight > 23.f ? "  over the 23 kg allowance" : "");
        snprintf(l1, sizeof l1, "%s   %.1f kg%s", bs_name(sel->state),
                 sel->weight, over);
        tx_draw(c, l1, rx + 16.f, my + 108.f, font_make(TF_UI, 12, TW_SEMI),
                sel->threat ? C_DANGER : C_INK_2, AL_L, AV_T);
        char l2[90];
        snprintf(l2, sizeof l2, "%s   %s", f ? f->no : "----",
                 p ? p->name : "unknown passenger");
        tx_clipped(c, l2, rx + 16.f, my + 128.f, sideW - 32.f,
                   font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);
        if (sel->threat && ui_button_i(uid("clearbag"), rx + 16.f, my + 146.f,
                                       sideW - 32.f, 0.f, "", 0, BTN_SOFT)) { }
    } else {
        tx_draw(c, "Enter a tag or click a bag on the belt", rx + 16.f,
                my + 92.f, font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);
        tx_draw(c, "Every bag on screen is a live record.", rx + 16.f,
                my + 110.f, font_make(TF_UI, 11, TW_REG), C_INK_4, AL_L, AV_T);
    }

    /* held bags list */
    float ly = my + 182.f;
    float lh = mh - 182.f;
    ui_card(rx, ly, sideW, lh, R_LG);
    tx_draw(c, "HELD FOR MANUAL SEARCH", rx + 16.f, ly + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_DANGER, AL_L, AV_T);

    int ids[40], n = 0;
    for (int i = 0; i < wo->nBags && n < 40; i++)
        if (wo->bag[i].state == BG_HELD) ids[n++] = i;

    if (!n) {
        icon_draw(c, IC_CHECK, rx + sideW*0.5f, ly + lh*0.5f - 14.f, 26.f, C_OK);
        tx_draw(c, "Nothing held", rx + sideW*0.5f, ly + lh*0.5f + 12.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    } else {
        float rowH = 60.f;
        float off = ui_scroll_begin(uid("heldscroll"), rx + 6.f, ly + 36.f,
                                    sideW - 12.f, lh - 46.f, n*rowH + 6.f);
        for (int i = 0; i < n; i++) {
            Bag *b = &wo->bag[ids[i]];
            float ry = ly + 36.f + i*rowH - off;
            if (ry + rowH < ly + 36.f || ry > ly + lh) continue;
            Flight *f = flight_by_id(wo, b->flight);

            cv_rrect(c, rx + 12.f, ry, sideW - 24.f, rowH - 8.f, 9.f, C_DANGER_BG);
            tx_backdrop(C_DANGER_BG);
            tx_draw(c, b->tag, rx + 22.f, ry + 8.f,
                    font_track(TF_MONO, 13, TW_BOLD, 0), C_INK, AL_L, AV_T);
            char sc[48];
            snprintf(sc, sizeof sc, "score %.2f   %s   %.1f kg", b->threatScore,
                     f ? f->no : "----", b->weight);
            tx_clipped(c, sc, rx + 22.f, ry + 27.f, sideW - 100.f,
                       font_make(TF_UI, 11, TW_MED), C_INK_2, AL_L, AV_T);

            if (ui_button(uidi("clr", b->id), rx + sideW - 88.f, ry + 12.f,
                          64.f, 28.f, "Clear", BTN_SOFT)) {
                b->state = BG_SORT;
                b->threat = 0;
                b->inspected = 1;
                b->t = 0.f;
                world_log(wo, LG_OK, "Bag %s cleared after manual search", b->tag);
                ui_toast(TOAST_OK, "Bag released", b->tag);
            }
        }
        ui_scroll_end();
    }
}
