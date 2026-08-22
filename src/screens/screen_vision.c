/* ==========================================================================
 *  AURA :: screen_vision.c   --   terminal surveillance
 *
 *  Six camera views over the landside and airside terminal, each running the
 *  full analytics pipeline from src/cv/vision.c.  What is drawn on a tile is
 *  the tracker's belief, not the scene: the boxes come from tracks, the
 *  identities come from the cross-camera gallery, and the heatmap and flow
 *  field come from the accumulators.  The underlying people can be revealed
 *  with the ground truth toggle to see where the tracker is wrong.
 * ========================================================================== */

#include "../app.h"
#include "../cv/vision.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* stable colour per global identity */
static Color id_colour(int gid)
{
    static const uint32_t PAL[10] = {
        0x8B5CF6, 0x22D3EE, 0xF472B6, 0xFBBF24, 0x34D399,
        0xF87171, 0xA3E635, 0x60A5FA, 0xC084FC, 0xFB923C
    };
    if (gid < 0) return HEX(0x94A3B8);
    return HEX(PAL[((unsigned)gid) % 10]);
}

/* --------------------------------------------------------------------------
 *  the fixed furniture of each zone, so a tile reads as a place
 * ------------------------------------------------------------------------- */

static void draw_zone_scene(Canvas *c, Camera *cam, float x, float y,
                            float w, float h, float detail)
{
    Color line = col_alpha(C_V300, .13f);

    switch (cam->zone) {
    case ZN_IMMIGRATION:
    case ZN_DEPARTURES: {
        /* counter line across the top, lane dividers below */
        float cy = y + h*0.16f;
        cv_rect(c, x + w*0.06f, cy, w*0.88f, h*0.045f, col_alpha(C_V300, .22f));
        int n = cam->servers;
        for (int i = 0; i < n; i++) {
            float bx = x + w*0.06f + (w*0.88f) * (i + 0.5f) / n;
            cv_rrect(c, bx - w*0.022f, cy - h*0.05f, w*0.044f, h*0.05f, 2.f,
                     col_alpha(C_TEAL, .35f));
        }
        for (int i = 1; i < n; i++) {
            float bx = x + w*0.06f + (w*0.88f) * i / n;
            for (float yy = cy + h*0.09f; yy < y + h*0.95f; yy += h*0.055f)
                cv_rect(c, bx, yy, 1.2f, h*0.03f, line);
        }
        if (detail > 0.6f) {
            tx_backdrop(HEX(0x0C0718));
            tx_draw(c, cam->oneWay ? "ONE WAY" : "", x + w*0.5f, y + h*0.93f,
                    font_track(TF_UI, 8, TW_BOLD, 2), col_alpha(C_V300,.30f),
                    AL_C, AV_M);
        }
    } break;

    case ZN_BAGGAGE: {
        /* a reclaim carousel */
        float cx = x + w*0.5f, cy = y + h*0.55f;
        float rx = w*0.30f, ry = h*0.22f;
        Path p; path_reset(&p);
        path_ellipse(&p, cx, cy, rx, ry);
        cv_stroke_col(c, &p, col_alpha(C_V300, .22f), 3.f);
        path_reset(&p);
        path_ellipse(&p, cx, cy, rx*0.72f, ry*0.68f);
        cv_stroke_col(c, &p, col_alpha(C_V300, .12f), 2.f);
    } break;

    case ZN_DUTYFREE: {
        /* retail gondolas */
        for (int i = 0; i < 4; i++) {
            float gx = x + w*(0.14f + i*0.22f);
            cv_rrect(c, gx, y + h*0.28f, w*0.10f, h*0.42f, 3.f,
                     col_alpha(C_V400, .10f));
            cv_rrect_line(c, gx, y + h*0.28f, w*0.10f, h*0.42f, 3.f, line, 1.f);
        }
    } break;

    case ZN_TARMAC: {
        /* stand markings and a restricted boundary */
        float cx = x + w*0.5f;
        cv_line(c, cx, y + h*0.10f, cx, y + h*0.70f, col_alpha(C_GOLD,.22f), 2.f);
        cv_line(c, x + w*0.22f, y + h*0.70f, x + w*0.78f, y + h*0.70f,
                col_alpha(C_GOLD,.22f), 2.f);
        for (float xx = x + w*0.06f; xx < x + w*0.94f; xx += w*0.06f)
            cv_rect(c, xx, y + h*0.86f, w*0.03f, 2.f, col_alpha(C_DANGER,.30f));
        if (detail > 0.6f) {
            tx_backdrop(HEX(0x0C0718));
            tx_draw(c, "RESTRICTED", x + w*0.5f, y + h*0.92f,
                    font_track(TF_UI, 8, TW_BOLD, 2), col_alpha(C_DANGER,.45f),
                    AL_C, AV_M);
        }
    } break;

    default: {
        /* open hall: a few structural columns */
        for (int i = 0; i < 3; i++) {
            float gx = x + w*(0.25f + i*0.25f);
            cv_circle(c, gx, y + h*0.5f, w*0.012f, col_alpha(C_V300,.14f));
        }
        cv_line(c, x + w*0.04f, y + h*0.80f, x + w*0.96f, y + h*0.80f, line, 1.4f);
    } break;
    }
}

/* --------------------------------------------------------------------------
 *  one camera tile
 * ------------------------------------------------------------------------- */

static void draw_camera(App *a, VisionSystem *v, int ci,
                        float x, float y, float w, float h, int big)
{
    Canvas *c = &a->cv;
    Camera *cam = &v->cam[ci];
    CameraState *s = &v->st[ci];
    float detail = big ? 1.f : (w > 300.f ? 0.7f : 0.45f);

    /* --- the picture ------------------------------------------------------ */
    cv_rrect(c, x, y, w, h, 10.f, HEX(0x0C0718));
    cv_clip_push(c, x + 1.f, y + 1.f, w - 2.f, h - 2.f);

    Paint fl = paint_linear(x, y, x, y + h, HEX(0x140B2C), HEX(0x0A0518));
    cv_rect_p(c, x, y, w, h, &fl);

    float vy0 = y + 26.f, vh = h - 26.f;      /* below the header strip       */
    draw_zone_scene(c, cam, x, vy0, w, vh, detail);

    #define VX(nx) (x   + (nx) * w)
    #define VY(ny) (vy0 + (ny) * vh)

    /* --- density heatmap -------------------------------------------------- */
    if (a->cvHeatmap) {
        float cw = w / CV_GRID_X, ch = vh / CV_GRID_Y;
        for (int gy = 0; gy < CV_GRID_Y; gy++)
            for (int gx = 0; gx < CV_GRID_X; gx++) {
                float d = s->density[gy][gx];
                if (d < 0.04f) continue;
                if (d > 1.f) d = 1.f;
                Color hc = col_mix(C_TEAL, C_DANGER, d);
                cv_rect(c, x + gx*cw, vy0 + gy*ch, cw, ch, col_alpha(hc, d*0.30f));
            }
    }

    /* --- flow field ------------------------------------------------------- */
    if (a->cvFlow && detail > 0.4f) {
        float cw = w / CV_GRID_X, ch = vh / CV_GRID_Y;
        for (int gy = 0; gy < CV_GRID_Y; gy += 2)
            for (int gx = 0; gx < CV_GRID_X; gx += 2) {
                float fx = s->flowX[gy][gx], fy = s->flowY[gy][gx];
                float m = sqrtf(fx*fx + fy*fy);
                if (m < 0.02f) continue;
                float px = x + (gx+0.5f)*cw, py = vy0 + (gy+0.5f)*ch;
                float sc = 14.f / (m + 0.6f);
                cv_line_round(c, px, py, px + fx*sc, py + fy*sc,
                              col_alpha(C_V300, .45f), 1.6f);
            }
    }

    /* --- ground truth, when asked ---------------------------------------- */
    if (a->cvTruth) {
        for (int i = 0; i < CV_AGENTS; i++) {
            Agent *ag = &s->agent[i];
            if (!ag->live) continue;
            if (ag->x < 0.f || ag->x > 1.f || ag->y < 0.f || ag->y > 1.f) continue;
            cv_circle(c, VX(ag->x), VY(ag->y), 3.2f, col_alpha(C_OK, .75f));
        }
    }

    /* --- raw detections --------------------------------------------------- */
    if (a->cvDets) {
        for (int i = 0; i < s->nDet; i++) {
            Detection *d = &s->det[i];
            float bw = d->w * w, bh = d->h * vh;
            cv_rrect_line(c, VX(d->x) - bw*0.5f, VY(d->y) - bh*0.5f, bw, bh, 2.f,
                          col_alpha(HEX(0xFFFFFF), .22f), 1.f);
        }
    }

    /* --- unattended items ------------------------------------------------- */
    for (int b = 0; b < 8; b++) {
        BagObject *g = &s->bag[b];
        if (!g->live) continue;
        float bx = VX(g->x), by = VY(g->y);
        Color bc = g->flagged ? C_DANGER : C_GOLD;
        cv_rrect(c, bx - 4.f, by - 3.f, 8.f, 6.f, 1.5f, col_alpha(bc, .9f));
        if (g->flagged) {
            float pulse = 0.5f + 0.5f*sinf(anim_time()*6.f);
            cv_circle_line(c, bx, by, 9.f + pulse*4.f,
                           col_alpha(C_DANGER, .30f + .45f*pulse), 1.6f);
        }
    }

    /* --- tracks ----------------------------------------------------------- */
    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state == TR_FREE) continue;
        if (t->state == TR_TENTATIVE && !a->cvDets) continue;

        Color tc = id_colour(t->globalId);
        float alpha = (t->state == TR_CONFIRMED) ? 1.f
                    : (t->state == TR_COASTING ? 0.55f : 0.35f);

        /* trail */
        if (a->cvTrails && t->nTrail > 2) {
            Path p; path_reset(&p);
            int started = 0;
            for (int k = 0; k < t->nTrail; k++) {
                int idx = (t->trailHead - t->nTrail + k + CV_TRAIL*2) % CV_TRAIL;
                float px = VX(t->trail[idx][0]), py = VY(t->trail[idx][1]);
                if (!started) { path_move(&p, px, py); started = 1; }
                else          path_line(&p, px, py);
            }
            cv_stroke_col(c, &p, col_alpha(tc, .32f*alpha), 1.8f);
        }

        float bw = t->w * w, bh = t->h * vh;
        float bx = VX(t->x) - bw*0.5f, by = VY(t->y) - bh*0.5f;

        if (t->flaggedLoiter || t->flaggedCounter) {
            float pulse = 0.5f + 0.5f*sinf(anim_time()*5.f);
            cv_rrect_line(c, bx-3.f, by-3.f, bw+6.f, bh+6.f, 4.f,
                          col_alpha(C_DANGER, .35f + .45f*pulse), 1.8f);
        }
        cv_rrect_line(c, bx, by, bw, bh, 3.f, col_alpha(tc, alpha), 1.6f);
        /* corner ticks, the way a tracker overlay usually looks */
        float tick = bw*0.28f;
        cv_line(c, bx, by, bx+tick, by, col_alpha(tc, alpha), 2.2f);
        cv_line(c, bx+bw-tick, by+bh, bx+bw, by+bh, col_alpha(tc, alpha), 2.2f);

        if (a->cvIds && detail > 0.5f && t->state != TR_TENTATIVE) {
            char lab[24];
            if (t->globalId >= 0) snprintf(lab, sizeof lab, "ID %d", t->globalId);
            else                  snprintf(lab, sizeof lab, "T%d", t->localId);
            Font lf = font_make(TF_UI, 9, TW_BOLD);
            float lw = (float)tx_width(c, lab, lf) + 8.f;
            cv_rrect(c, bx, by - 12.f, lw, 11.f, 2.f, col_alpha(tc, .85f*alpha));
            tx_backdrop(col_mix(HEX(0x0C0718), tc, .6f));
            tx_draw(c, lab, bx + lw*0.5f, by - 6.5f, lf, HEX(0x0C0718),
                    AL_C, AV_M);
        }
    }

    /* --- CCTV texture ----------------------------------------------------- */
    for (float sy = vy0; sy < y + h; sy += 3.f)
        cv_rect(c, x, sy, w, 1.f, col_alpha(HEX(0x000000), .10f));

    /* --- header strip ----------------------------------------------------- */
    cv_rect(c, x, y, w, 26.f, col_alpha(HEX(0x000000), .55f));
    float rec = 0.5f + 0.5f*sinf(anim_time()*2.6f);
    cv_circle(c, x + 12.f, y + 13.f, 4.f, col_alpha(C_DANGER, .45f + .55f*rec));
    tx_backdrop(HEX(0x120A24));
    tx_draw(c, cam->code, x + 24.f, y + 13.f, font_track(TF_MONO, 10, TW_BOLD, 0),
            col_alpha(HEX(0xFFFFFF), .9f), AL_L, AV_M);
    if (detail > 0.5f)
        tx_clipped(c, cam->name, x + 74.f, y + 13.f, w*0.42f,
                   font_make(TF_UI, 10, TW_MED), col_alpha(C_V200,.75f),
                   AL_L, AV_M);

    char stat[48];
    snprintf(stat, sizeof stat, "%d tracked", s->confirmed);
    tx_draw(c, stat, x + w - 10.f, y + 13.f, font_make(TF_UI, 10, TW_SEMI),
            col_alpha(C_V200, .85f), AL_R, AV_M);

    #undef VX
    #undef VY
    cv_clip_pop(c);

    /* selection border */
    int hov = ui_hit(x, y, w, h);
    if (hov) ui_cursor(1);
    if (a->cvFocus == ci)
        cv_rrect_line(c, x, y, w, h, 10.f, C_V400, 2.f);
    else if (hov)
        cv_rrect_line(c, x, y, w, h, 10.f, col_alpha(C_V400, .5f), 1.4f);

    if (hov && a->in.pressed)
        a->cvFocus = (a->cvFocus == ci) ? -1 : ci;
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_vision(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    VisionSystem *v = &a->vision;
    float pad = PAD;

    /* ---- toolbar --------------------------------------------------------- */
    float tb = y + pad;
    float cx = x + pad;

    struct { const char *lbl; int *flag; } tog[] = {
        { "Boxes",    &a->cvIds     },
        { "Trails",   &a->cvTrails  },
        { "Heatmap",  &a->cvHeatmap },
        { "Flow",     &a->cvFlow    },
        { "Raw dets", &a->cvDets    },
        { "Truth",    &a->cvTruth   },
    };
    for (int i = 0; i < 6; i++) {
        Font f = font_make(TF_UI, 12, TW_SEMI);
        float cw = tx_width(c, tog[i].lbl, f) + 26.f;
        if (ui_chip(uidi("cvtog", i), cx, tb, 34.f, tog[i].lbl, *tog[i].flag))
            *tog[i].flag = !*tog[i].flag;
        cx += cw + 6.f;
    }

    if (a->cvFocus >= 0 &&
        ui_button_i(uid("cvback"), x + w - pad - 130.f, tb, 130.f, 34.f,
                    "All cameras", IC_GRID, BTN_SOFT))
        a->cvFocus = -1;

    tx_backdrop(C_BG);
    char hdr[90];
    snprintf(hdr, sizeof hdr, "%d cameras   -   %d tracked   -   %d alerts",
             CV_CAMS, vision_total_tracked(v), vision_alert_count(v));
    if (a->cvFocus < 0)
        tx_draw(c, hdr, x + w - pad, tb + 17.f, font_make(TF_UI, 12, TW_MED),
                C_INK_3, AL_R, AV_M);

    /* ---- layout ---------------------------------------------------------- */
    float gy = tb + 46.f;
    float gh = h - (gy - y) - pad;
    float sideW = 330.f;
    float gw = w - pad*2.f - sideW - 14.f;

    if (a->cvFocus >= 0) {
        /* one large view, the rest as a filmstrip underneath */
        float stripH = 96.f;
        draw_camera(a, v, a->cvFocus, x + pad, gy, gw, gh - stripH - 10.f, 1);
        float sx = x + pad;
        float sw = (gw - 5*6.f) / 6.f;
        for (int i = 0; i < CV_CAMS; i++) {
            draw_camera(a, v, i, sx, gy + gh - stripH, sw, stripH, 0);
            sx += sw + 6.f;
        }
    } else {
        float cwid = (gw - 10.f) / 3.f;
        float chgt = (gh - 10.f) / 2.f;
        for (int i = 0; i < CV_CAMS; i++) {
            float px = x + pad + (i % 3) * (cwid + 5.f);
            float py = gy + (i / 3) * (chgt + 5.f);
            draw_camera(a, v, i, px, py, cwid, chgt, 0);
        }
    }

    /* ---- telemetry sidebar ---------------------------------------------- */
    float rx = x + pad + gw + 14.f;

    /* pipeline */
    float ph = 176.f;
    ui_card(rx, gy, sideW, ph, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "PIPELINE", rx + 18.f, gy + 15.f, font_track(TF_UI, 10, TW_BOLD, 2),
            C_V600, AL_L, AV_T);

    int dets = 0, tracks = 0, gal = 0;
    for (int i = 0; i < CV_CAMS; i++) { dets += v->st[i].nDet; tracks += v->st[i].confirmed; }
    for (int i = 0; i < CV_GALLERY; i++) if (v->gallery[i].live) gal++;

    struct { const char *k; char val[28]; } rows[5];
    snprintf(rows[0].val, 28, "%.2f ms", v->procMs);        rows[0].k = "Tick cost";
    snprintf(rows[1].val, 28, "%d", dets);                  rows[1].k = "Detections/frame";
    snprintf(rows[2].val, 28, "%d", tracks);                rows[2].k = "Confirmed tracks";
    snprintf(rows[3].val, 28, "%d", gal);                   rows[3].k = "Identity gallery";
    snprintf(rows[4].val, 28, "%d of %d", v->reidCorrect,
             v->reidCorrect + v->reidWrong);                rows[4].k = "Re-ID correct";
    for (int i = 0; i < 5; i++) {
        float ry = gy + 40.f + i*24.f;
        tx_draw(c, rows[i].k, rx + 18.f, ry, font_make(TF_UI, 11, TW_MED),
                C_INK_2, AL_L, AV_T);
        tx_draw(c, rows[i].val, rx + sideW - 18.f, ry,
                font_track(TF_MONO, 11, TW_SEMI, 0), C_INK, AL_R, AV_T);
    }

    /* scoring */
    float sy2 = gy + ph + 12.f;
    float sh2 = 186.f;
    ui_card(rx, sy2, sideW, sh2, R_LG);
    tx_draw(c, "TRACKER SCORING", rx + 18.f, sy2 + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_para(c, "Scored against the scene's ground truth, so these are measured "
               "rather than asserted.",
            rx + 18.f, sy2 + 32.f, sideW - 36.f, font_make(TF_UI, 10, TW_REG),
            C_INK_3, 1);

    float mota = v->mota;
    Color mc = mota > 0.85f ? C_OK : (mota > 0.6f ? C_WARN : C_DANGER);
    char mv[16]; snprintf(mv, sizeof mv, "%.1f%%", mota*100.f);
    tx_draw(c, "MOTA", rx + 18.f, sy2 + 68.f, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    tx_draw(c, mv, rx + 18.f, sy2 + 80.f, font_make(TF_DISPLAY, 26, TW_BOLD),
            mc, AL_L, AV_T);
    ui_meter(rx + 18.f, sy2 + 114.f, sideW - 36.f, 6.f,
             mota < 0.f ? 0.f : mota, col_alpha(mc,.5f), mc);

    struct { const char *k; int val; } sc[3] = {
        { "ID switches", v->totalIdSwitches },
        { "False positives", v->totalFalsePos },
        { "Missed", v->totalMissed },
    };
    for (int i = 0; i < 3; i++) {
        float px = rx + 18.f + i*((sideW - 36.f)/3.f);
        char b[16]; snprintf(b, sizeof b, "%d", sc[i].val);
        tx_draw(c, b, px, sy2 + 128.f, font_make(TF_UI, 15, TW_BOLD), C_INK,
                AL_L, AV_T);
        tx_clipped(c, sc[i].k, px, sy2 + 148.f, (sideW - 36.f)/3.f - 6.f,
                   font_make(TF_UI, 9, TW_MED), C_INK_3, AL_L, AV_T);
    }
    if (ui_button(uid("cvreset"), rx + sideW - 92.f, sy2 + 66.f, 74.f, 28.f,
                  "Reset", BTN_OUTLINE))
        vision_reset_metrics(v);

    /* ---- detector tuning -------------------------------------------------
     * The point of exposing these is that the scoring panel above is measured
     * against ground truth: push the miss rate up and MOTA visibly falls,
     * loosen the association gate and identity switches climb.  It turns the
     * panel from a claim into something a demonstrator can interrogate. */
    float ty2 = sy2 + sh2 + 12.f;
    float th2 = 168.f;
    ui_card(rx, ty2, sideW, th2, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "DETECTOR TUNING", rx + 18.f, ty2 + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    if (ui_button(uid("cvdefaults"), rx + sideW - 88.f, ty2 + 10.f, 70.f, 24.f,
                  "Defaults", BTN_GHOST)) {
        v->missRate = 0.040f; v->fpRate = 0.030f;
        v->jitter   = 0.005f; v->gate  = 0.085f;
        vision_reset_metrics(v);
    }

    struct { const char *k; float *val, lo, hi; const char *fmt; } kn[4] = {
        { "Missed detections", &v->missRate, 0.00f, 0.40f, "%.0f%%"  },
        { "False positives",   &v->fpRate,   0.00f, 0.50f, "%.2f/f"  },
        { "Position noise",    &v->jitter,   0.00f, 0.030f,"%.3f"    },
        { "Association gate",  &v->gate,     0.02f, 0.20f, "%.3f"    },
    };
    for (int i = 0; i < 4; i++) {
        float ky = ty2 + 40.f + i*31.f;
        tx_draw(c, kn[i].k, rx + 18.f, ky, font_make(TF_UI, 10, TW_MED),
                C_INK_2, AL_L, AV_T);
        char vb[24];
        float shown = (i == 0) ? *kn[i].val * 100.f : *kn[i].val;
        snprintf(vb, sizeof vb, kn[i].fmt, shown);
        tx_draw(c, vb, rx + sideW - 18.f, ky, font_track(TF_MONO, 10, TW_SEMI, 0),
                C_INK, AL_R, AV_T);
        float nv = ui_slider(uidi("cvknob", i), rx + 18.f, ky + 13.f,
                             sideW - 36.f, *kn[i].val, kn[i].lo, kn[i].hi);
        if (nv != *kn[i].val) { *kn[i].val = nv; }
    }

    /* alerts */
    float ay = ty2 + th2 + 12.f;
    float ah = gy + gh - ay;
    ui_card(rx, ay, sideW, ah, R_LG);
    tx_draw(c, "ALERTS", rx + 18.f, ay + 15.f, font_track(TF_UI, 10, TW_BOLD, 2),
            C_DANGER, AL_L, AV_T);
    char ac[24]; snprintf(ac, sizeof ac, "%d active", vision_alert_count(v));
    tx_draw(c, ac, rx + sideW - 18.f, ay + 15.f, font_make(TF_UI, 11, TW_SEMI),
            C_INK_3, AL_R, AV_T);

    int idx[CV_ALERTS], n = 0;
    for (int i = 0; i < CV_ALERTS; i++) if (v->alert[i].live) idx[n++] = i;
    for (int i = 1; i < n; i++) {
        int k = idx[i], j = i - 1;
        while (j >= 0 && v->alert[idx[j]].age > v->alert[k].age)
            { idx[j+1] = idx[j]; j--; }
        idx[j+1] = k;
    }

    if (!n) {
        icon_draw(c, IC_CHECK, rx + sideW*0.5f, ay + ah*0.5f - 12.f, 24.f, C_OK);
        tx_draw(c, "No active alerts", rx + sideW*0.5f, ay + ah*0.5f + 12.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_C, AV_M);
    } else {
        float rowH = 56.f;
        float off = ui_scroll_begin(uid("cvalerts"), rx + 6.f, ay + 38.f,
                                    sideW - 12.f, ah - 48.f, n*rowH + 6.f);
        for (int i = 0; i < n; i++) {
            CvAlert *al = &v->alert[idx[i]];
            float ry = ay + 38.f + i*rowH - off;
            if (ry + rowH < ay + 38.f || ry > ay + ah) continue;

            Color kc = al->kind == AL_UNATTENDED ? C_DANGER
                     : (al->kind == AL_COUNTERFLOW ? C_WARN
                     : (al->kind == AL_LOITER ? C_MAGENTA : C_INFO));
            int hov = ui_hit(rx + 12.f, ry, sideW - 24.f, rowH - 6.f);
            if (hov) ui_cursor(1);
            if (hov && a->in.pressed) a->cvFocus = al->cam;

            cv_rrect(c, rx + 12.f, ry, sideW - 24.f, rowH - 6.f, 9.f,
                     hov ? C_SURF_2 : col_alpha(kc, .07f));
            cv_rrect(c, rx + 12.f, ry + 8.f, 3.f, rowH - 22.f, 1.5f, kc);

            tx_backdrop(hov ? C_SURF_2 : C_SURF);
            tx_draw(c, cv_alert_name(al->kind), rx + 24.f, ry + 7.f,
                    font_make(TF_UI, 12, TW_SEMI), kc, AL_L, AV_T);
            char meta[24];
            snprintf(meta, sizeof meta, "%s  %.0fs", v->cam[al->cam].code, al->age);
            tx_draw(c, meta, rx + sideW - 24.f, ry + 7.f,
                    font_track(TF_MONO, 10, TW_MED, 0), C_INK_3, AL_R, AV_T);
            tx_clipped(c, al->text, rx + 24.f, ry + 26.f, sideW - 48.f,
                       font_make(TF_UI, 10, TW_REG), C_INK_2, AL_L, AV_T);
        }
        ui_scroll_end();
    }
}
