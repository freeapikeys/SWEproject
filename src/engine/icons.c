/* ==========================================================================
 *  AURA :: icons.c
 * ========================================================================== */

#include "icons.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* drawing state for the current icon: maps the 24x24 design grid to pixels */
static float g_ox, g_oy, g_s;

static float MX(float x) { return g_ox + x * g_s; }
static float MY(float y) { return g_oy + y * g_s; }

static void L(Canvas *c, float x0,float y0,float x1,float y1, Color col, float w)
{
    cv_line_round(c, MX(x0), MY(y0), MX(x1), MY(y1), col, w * g_s);
}

static void CIRC(Canvas *c, float x,float y,float r, Color col, float w)
{
    if (w <= 0.f) cv_circle(c, MX(x), MY(y), r*g_s, col);
    else          cv_circle_line(c, MX(x), MY(y), r*g_s, col, w*g_s);
}

static void RR(Canvas *c, float x,float y,float ww,float hh,float r,
               Color col, float w)
{
    if (w <= 0.f) cv_rrect(c, MX(x), MY(y), ww*g_s, hh*g_s, r*g_s, col);
    else cv_rrect_line(c, MX(x), MY(y), ww*g_s, hh*g_s, r*g_s, col, w*g_s);
}

/* polyline through the 24-grid */
static void PL(Canvas *c, const float *pts, int n, Color col, float w, int close)
{
    Path p; path_reset(&p);
    path_move(&p, MX(pts[0]), MY(pts[1]));
    for (int i = 1; i < n; i++) path_line(&p, MX(pts[i*2]), MY(pts[i*2+1]));
    if (close) path_close(&p);
    cv_stroke_col(c, &p, col, w * g_s);
}

static void POLY(Canvas *c, const float *pts, int n, Color col)
{
    Path p; path_reset(&p);
    path_move(&p, MX(pts[0]), MY(pts[1]));
    for (int i = 1; i < n; i++) path_line(&p, MX(pts[i*2]), MY(pts[i*2+1]));
    path_close(&p);
    cv_fill_col(c, &p, col);
}

void icon_draw(Canvas *c, int id, float cx, float cy, float size, Color col)
{
    icon_draw_w(c, id, cx, cy, size, col, 1.9f);
}

void icon_draw_w(Canvas *c, int id, float cx, float cy, float size, Color col,
                 float sw)
{
    g_s  = size / 24.f;
    g_ox = cx - size * 0.5f;
    g_oy = cy - size * 0.5f;
    const float w = sw;

    switch (id) {

    case IC_PLANE: {
        const float p[] = {12,2, 13.6f,3.4f, 14,10, 22,15, 22,17, 14,14.6f,
                           13.4f,19.4f, 16.4f,21.4f, 16.4f,22.6f, 12,21.4f,
                           7.6f,22.6f, 7.6f,21.4f, 10.6f,19.4f, 10,14.6f,
                           2,17, 2,15, 10,10, 10.4f,3.4f};
        POLY(c, p, 18, col);
    } break;

    case IC_TAKEOFF: {
        const float p[] = {2.5f,19, 21.5f,19};
        PL(c, p, 2, col, w, 0);
        const float q[] = {3,14.5f, 6.6f,15.6f, 11,11.4f, 8,4.6f, 9.9f,4.2f,
                           15,10.2f, 20.4f,8.6f, 21.6f,10.4f, 5.4f,16.6f};
        POLY(c, q, 9, col);
    } break;

    case IC_LANDING: {
        const float p[] = {2.5f,19, 21.5f,19};
        PL(c, p, 2, col, w, 0);
        const float q[] = {3.2f,7.4f, 5.2f,6.2f, 10.6f,10, 16.6f,5.6f,
                           18.2f,7, 15,14.2f, 21,16.4f, 20.6f,17.6f, 3.6f,15};
        POLY(c, q, 9, col);
    } break;

    case IC_RADAR:
        CIRC(c, 12,12, 9.2f, col, w*0.8f);
        CIRC(c, 12,12, 5.6f, col_alpha(col,.6f), w*0.7f);
        CIRC(c, 12,12, 2.0f, col, 0.f);
        L(c, 12,12, 19,7.4f, col, w);
        break;

    case IC_LUGGAGE:
        RR(c, 4,7.5f, 16,13, 2.6f, col, w);
        PL(c, (const float[]){8.6f,7.5f, 8.6f,4.4f, 15.4f,4.4f, 15.4f,7.5f},4,col,w,0);
        L(c, 9.4f,11, 9.4f,17, col_alpha(col,.7f), w*0.75f);
        L(c, 14.6f,11, 14.6f,17, col_alpha(col,.7f), w*0.75f);
        break;

    case IC_BELT:
        RR(c, 2.4f,9.5f, 19.2f,6, 3, col, w);
        CIRC(c, 7,18.5f, 2.1f, col, w);
        CIRC(c, 17,18.5f, 2.1f, col, w);
        RR(c, 8.5f,4.4f, 7,4.4f, 1.2f, col, w*0.85f);
        break;

    case IC_TAG:
        PL(c, (const float[]){12.6f,2.6f, 21.4f,11.4f, 12.4f,20.4f,
                              3.6f,11.6f, 3.6f,3.6f, 11.6f,3.6f}, 6, col, w, 1);
        CIRC(c, 8,8, 1.5f, col, 0.f);
        break;

    case IC_BOX:
        PL(c,(const float[]){12,2.6f, 21,7.2f, 21,16.8f, 12,21.4f, 3,16.8f,
                             3,7.2f},6,col,w,1);
        PL(c,(const float[]){3,7.2f, 12,11.8f, 21,7.2f},3,col,w,0);
        L(c, 12,11.8f, 12,21.4f, col, w);
        break;

    case IC_CHAT:
        PL(c,(const float[]){4,4.5f, 20,4.5f, 20,16, 12.8f,16, 8.2f,20.4f,
                             8.2f,16, 4,16},7,col,w,1);
        CIRC(c, 9,10.2f, 1.15f, col, 0.f);
        CIRC(c, 12,10.2f, 1.15f, col, 0.f);
        CIRC(c, 15,10.2f, 1.15f, col, 0.f);
        break;

    case IC_SPARK: {
        const float p[] = {12,2.2f, 14.1f,9.1f, 21,11.2f, 14.1f,13.3f,
                           12,20.2f, 9.9f,13.3f, 3,11.2f, 9.9f,9.1f};
        POLY(c, p, 8, col);
        const float q[] = {19,17, 19.8f,19.2f, 22,20, 19.8f,20.8f,
                           19,23, 18.2f,20.8f, 16,20, 18.2f,19.2f};
        POLY(c, q, 8, col_alpha(col,.75f));
    } break;

    case IC_BRAIN:
        PL(c,(const float[]){12,4, 9,3.2f, 6.4f,5, 6,8, 3.8f,10, 4.6f,13.4f,
                             4,16, 6.4f,19, 9.4f,19.6f, 12,18.6f},10,col,w,0);
        PL(c,(const float[]){12,4, 15,3.2f, 17.6f,5, 18,8, 20.2f,10,
                             19.4f,13.4f, 20,16, 17.6f,19, 14.6f,19.6f,
                             12,18.6f},10,col,w,0);
        L(c, 12,4, 12,18.6f, col_alpha(col,.55f), w*0.8f);
        CIRC(c, 8.8f,9.4f, 1.05f, col, 0.f);
        CIRC(c, 15.2f,13.2f, 1.05f, col, 0.f);
        break;

    case IC_CPU:
        RR(c, 6,6, 12,12, 2.2f, col, w);
        RR(c, 9.6f,9.6f, 4.8f,4.8f, 1.2f, col, w*0.85f);
        for (int i = 0; i < 3; i++) {
            float t = 8.6f + i*3.4f;
            L(c, t,2.6f, t,6, col, w*0.85f);
            L(c, t,18, t,21.4f, col, w*0.85f);
            L(c, 2.6f,t, 6,t, col, w*0.85f);
            L(c, 18,t, 21.4f,t, col, w*0.85f);
        }
        break;

    case IC_SHIELD:
        PL(c,(const float[]){12,2.6f, 20,5.8f, 20,11.6f, 12,21.4f,
                             4,11.6f, 4,5.8f},6,col,w,1);
        PL(c,(const float[]){8.4f,11.8f, 11,14.4f, 15.8f,8.8f},3,col,w,0);
        break;

    case IC_SCAN:
        PL(c,(const float[]){3,8.4f, 3,3.4f, 8,3.4f},3,col,w,0);
        PL(c,(const float[]){16,3.4f, 21,3.4f, 21,8.4f},3,col,w,0);
        PL(c,(const float[]){21,15.6f, 21,20.6f, 16,20.6f},3,col,w,0);
        PL(c,(const float[]){8,20.6f, 3,20.6f, 3,15.6f},3,col,w,0);
        L(c, 3,12, 21,12, col, w);
        break;

    case IC_USERS:
        CIRC(c, 9.4f,8.2f, 3.6f, col, w);
        PL(c,(const float[]){2.6f,20, 3.4f,16.2f, 9.4f,14.4f, 15.4f,16.2f,
                             16.2f,20},5,col,w,0);
        PL(c,(const float[]){16.4f,5.2f, 19.6f,7.2f, 19.6f,10.4f,
                             16.4f,12.2f},4,col,w*0.85f,0);
        PL(c,(const float[]){18,14.8f, 21.4f,16.6f, 21.8f,20},3,col,w*0.85f,0);
        break;

    case IC_USER:
        CIRC(c, 12,8, 4.2f, col, w);
        PL(c,(const float[]){4.4f,20.4f, 5.4f,16.2f, 12,14, 18.6f,16.2f,
                             19.6f,20.4f},5,col,w,0);
        break;

    case IC_GATE:
        L(c, 4,21, 4,4, col, w);
        L(c, 20,21, 20,4, col, w);
        PL(c,(const float[]){4,4, 12,7.4f, 20,4},3,col,w,0);
        L(c, 12,7.4f, 12,21, col_alpha(col,.65f), w*0.8f);
        break;

    case IC_TICKET:
        PL(c,(const float[]){3,7, 21,7, 21,10.4f},3,col,w,0);
        PL(c,(const float[]){21,13.6f, 21,17, 3,17, 3,13.6f},4,col,w,0);
        PL(c,(const float[]){3,10.4f, 3,7},2,col,w,0);
        CIRC(c, 21,12, 1.9f, col, w*0.9f);
        L(c, 8.6f,10, 8.6f,14, col_alpha(col,.7f), w*0.8f);
        break;

    case IC_SEAT:
        PL(c,(const float[]){7,4.4f, 7,13.4f, 17.4f,13.4f},3,col,w,0);
        PL(c,(const float[]){4.6f,4.4f, 4.6f,16.6f, 19.6f,16.6f},3,col,w,0);
        L(c, 7,19.6f, 19.6f,19.6f, col_alpha(col,.6f), w*0.85f);
        break;

    case IC_PASSPORT:
        RR(c, 5,3, 14,18, 2.2f, col, w);
        CIRC(c, 12,10, 3.2f, col, w*0.85f);
        L(c, 8.6f,17, 15.4f,17, col, w*0.85f);
        break;

    case IC_CHART:
        L(c, 4,20, 20,20, col, w);
        RR(c, 5.6f,12, 3.2f,7, 1, col, 0.f);
        RR(c, 10.4f,7.4f, 3.2f,11.6f, 1, col, 0.f);
        RR(c, 15.2f,9.8f, 3.2f,9.2f, 1, col_alpha(col,.7f), 0.f);
        break;

    case IC_TREND:
        PL(c,(const float[]){3,16.6f, 9,10.6f, 13,14.6f, 21,6.2f},4,col,w,0);
        PL(c,(const float[]){15.6f,6.2f, 21,6.2f, 21,11.6f},3,col,w,0);
        break;

    case IC_GAUGE: {
        Path p; path_reset(&p);
        path_ring_arc(&p, MX(12), MY(14.5f), 9.2f*g_s, (9.2f-w*1.05f)*g_s,
                      (float)M_PI*1.03f, (float)M_PI*1.97f);
        cv_fill_col(c, &p, col);
        L(c, 12,14.5f, 16.2f,9.6f, col, w);
        CIRC(c, 12,14.5f, 1.5f, col, 0.f);
    } break;

    case IC_LAYERS:
        PL(c,(const float[]){12,2.8f, 21,7.4f, 12,12, 3,7.4f},4,col,w,1);
        PL(c,(const float[]){3,12, 12,16.6f, 21,12},3,col,w,0);
        PL(c,(const float[]){3,16.4f, 12,21, 21,16.4f},3,col,w,0);
        break;

    case IC_CLOCK:
        CIRC(c, 12,12, 9.2f, col, w);
        PL(c,(const float[]){12,6.6f, 12,12, 16.2f,14.2f},3,col,w,0);
        break;

    case IC_CALENDAR:
        RR(c, 3.4f,5.4f, 17.2f,15.4f, 2.4f, col, w);
        L(c, 3.4f,10, 20.6f,10, col, w);
        L(c, 8,3, 8,7, col, w);
        L(c, 16,3, 16,7, col, w);
        CIRC(c, 8.6f,14, 1.05f, col, 0.f);
        CIRC(c, 12.6f,14, 1.05f, col, 0.f);
        break;

    case IC_SEARCH:
        CIRC(c, 10.6f,10.6f, 6.6f, col, w);
        L(c, 15.6f,15.6f, 20.6f,20.6f, col, w*1.05f);
        break;

    case IC_FILTER:
        PL(c,(const float[]){3.2f,5, 20.8f,5, 14,12.6f, 14,19.6f,
                             10,17.4f, 10,12.6f},6,col,w,1);
        break;

    case IC_CHECK:
        PL(c,(const float[]){4.6f,12.6f, 9.6f,17.6f, 19.4f,6.6f},3,col,w*1.15f,0);
        break;

    case IC_CLOSE:
        L(c, 6,6, 18,18, col, w*1.1f);
        L(c, 18,6, 6,18, col, w*1.1f);
        break;

    case IC_ALERT:
        PL(c,(const float[]){12,3, 22,20.4f, 2,20.4f},3,col,w,1);
        L(c, 12,9.4f, 12,14.6f, col, w*1.05f);
        CIRC(c, 12,17.6f, 1.15f, col, 0.f);
        break;

    case IC_INFO:
        CIRC(c, 12,12, 9.2f, col, w);
        L(c, 12,11, 12,16.4f, col, w*1.05f);
        CIRC(c, 12,7.6f, 1.15f, col, 0.f);
        break;

    case IC_ARROW_L:
        L(c, 20,12, 4,12, col, w);
        PL(c,(const float[]){10,6, 4,12, 10,18},3,col,w,0);
        break;
    case IC_ARROW_R:
        L(c, 4,12, 20,12, col, w);
        PL(c,(const float[]){14,6, 20,12, 14,18},3,col,w,0);
        break;
    case IC_ARROW_U:
        L(c, 12,20, 12,4, col, w);
        PL(c,(const float[]){6,10, 12,4, 18,10},3,col,w,0);
        break;
    case IC_ARROW_D:
        L(c, 12,4, 12,20, col, w);
        PL(c,(const float[]){6,14, 12,20, 18,14},3,col,w,0);
        break;

    case IC_CHEV_L: PL(c,(const float[]){15,5, 8,12, 15,19},3,col,w*1.1f,0); break;
    case IC_CHEV_R: PL(c,(const float[]){9,5, 16,12, 9,19},3,col,w*1.1f,0);  break;
    case IC_CHEV_U: PL(c,(const float[]){5,15, 12,8, 19,15},3,col,w*1.1f,0); break;
    case IC_CHEV_D: PL(c,(const float[]){5,9, 12,16, 19,9},3,col,w*1.1f,0);  break;

    case IC_PLAY:  POLY(c,(const float[]){7.4f,4.6f, 19.4f,12, 7.4f,19.4f},3,col); break;
    case IC_PAUSE:
        RR(c, 7,4.6f, 3.6f,14.8f, 1.2f, col, 0.f);
        RR(c, 13.4f,4.6f, 3.6f,14.8f, 1.2f, col, 0.f);
        break;
    case IC_FFWD:
        POLY(c,(const float[]){3.4f,5.4f, 12,12, 3.4f,18.6f},3,col);
        POLY(c,(const float[]){12,5.4f, 20.6f,12, 12,18.6f},3,col);
        break;

    case IC_REFRESH: {
        Path p; path_reset(&p);
        path_arc(&p, MX(12), MY(12), 8.2f*g_s, -2.5f, 1.9f);
        cv_stroke_col(c, &p, col, w*g_s);
        POLY(c,(const float[]){15.4f,2.2f, 21,4.4f, 16.6f,8.4f},3,col);
    } break;

    case IC_SETTINGS:
        CIRC(c, 12,12, 3.4f, col, w);
        for (int i = 0; i < 8; i++) {
            float a = (float)(i * M_PI / 4.0);
            float x0 = 12 + cosf(a)*6.2f, y0 = 12 + sinf(a)*6.2f;
            float x1 = 12 + cosf(a)*9.4f, y1 = 12 + sinf(a)*9.4f;
            L(c, x0,y0, x1,y1, col, w);
        }
        break;

    case IC_BELL:
        PL(c,(const float[]){6.2f,17, 6.2f,10.6f, 8,6.4f, 12,5, 16,6.4f,
                             17.8f,10.6f, 17.8f,17},7,col,w,0);
        L(c, 4,17, 20,17, col, w);
        PL(c,(const float[]){10,20, 12,21.4f, 14,20},3,col,w,0);
        break;

    case IC_LOCK:
        RR(c, 4.6f,10.6f, 14.8f,10.4f, 2.4f, col, w);
        PL(c,(const float[]){8,10.6f, 8,7.4f, 12,4.6f, 16,7.4f,
                             16,10.6f},5,col,w,0);
        CIRC(c, 12,15.6f, 1.5f, col, 0.f);
        break;

    case IC_PIN:
        PL(c,(const float[]){12,21.4f, 5.4f,11.6f, 6.6f,6, 12,3, 17.4f,6,
                             18.6f,11.6f},6,col,w,1);
        CIRC(c, 12,9.6f, 2.6f, col, w*0.85f);
        break;

    case IC_FILE:
        PL(c,(const float[]){13.6f,2.8f, 19.4f,8.6f, 19.4f,21, 4.6f,21,
                             4.6f,2.8f},5,col,w,1);
        PL(c,(const float[]){13.6f,2.8f, 13.6f,8.6f, 19.4f,8.6f},3,col,w,0);
        break;

    case IC_DATABASE:
        cv_circle_line(c, MX(12), MY(6.4f), 8.2f*g_s, col, w*g_s);
        PL(c,(const float[]){3.8f,6.4f, 3.8f,17.6f},2,col,w,0);
        PL(c,(const float[]){20.2f,6.4f, 20.2f,17.6f},2,col,w,0);
        { Path p; path_reset(&p);
          path_arc(&p, MX(12), MY(12), 8.2f*g_s, 0.28f, (float)M_PI-0.28f);
          cv_stroke_col(c, &p, col, w*g_s);
          path_reset(&p);
          path_arc(&p, MX(12), MY(17.6f), 8.2f*g_s, 0.28f, (float)M_PI-0.28f);
          cv_stroke_col(c, &p, col, w*g_s); }
        break;

    case IC_SAVE:
        RR(c, 3.6f,3.6f, 16.8f,16.8f, 2.4f, col, w);
        RR(c, 7.4f,3.6f, 9.2f,6.2f, 1, col, w*0.85f);
        RR(c, 7,13, 10,7.4f, 1, col, w*0.85f);
        break;

    case IC_FOLDER:
        PL(c,(const float[]){3,19.4f, 3,5.4f, 9.4f,5.4f, 11.4f,8, 21,8,
                             21,19.4f},6,col,w,1);
        break;

    case IC_PLUS:  L(c,12,5,12,19,col,w*1.15f); L(c,5,12,19,12,col,w*1.15f); break;
    case IC_MINUS: L(c,5,12,19,12,col,w*1.15f); break;

    case IC_TRASH:
        PL(c,(const float[]){5.4f,6.6f, 6.6f,20.4f, 17.4f,20.4f,
                             18.6f,6.6f},4,col,w,0);
        L(c, 3.4f,6.6f, 20.6f,6.6f, col, w);
        PL(c,(const float[]){9.4f,6.6f, 9.4f,3.6f, 14.6f,3.6f,
                             14.6f,6.6f},4,col,w,0);
        L(c, 10.2f,10.4f, 10.6f,16.8f, col_alpha(col,.7f), w*0.8f);
        L(c, 13.8f,10.4f, 13.4f,16.8f, col_alpha(col,.7f), w*0.8f);
        break;

    case IC_EDIT:
        PL(c,(const float[]){4,20, 4.8f,15.6f, 16.4f,4, 20,7.6f,
                             8.4f,19.2f},5,col,w,1);
        L(c, 14.2f,6.2f, 17.8f,9.8f, col, w*0.85f);
        break;

    case IC_EYE:
        PL(c,(const float[]){2.4f,12, 6.4f,6.6f, 12,5.4f, 17.6f,6.6f,
                             21.6f,12, 17.6f,17.4f, 12,18.6f,
                             6.4f,17.4f},8,col,w,1);
        CIRC(c, 12,12, 3.2f, col, w*0.9f);
        break;

    case IC_SEND:
        POLY(c,(const float[]){2.6f,11.4f, 21.4f,3.2f, 13.6f,21.4f,
                               11,13.4f},4,col);
        break;

    case IC_MIC:
        RR(c, 9,2.8f, 6,11.4f, 3, col, w);
        PL(c,(const float[]){5.6f,11.4f, 5.6f,13, 12,17, 18.4f,13,
                             18.4f,11.4f},5,col,w,0);
        L(c, 12,17, 12,21.2f, col, w);
        break;

    case IC_GRID:
        RR(c, 3.6f,3.6f, 7,7, 1.6f, col, w);
        RR(c, 13.4f,3.6f, 7,7, 1.6f, col, w);
        RR(c, 3.6f,13.4f, 7,7, 1.6f, col, w);
        RR(c, 13.4f,13.4f, 7,7, 1.6f, col, w);
        break;

    case IC_SUN:
        CIRC(c, 12,12, 4.6f, col, w);
        for (int i = 0; i < 8; i++) {
            float a = (float)(i * M_PI / 4.0);
            L(c, 12+cosf(a)*7.2f, 12+sinf(a)*7.2f,
                 12+cosf(a)*9.6f, 12+sinf(a)*9.6f, col, w);
        }
        break;

    case IC_CLOUD:
        PL(c,(const float[]){7,18.4f, 5,17.4f, 4,15, 5.2f,12.4f, 8,11.6f,
                             9.4f,8.4f, 13,7, 16.4f,8.6f, 17.4f,11.6f,
                             20,12.8f, 20.4f,16, 18.4f,18.4f},12,col,w,1);
        break;

    case IC_RAIN:
        PL(c,(const float[]){7,14.4f, 5,13.4f, 4.2f,11, 5.4f,8.6f, 8.2f,7.8f,
                             9.6f,5, 13.2f,3.8f, 16.4f,5.4f, 17.4f,8.2f,
                             19.8f,9.4f, 20.2f,12.4f, 18.4f,14.4f},12,col,w,1);
        L(c, 8.4f,17.4f, 7.2f,20.6f, col, w);
        L(c, 12.4f,17.4f, 11.2f,20.6f, col, w);
        L(c, 16.4f,17.4f, 15.2f,20.6f, col, w);
        break;

    case IC_WIND:
        PL(c,(const float[]){3,8.4f, 13.4f,8.4f, 15.6f,6.2f, 14.2f,4},4,col,w,0);
        PL(c,(const float[]){3,13, 17.4f,13, 19.6f,10.8f, 18.2f,8.6f},4,col,w,0);
        PL(c,(const float[]){3,17.6f, 12.4f,17.6f, 14.6f,19.8f,
                             13.2f,21.8f},4,col,w,0);
        break;

    case IC_FUEL:
        RR(c, 4,3.4f, 10.4f,17.2f, 2, col, w);
        L(c, 4,9.4f, 14.4f,9.4f, col, w*0.85f);
        PL(c,(const float[]){14.4f,7.4f, 18.4f,10.4f, 18.4f,16.4f,
                             20.4f,16.4f},4,col,w*0.85f,0);
        break;

    case IC_TRUCK:
        RR(c, 2.4f,7, 11.4f,9, 1.6f, col, w);
        PL(c,(const float[]){13.8f,10.4f, 18,10.4f, 21.4f,14, 21.4f,16,
                             13.8f,16},5,col,w,0);
        CIRC(c, 7,18.4f, 2.2f, col, w);
        CIRC(c, 17.6f,18.4f, 2.2f, col, w);
        break;

    case IC_TOWER:
        PL(c,(const float[]){9.4f,21, 10.6f,10, 13.4f,10, 14.6f,21},4,col,w,1);
        RR(c, 7.4f,4.4f, 9.2f,5.6f, 1.6f, col, w);
        L(c, 5,21, 19,21, col, w);
        L(c, 12,4.4f, 12,1.8f, col, w*0.85f);
        break;

    case IC_RUNWAY:
        PL(c,(const float[]){8.4f,2.6f, 15.6f,2.6f, 19,21.4f, 5,21.4f},4,col,w,1);
        L(c, 12,6.4f, 12,9.4f, col_alpha(col,.8f), w*0.9f);
        L(c, 12,12, 12,15, col_alpha(col,.8f), w*0.9f);
        L(c, 12,17.6f, 12,20, col_alpha(col,.8f), w*0.9f);
        break;

    case IC_BOLT:
        POLY(c,(const float[]){13.6f,2, 5,13.4f, 11,13.4f, 10.4f,22,
                               19,10.6f, 13,10.6f},6,col);
        break;

    case IC_HEART:
        PL(c,(const float[]){12,20.6f, 4.2f,13, 3.4f,8.6f, 6.4f,5, 10,5.4f,
                             12,8, 14,5.4f, 17.6f,5, 20.6f,8.6f,
                             19.8f,13},10,col,w,1);
        break;

    case IC_STAR: {
        Path p; path_reset(&p);
        path_star(&p, MX(12), MY(12), 9.4f*g_s, 4.f*g_s, 5, -1.5708f);
        cv_fill_col(c, &p, col);
    } break;

    case IC_GLOBE:
        CIRC(c, 12,12, 9.2f, col, w);
        L(c, 2.8f,12, 21.2f,12, col, w*0.85f);
        PL(c,(const float[]){12,2.8f, 8.6f,7, 8,12, 8.6f,17, 12,21.2f},5,col,w*0.85f,0);
        PL(c,(const float[]){12,2.8f, 15.4f,7, 16,12, 15.4f,17,
                             12,21.2f},5,col,w*0.85f,0);
        break;

    case IC_MENU:
        L(c, 4,7, 20,7, col, w*1.1f);
        L(c, 4,12, 20,12, col, w*1.1f);
        L(c, 4,17, 20,17, col, w*1.1f);
        break;

    case IC_MORE:
        CIRC(c, 5.4f,12, 1.7f, col, 0.f);
        CIRC(c, 12,12, 1.7f, col, 0.f);
        CIRC(c, 18.6f,12, 1.7f, col, 0.f);
        break;

    case IC_LINK:
        PL(c,(const float[]){10,14, 14,10},2,col,w,0);
        PL(c,(const float[]){9.4f,17.4f, 7,19.8f, 4.2f,17, 6.6f,14.6f},4,col,w,0);
        PL(c,(const float[]){14.6f,6.6f, 17,4.2f, 19.8f,7, 17.4f,9.4f},4,col,w,0);
        break;

    case IC_DOWNLOAD:
        L(c, 12,3.4f, 12,15.4f, col, w);
        PL(c,(const float[]){6.6f,10, 12,15.4f, 17.4f,10},3,col,w,0);
        PL(c,(const float[]){3.6f,18.6f, 3.6f,20.6f, 20.4f,20.6f,
                             20.4f,18.6f},4,col,w,0);
        break;

    default:
        CIRC(c, 12,12, 8, col, w);
        break;
    }
}
