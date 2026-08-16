/* ==========================================================================
 *  AURA  ::  canvas.c   --   anti-aliased software rasteriser
 *
 *  Two rasterisation paths live here:
 *
 *   1. A signed-distance-field rasteriser for rounded rectangles, circles and
 *      their outlines.  These make up the overwhelming majority of interface
 *      geometry, and an SDF gives mathematically exact anti-aliasing at a
 *      fraction of the cost of tessellating the shape into a polygon.
 *
 *   2. A general scanline coverage rasteriser with an active-edge list and
 *      5x vertical supersampling with exact horizontal span coverage, used
 *      for arbitrary paths: the airfield map, aircraft silhouettes, icons,
 *      charts and the baggage conveyor geometry.  Non-zero winding fill.
 *
 *  Compositing is straight source-over into a 32-bit BGRA DIB section whose
 *  device context is shared with GDI so that text can be laid on top.
 * ========================================================================== */

#include "canvas.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SUBS      5        /* vertical subsamples per scanline               */
#define MAX_EDGES 8192

/* ==========================================================================
 *  small helpers
 * ========================================================================== */

float cv_clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

float cv_lerp(float a, float b, float t) { return a + (b - a) * t; }

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

/* divide by 255 with rounding, without a division; safe for negatives */
static inline int div255(int v)
{
    if (v < 0) { int u = -v + 128; return -((u + (u >> 8)) >> 8); }
    v += 128; return (v + (v >> 8)) >> 8;
}

static inline int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

Color col_alpha(Color c, float a)
{
    int na = (int)(COL_A(c) * cv_clampf(a, 0.f, 1.f) + 0.5f);
    return (c & 0x00FFFFFFu) | ((uint32_t)clamp255(na) << 24);
}

Color col_mix(Color a, Color b, float t)
{
    t = cv_clampf(t, 0.f, 1.f);
    int r = (int)(COL_R(a) + (COL_R(b) - COL_R(a)) * t + 0.5f);
    int g = (int)(COL_G(a) + (COL_G(b) - COL_G(a)) * t + 0.5f);
    int bl= (int)(COL_B(a) + (COL_B(b) - COL_B(a)) * t + 0.5f);
    int al= (int)(COL_A(a) + (COL_A(b) - COL_A(a)) * t + 0.5f);
    return RGBA(clamp255(r), clamp255(g), clamp255(bl), clamp255(al));
}

Color col_lighten(Color c, float amt) { return col_mix(c, RGBA(255,255,255,COL_A(c)), amt); }
Color col_darken (Color c, float amt) { return col_mix(c, RGBA(0,0,0,COL_A(c)),       amt); }

/* ==========================================================================
 *  paint
 * ========================================================================== */

Paint paint_solid(Color c)
{
    Paint p; memset(&p, 0, sizeof p);
    p.kind = PAINT_SOLID; p.c0 = p.c1 = c;
    return p;
}

Paint paint_linear(float x0,float y0,float x1,float y1, Color a, Color b)
{
    Paint p; memset(&p, 0, sizeof p);
    p.kind = PAINT_LINEAR;
    p.x0=x0; p.y0=y0; p.x1=x1; p.y1=y1; p.c0=a; p.c1=b;
    return p;
}

Paint paint_radial(float cx,float cy,float r, Color inner, Color outer)
{
    Paint p; memset(&p, 0, sizeof p);
    p.kind = PAINT_RADIAL;
    p.cxp=cx; p.cyp=cy; p.r=(r<=0.f?1.f:r); p.c0=inner; p.c1=outer;
    return p;
}

static inline Color paint_at(const Paint *p, float x, float y)
{
    float t;
    if (p->kind == PAINT_SOLID) return p->c0;
    if (p->kind == PAINT_LINEAR) {
        float dx = p->x1 - p->x0, dy = p->y1 - p->y0;
        float len2 = dx*dx + dy*dy;
        if (len2 < 1e-6f) return p->c0;
        t = ((x - p->x0)*dx + (y - p->y0)*dy) / len2;
    } else {
        float dx = x - p->cxp, dy = y - p->cyp;
        t = sqrtf(dx*dx + dy*dy) / p->r;
    }
    return col_mix(p->c0, p->c1, cv_clampf(t, 0.f, 1.f));
}

/* ==========================================================================
 *  path construction
 * ========================================================================== */

void path_reset(Path *p)
{
    p->npt = 0; p->nsub = 0; p->cx = p->cy = p->sx = p->sy = 0.f;
}

static void path_push(Path *p, float x, float y)
{
    if (p->npt >= PATH_MAX_PTS) return;
    p->pts[p->npt*2+0] = x;
    p->pts[p->npt*2+1] = y;
    p->npt++;
    p->cx = x; p->cy = y;
}

void path_move(Path *p, float x, float y)
{
    if (p->nsub >= PATH_MAX_SUBS) return;
    p->sub[p->nsub++] = p->npt;
    p->sx = x; p->sy = y;
    path_push(p, x, y);
}

void path_line(Path *p, float x, float y)
{
    if (p->nsub == 0) { path_move(p, x, y); return; }
    path_push(p, x, y);
}

void path_cubic(Path *p, float c1x,float c1y,float c2x,float c2y,float x,float y)
{
    float x0 = p->cx, y0 = p->cy;
    /* adaptive-ish: segment count from control polygon length */
    float d = fabsf(c1x-x0)+fabsf(c1y-y0)+fabsf(c2x-c1x)+fabsf(c2y-c1y)
            + fabsf(x-c2x)+fabsf(y-c2y);
    int n = (int)(d / 3.0f); if (n < 6) n = 6; if (n > 64) n = 64;
    for (int i = 1; i <= n; i++) {
        float t = (float)i / n, u = 1.f - t;
        float bx = u*u*u*x0 + 3*u*u*t*c1x + 3*u*t*t*c2x + t*t*t*x;
        float by = u*u*u*y0 + 3*u*u*t*c1y + 3*u*t*t*c2y + t*t*t*y;
        path_push(p, bx, by);
    }
}

void path_quad(Path *p, float cx,float cy,float x,float y)
{
    float x0 = p->cx, y0 = p->cy;
    path_cubic(p, x0 + 2.f/3.f*(cx-x0), y0 + 2.f/3.f*(cy-y0),
                  x  + 2.f/3.f*(cx-x),  y  + 2.f/3.f*(cy-y), x, y);
}

void path_close(Path *p)
{
    if (p->nsub > 0) path_push(p, p->sx, p->sy);
}

void path_rect(Path *p, float x,float y,float w,float h)
{
    path_move(p, x, y); path_line(p, x+w, y);
    path_line(p, x+w, y+h); path_line(p, x, y+h); path_close(p);
}

void path_rrect4(Path *p, float x,float y,float w,float h,
                 float tl,float tr,float br,float bl)
{
    float m = (w < h ? w : h) * 0.5f;
    if (tl > m) tl = m;
    if (tr > m) tr = m;
    if (br > m) br = m;
    if (bl > m) bl = m;
    const float K = 0.5522847498f;
    path_move(p, x+tl, y);
    path_line(p, x+w-tr, y);
    if (tr > 0) path_cubic(p, x+w-tr+tr*K, y, x+w, y+tr-tr*K, x+w, y+tr);
    path_line(p, x+w, y+h-br);
    if (br > 0) path_cubic(p, x+w, y+h-br+br*K, x+w-br+br*K, y+h, x+w-br, y+h);
    path_line(p, x+bl, y+h);
    if (bl > 0) path_cubic(p, x+bl-bl*K, y+h, x, y+h-bl+bl*K, x, y+h-bl);
    path_line(p, x, y+tl);
    if (tl > 0) path_cubic(p, x, y+tl-tl*K, x+tl-tl*K, y, x+tl, y);
    path_close(p);
}

void path_rrect(Path *p, float x,float y,float w,float h,float r)
{
    path_rrect4(p, x, y, w, h, r, r, r, r);
}

void path_ellipse(Path *p, float cx,float cy,float rx,float ry)
{
    int n = (int)(6 + (rx + ry) * 0.7f); if (n < 12) n = 12; if (n > 180) n = 180;
    for (int i = 0; i < n; i++) {
        float a = (float)(2.0 * M_PI * i / n);
        float px = cx + cosf(a)*rx, py = cy + sinf(a)*ry;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    path_close(p);
}

void path_circle(Path *p, float cx,float cy,float r) { path_ellipse(p, cx, cy, r, r); }

void path_arc(Path *p, float cx,float cy,float r,float a0,float a1)
{
    int n = (int)(fabsf(a1-a0) * r * 0.35f); if (n < 4) n = 4; if (n > 220) n = 220;
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1-a0) * i / n;
        float px = cx + cosf(a)*r, py = cy + sinf(a)*r;
        if (i == 0 && p->nsub == 0) path_move(p, px, py);
        else if (i == 0)            path_move(p, px, py);
        else                        path_line(p, px, py);
    }
}

void path_ring(Path *p, float cx,float cy,float rO,float rI)
{
    int n = (int)(8 + rO); if (n < 16) n = 16; if (n > 200) n = 200;
    for (int i = 0; i < n; i++) {                 /* outer CCW              */
        float a = (float)(2.0*M_PI*i/n);
        float px = cx + cosf(a)*rO, py = cy + sinf(a)*rO;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    path_close(p);
    for (int i = 0; i < n; i++) {                 /* inner CW -> hole       */
        float a = (float)(-2.0*M_PI*i/n);
        float px = cx + cosf(a)*rI, py = cy + sinf(a)*rI;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    path_close(p);
}

void path_ring_arc(Path *p,float cx,float cy,float rO,float rI,float a0,float a1)
{
    int n = (int)(fabsf(a1-a0) * rO * 0.35f); if (n < 4) n = 4; if (n > 220) n = 220;
    for (int i = 0; i <= n; i++) {
        float a = a0 + (a1-a0)*i/n;
        float px = cx + cosf(a)*rO, py = cy + sinf(a)*rO;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    for (int i = n; i >= 0; i--) {
        float a = a0 + (a1-a0)*i/n;
        path_line(p, cx + cosf(a)*rI, cy + sinf(a)*rI);
    }
    path_close(p);
}

void path_poly(Path *p, const float *xy, int n)
{
    for (int i = 0; i < n; i++) {
        if (i == 0) path_move(p, xy[0], xy[1]);
        else        path_line(p, xy[i*2], xy[i*2+1]);
    }
    path_close(p);
}

void path_ngon(Path *p, float cx,float cy,float r,int sides,float rot)
{
    for (int i = 0; i < sides; i++) {
        float a = rot + (float)(2.0*M_PI*i/sides);
        float px = cx + cosf(a)*r, py = cy + sinf(a)*r;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    path_close(p);
}

void path_star(Path *p, float cx,float cy,float rO,float rI,int pts,float rot)
{
    for (int i = 0; i < pts*2; i++) {
        float r = (i & 1) ? rI : rO;
        float a = rot + (float)(M_PI*i/pts);
        float px = cx + cosf(a)*r, py = cy + sinf(a)*r;
        if (i == 0) path_move(p, px, py); else path_line(p, px, py);
    }
    path_close(p);
}

/* ==========================================================================
 *  canvas lifecycle
 * ========================================================================== */

int cv_create(Canvas *c, int w, int h)
{
    memset(c, 0, sizeof *c);
    return cv_resize(c, w, h);
}

void cv_destroy(Canvas *c)
{
    if (c->hdc) {
        if (c->hbmOld) SelectObject(c->hdc, c->hbmOld);
        DeleteDC(c->hdc);
    }
    if (c->hbm)  DeleteObject(c->hbm);
    free(c->cov); free(c->xs); free(c->dirs);
    memset(c, 0, sizeof *c);
}

int cv_resize(Canvas *c, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (c->w == w && c->h == h && c->px) return 1;

    if (c->hdc) { if (c->hbmOld) SelectObject(c->hdc, c->hbmOld); DeleteDC(c->hdc); c->hdc = NULL; }
    if (c->hbm) { DeleteObject(c->hbm); c->hbm = NULL; }
    free(c->cov); free(c->xs); free(c->dirs);

    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;            /* top-down                 */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(NULL);
    void *bits = NULL;
    c->hbm = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    c->hdc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    if (!c->hbm || !c->hdc) return 0;

    c->hbmOld = SelectObject(c->hdc, c->hbm);
    c->px = (uint32_t *)bits;
    c->w = w; c->h = h;

    c->cov  = (float *)malloc(sizeof(float) * (size_t)(w + 8));
    c->xs   = (float *)malloc(sizeof(float) * MAX_EDGES);
    c->dirs = (int   *)malloc(sizeof(int)   * MAX_EDGES);

    c->nclip = 0;
    c->clip[0][0] = 0; c->clip[0][1] = 0; c->clip[0][2] = w; c->clip[0][3] = h;

    SetBkMode(c->hdc, TRANSPARENT);
    SetGraphicsMode(c->hdc, GM_ADVANCED);
    return (c->px && c->cov && c->xs && c->dirs);
}

void cv_gdi_used(Canvas *c) { c->gdiDirty = 1; }

void cv_sync(Canvas *c)
{
    if (c->gdiDirty) { GdiFlush(); c->gdiDirty = 0; }
}

/* ---------------------------------------------------------------- clip --- */

void cv_clip_push(Canvas *c, float x,float y,float w,float h)
{
    int nx0 = (int)floorf(x), ny0 = (int)floorf(y);
    int nx1 = (int)ceilf(x+w), ny1 = (int)ceilf(y+h);
    int *cur = c->clip[c->nclip];
    int px0 = cur[0], py0 = cur[1], px1 = cur[2], py1 = cur[3];
    if (c->nclip + 1 < CLIP_STACK_MAX) c->nclip++;
    int *n = c->clip[c->nclip];
    n[0] = imax(nx0, px0); n[1] = imax(ny0, py0);
    n[2] = imin(nx1, px1); n[3] = imin(ny1, py1);
    if (n[2] < n[0]) n[2] = n[0];
    if (n[3] < n[1]) n[3] = n[1];

    HRGN rgn = CreateRectRgn(n[0], n[1], n[2], n[3]);
    SelectClipRgn(c->hdc, rgn);
    DeleteObject(rgn);
}

void cv_clip_pop(Canvas *c)
{
    if (c->nclip > 0) c->nclip--;
    int *n = c->clip[c->nclip];
    HRGN rgn = CreateRectRgn(n[0], n[1], n[2], n[3]);
    SelectClipRgn(c->hdc, rgn);
    DeleteObject(rgn);
}

int cv_clip_test(Canvas *c, float x,float y,float w,float h)
{
    int *cl = c->clip[c->nclip];
    if (x + w < cl[0] || x > cl[2] || y + h < cl[1] || y > cl[3]) return 0;
    return 1;
}

void cv_clear(Canvas *c, Color col)
{
    uint32_t v = 0xFF000000u | (col & 0x00FFFFFFu);
    int n = c->w * c->h;
    cv_sync(c);
    for (int i = 0; i < n; i++) c->px[i] = v;
}

/* ==========================================================================
 *  compositing
 * ========================================================================== */

static inline void blend_one(uint32_t *d, Color s, int a)
{
    if (a <= 0) return;
    if (a >= 255) { *d = 0xFF000000u | (s & 0x00FFFFFFu); return; }
    uint32_t dv = *d;
    int dr = (dv >> 16) & 0xFF, dg = (dv >> 8) & 0xFF, db = dv & 0xFF;
    int sr = (s  >> 16) & 0xFF, sg = (s  >> 8) & 0xFF, sb = s  & 0xFF;
    dr += div255((sr - dr) * a);
    dg += div255((sg - dg) * a);
    db += div255((sb - db) * a);
    *d = 0xFF000000u | ((uint32_t)dr << 16) | ((uint32_t)dg << 8) | (uint32_t)db;
}

void cv_blend_px(Canvas *c, int x,int y, Color col, float a)
{
    int *cl = c->clip[c->nclip];
    if (x < cl[0] || x >= cl[2] || y < cl[1] || y >= cl[3]) return;
    blend_one(c->px + (size_t)y * c->w + x, col, (int)(COL_A(col) * cv_clampf(a,0,1) + .5f));
}

/* ==========================================================================
 *  general scanline rasteriser (non-zero winding)
 * ========================================================================== */

typedef struct { float x0,y0,x1,y1; float dxdy; int dir; } REdge;

static REdge  g_edges[MAX_EDGES];
static int    g_order[MAX_EDGES];
static int    g_active[MAX_EDGES];

static int build_edges(const Path *p)
{
    int n = 0;
    for (int s = 0; s < p->nsub; s++) {
        int a = p->sub[s];
        int b = (s + 1 < p->nsub) ? p->sub[s+1] : p->npt;
        if (b - a < 2) continue;
        for (int i = a; i < b; i++) {
            int j = (i + 1 < b) ? i + 1 : a;      /* implicit close        */
            float x0 = p->pts[i*2], y0 = p->pts[i*2+1];
            float x1 = p->pts[j*2], y1 = p->pts[j*2+1];
            if (y0 == y1) continue;
            if (n >= MAX_EDGES) return n;
            REdge *e = &g_edges[n++];
            if (y0 < y1) { e->x0=x0; e->y0=y0; e->x1=x1; e->y1=y1; e->dir= 1; }
            else         { e->x0=x1; e->y0=y1; e->x1=x0; e->y1=y0; e->dir=-1; }
            e->dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
        }
    }
    return n;
}

static int cmp_edge_y(const void *a, const void *b)
{
    float ya = g_edges[*(const int*)a].y0;
    float yb = g_edges[*(const int*)b].y0;
    return (ya < yb) ? -1 : (ya > yb ? 1 : 0);
}

static void cov_span(float *cov, float xa, float xb, float wgt,
                     int lo, int hi, int *minx, int *maxx)
{
    if (xb <= xa) return;
    if (xa < (float)lo) xa = (float)lo;
    if (xb > (float)hi) xb = (float)hi;
    if (xb <= xa) return;

    int ia = (int)floorf(xa), ib = (int)floorf(xb - 1e-4f);
    if (ia < lo) ia = lo;
    if (ib >= hi) ib = hi - 1;
    if (ib < ia) return;
    if (ia < *minx) *minx = ia;
    if (ib > *maxx) *maxx = ib;

    if (ia == ib) { cov[ia] += (xb - xa) * wgt; return; }
    cov[ia] += ((float)(ia + 1) - xa) * wgt;
    for (int i = ia + 1; i < ib; i++) cov[i] += wgt;
    cov[ib] += (xb - (float)ib) * wgt;
}

void cv_fill(Canvas *c, const Path *p, const Paint *paint, float alpha)
{
    if (alpha <= 0.001f || p->npt < 3) return;
    cv_sync(c);

    int ne = build_edges(p);
    if (!ne) return;

    int *cl = c->clip[c->nclip];
    float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
    for (int i = 0; i < ne; i++) {
        REdge *e = &g_edges[i];
        if (e->y0 < by0) by0 = e->y0;
        if (e->y1 > by1) by1 = e->y1;
        float lx = e->x0 < e->x1 ? e->x0 : e->x1;
        float hx = e->x0 > e->x1 ? e->x0 : e->x1;
        if (lx < bx0) bx0 = lx;
        if (hx > bx1) bx1 = hx;
    }
    int y0 = imax((int)floorf(by0), cl[1]);
    int y1 = imin((int)ceilf (by1), cl[3]);
    int lo = imax((int)floorf(bx0), cl[0]);
    int hi = imin((int)ceilf (bx1) + 1, cl[2]);
    if (y0 >= y1 || lo >= hi) return;

    for (int i = 0; i < ne; i++) g_order[i] = i;
    qsort(g_order, (size_t)ne, sizeof(int), cmp_edge_y);

    int nact = 0, next = 0;
    float *cov = c->cov;
    const float wgt = 1.0f / SUBS;

    for (int y = y0; y < y1; y++) {
        float ytop = (float)y, ybot = (float)(y + 1);

        while (next < ne && g_edges[g_order[next]].y0 < ybot)
            g_active[nact++] = g_order[next++];
        for (int i = 0; i < nact; ) {
            if (g_edges[g_active[i]].y1 <= ytop) g_active[i] = g_active[--nact];
            else i++;
        }
        if (!nact) continue;

        memset(cov + lo, 0, sizeof(float) * (size_t)(hi - lo));
        int minx = hi, maxx = lo - 1;

        for (int s = 0; s < SUBS; s++) {
            float sy = ytop + ((float)s + 0.5f) * wgt;
            int nx = 0;
            for (int i = 0; i < nact; i++) {
                REdge *e = &g_edges[g_active[i]];
                if (sy < e->y0 || sy >= e->y1) continue;
                c->xs[nx]   = e->x0 + (sy - e->y0) * e->dxdy;
                c->dirs[nx] = e->dir;
                nx++;
            }
            if (nx < 2) continue;
            for (int i = 1; i < nx; i++) {          /* insertion sort       */
                float kx = c->xs[i]; int kd = c->dirs[i]; int j = i - 1;
                while (j >= 0 && c->xs[j] > kx) {
                    c->xs[j+1] = c->xs[j]; c->dirs[j+1] = c->dirs[j]; j--;
                }
                c->xs[j+1] = kx; c->dirs[j+1] = kd;
            }
            int wind = 0;
            for (int i = 0; i < nx - 1; i++) {
                wind += c->dirs[i];
                if (wind != 0)
                    cov_span(cov, c->xs[i], c->xs[i+1], wgt, lo, hi, &minx, &maxx);
            }
        }
        if (maxx < minx) continue;

        uint32_t *row = c->px + (size_t)y * c->w;
        if (paint->kind == PAINT_SOLID) {
            Color sc = paint->c0;
            int   ca = (int)(COL_A(sc) * alpha + 0.5f);
            for (int x = minx; x <= maxx; x++) {
                float a = cov[x];
                if (a <= 0.002f) continue;
                if (a > 1.f) a = 1.f;
                blend_one(row + x, sc, (int)(a * ca + 0.5f));
            }
        } else {
            float fy = (float)y + 0.5f;
            for (int x = minx; x <= maxx; x++) {
                float a = cov[x];
                if (a <= 0.002f) continue;
                if (a > 1.f) a = 1.f;
                Color sc = paint_at(paint, (float)x + 0.5f, fy);
                blend_one(row + x, sc, (int)(a * COL_A(sc) * alpha + 0.5f));
            }
        }
    }
}

void cv_fill_col(Canvas *c, const Path *p, Color col)
{
    Paint pt = paint_solid(col);
    cv_fill(c, p, &pt, 1.f);
}

/* ---------------------------------------------------------------- stroke -- */

static void stroke_seg(Path *out, float x0,float y0,float x1,float y1,float hw)
{
    float dx = x1-x0, dy = y1-y0;
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 1e-5f) return;
    float nx = -dy/len*hw, ny = dx/len*hw;
    path_move(out, x0+nx, y0+ny);
    path_line(out, x1+nx, y1+ny);
    path_line(out, x1-nx, y1-ny);
    path_line(out, x0-nx, y0-ny);
    path_close(out);
}

static Path g_strokeBuf;

/* Round join, wound the SAME way round as stroke_seg's quads.  With non-zero
 * winding, an opposite orientation would cancel the quad underneath it and
 * punch a hole at every joint -- which is exactly what a dashed-looking
 * polyline means when you see one. */
static void join_dot(Path *out, float cx, float cy, float r)
{
    const int n = 10;
    for (int i = 0; i < n; i++) {
        float a = (float)(-2.0 * M_PI * i / n);
        float px = cx + cosf(a)*r, py = cy + sinf(a)*r;
        if (i == 0) path_move(out, px, py); else path_line(out, px, py);
    }
    path_close(out);
}

static void stroke_emit(Canvas *c, const Path *p, const Paint *paint,
                        float hw, float alpha, int joins)
{
    Path *o = &g_strokeBuf;
    path_reset(o);

    for (int s = 0; s < p->nsub; s++) {
        int a = p->sub[s];
        int b = (s + 1 < p->nsub) ? p->sub[s+1] : p->npt;
        for (int i = a; i + 1 < b; i++) {
            /* flush before the buffer fills so long polylines stay unbroken */
            if (o->npt > PATH_MAX_PTS - 64 || o->nsub > PATH_MAX_SUBS - 6) {
                cv_fill(c, o, paint, alpha);
                path_reset(o);
            }
            stroke_seg(o, p->pts[i*2], p->pts[i*2+1],
                          p->pts[i*2+2], p->pts[i*2+3], hw);
            if (joins && i + 2 < b)
                join_dot(o, p->pts[i*2+2], p->pts[i*2+3], hw);
        }
    }
    if (o->npt) cv_fill(c, o, paint, alpha);
}

void cv_stroke(Canvas *c, const Path *p, const Paint *paint, float width, float alpha)
{
    if (width <= 0.f || alpha <= 0.001f) return;
    stroke_emit(c, p, paint, width * 0.5f, alpha, width > 2.2f);
}

void cv_stroke_col(Canvas *c, const Path *p, Color col, float width)
{
    Paint pt = paint_solid(col);
    cv_stroke(c, p, &pt, width, 1.f);
}

/* ==========================================================================
 *  SDF primitives  --  rounded rect / circle fills and outlines
 * ========================================================================== */

static void sdf_rrect(Canvas *c, float x,float y,float w,float h,float r,
                      const Paint *paint, float alpha, float strokeW)
{
    if (alpha <= 0.001f || w <= 0.f || h <= 0.f) return;
    cv_sync(c);

    float hw = w*0.5f, hh = h*0.5f;
    float cx = x + hw, cy = y + hh;
    float m = (hw < hh ? hw : hh);
    if (r > m) r = m;
    if (r < 0.f) r = 0.f;

    float pad = 1.5f + (strokeW > 0.f ? strokeW*0.5f : 0.f);
    int *cl = c->clip[c->nclip];
    int x0 = imax((int)floorf(x - pad), cl[0]);
    int x1 = imin((int)ceilf (x + w + pad), cl[2]);
    int y0 = imax((int)floorf(y - pad), cl[1]);
    int y1 = imin((int)ceilf (y + h + pad), cl[3]);
    if (x0 >= x1 || y0 >= y1) return;

    float ex = hw - r, ey = hh - r;
    int solid = (paint->kind == PAINT_SOLID);
    Color sc  = paint->c0;
    int   ca  = (int)(COL_A(sc) * alpha + 0.5f);
    float shw = strokeW * 0.5f;

    for (int py = y0; py < y1; py++) {
        float fy = (float)py + 0.5f;
        float qy = fabsf(fy - cy) - ey; if (qy < 0.f) qy = 0.f;
        uint32_t *row = c->px + (size_t)py * c->w;
        for (int px = x0; px < x1; px++) {
            float fx = (float)px + 0.5f;
            float qx = fabsf(fx - cx) - ex; if (qx < 0.f) qx = 0.f;
            float d;
            if (qx == 0.f && qy == 0.f) {
                float ddx = ex - fabsf(fx-cx), ddy = ey - fabsf(fy-cy);
                d = -(ddx < ddy ? ddx : ddy) - r;
            } else {
                d = sqrtf(qx*qx + qy*qy) - r;
            }
            if (strokeW > 0.f) d = fabsf(d) - shw;
            float cov = 0.5f - d;
            if (cov <= 0.004f) continue;
            if (cov > 1.f) cov = 1.f;
            if (solid) blend_one(row + px, sc, (int)(cov * ca + 0.5f));
            else {
                Color s2 = paint_at(paint, fx, fy);
                blend_one(row + px, s2, (int)(cov * COL_A(s2) * alpha + 0.5f));
            }
        }
    }
}

void cv_rrect_p(Canvas *c,float x,float y,float w,float h,float r,const Paint *p)
{ sdf_rrect(c, x, y, w, h, r, p, 1.f, 0.f); }

void cv_rrect(Canvas *c,float x,float y,float w,float h,float r,Color col)
{ Paint p = paint_solid(col); sdf_rrect(c, x, y, w, h, r, &p, 1.f, 0.f); }

void cv_rrect_line(Canvas *c,float x,float y,float w,float h,float r,
                   Color col,float width)
{ Paint p = paint_solid(col); sdf_rrect(c, x, y, w, h, r, &p, 1.f, width); }

void cv_rect(Canvas *c,float x,float y,float w,float h,Color col)
{ Paint p = paint_solid(col); sdf_rrect(c, x, y, w, h, 0.f, &p, 1.f, 0.f); }

void cv_rect_p(Canvas *c,float x,float y,float w,float h,const Paint *p)
{ sdf_rrect(c, x, y, w, h, 0.f, p, 1.f, 0.f); }

void cv_circle_p(Canvas *c, float cx,float cy,float r, const Paint *p)
{ sdf_rrect(c, cx-r, cy-r, r*2.f, r*2.f, r, p, 1.f, 0.f); }

void cv_circle(Canvas *c, float cx,float cy,float r, Color col)
{ Paint p = paint_solid(col); sdf_rrect(c, cx-r, cy-r, r*2, r*2, r, &p, 1.f, 0.f); }

void cv_circle_line(Canvas *c,float cx,float cy,float r,Color col,float width)
{ Paint p = paint_solid(col); sdf_rrect(c, cx-r, cy-r, r*2, r*2, r, &p, 1.f, width); }

/* ------------------------------------------------------------------ line -- */

void cv_line(Canvas *c, float x0,float y0,float x1,float y1, Color col,float width)
{
    if (width <= 0.f) return;
    /* axis-aligned fast path keeps thin rules crisp */
    if (fabsf(y0-y1) < 0.01f) {
        float xa = x0<x1?x0:x1, xb = x0>x1?x0:x1;
        cv_rect(c, xa, y0 - width*0.5f, xb-xa, width, col);
        return;
    }
    if (fabsf(x0-x1) < 0.01f) {
        float ya = y0<y1?y0:y1, yb = y0>y1?y0:y1;
        cv_rect(c, x0 - width*0.5f, ya, width, yb-ya, col);
        return;
    }
    Path p; path_reset(&p);
    stroke_seg(&p, x0, y0, x1, y1, width*0.5f);
    cv_fill_col(c, &p, col);
}

void cv_line_round(Canvas *c,float x0,float y0,float x1,float y1,Color col,float w)
{
    cv_line(c, x0, y0, x1, y1, col, w);
    if (w > 1.6f) { cv_circle(c, x0, y0, w*0.5f, col); cv_circle(c, x1, y1, w*0.5f, col); }
}

void cv_tri(Canvas *c,float x0,float y0,float x1,float y1,float x2,float y2,Color col)
{
    Path p; path_reset(&p);
    path_move(&p, x0, y0); path_line(&p, x1, y1); path_line(&p, x2, y2);
    path_close(&p);
    cv_fill_col(c, &p, col);
}

/* ==========================================================================
 *  effects
 * ========================================================================== */

#define SHADOW_CACHE 20

typedef struct {
    int      w, h, r, blur, valid;
    int      aw, ah;
    uint8_t *a;
    unsigned stamp;
} ShadowEntry;

static ShadowEntry g_shadow[SHADOW_CACHE];
static unsigned    g_shadowClock = 0;

static void box_blur_u8(uint8_t *src, int w, int h, int rad, uint8_t *tmp)
{
    if (rad < 1) return;
    int span = rad*2 + 1;
    for (int y = 0; y < h; y++) {                    /* horizontal          */
        uint8_t *s = src + (size_t)y*w, *d = tmp + (size_t)y*w;
        int acc = 0;
        for (int i = -rad; i <= rad; i++) acc += s[i < 0 ? 0 : (i >= w ? w-1 : i)];
        for (int x = 0; x < w; x++) {
            d[x] = (uint8_t)(acc / span);
            int add = x + rad + 1, sub = x - rad;
            acc += s[add >= w ? w-1 : add] - s[sub < 0 ? 0 : sub];
        }
    }
    for (int x = 0; x < w; x++) {                    /* vertical            */
        int acc = 0;
        for (int i = -rad; i <= rad; i++) acc += tmp[(size_t)(i<0?0:(i>=h?h-1:i))*w + x];
        for (int y = 0; y < h; y++) {
            src[(size_t)y*w + x] = (uint8_t)(acc / span);
            int add = y + rad + 1, sub = y - rad;
            acc += tmp[(size_t)(add>=h?h-1:add)*w + x] - tmp[(size_t)(sub<0?0:sub)*w + x];
        }
    }
}

static ShadowEntry *shadow_get(int w, int h, int r, int blur)
{
    int lru = 0;
    for (int i = 0; i < SHADOW_CACHE; i++) {
        ShadowEntry *e = &g_shadow[i];
        if (e->valid && e->w==w && e->h==h && e->r==r && e->blur==blur) {
            e->stamp = ++g_shadowClock;
            return e;
        }
        if (!e->valid) { lru = i; break; }
        if (e->stamp < g_shadow[lru].stamp) lru = i;
    }
    ShadowEntry *e = &g_shadow[lru];
    free(e->a);

    int pad = blur * 2 + 3;
    int aw = w + pad*2, ah = h + pad*2;
    e->a = (uint8_t *)calloc((size_t)aw*ah, 1);
    if (!e->a) { e->valid = 0; return NULL; }

    /* rasterise the rounded rect into the mask analytically */
    float cx = aw*0.5f, cy = ah*0.5f;
    float hw = w*0.5f,  hh = h*0.5f;
    float rr = (float)r;
    float ex = hw - rr, ey = hh - rr;
    if (ex < 0) ex = 0;
    if (ey < 0) ey = 0;
    for (int y = 0; y < ah; y++) {
        float fy = (float)y + 0.5f;
        float qy = fabsf(fy - cy) - ey; if (qy < 0) qy = 0;
        for (int x = 0; x < aw; x++) {
            float fx = (float)x + 0.5f;
            float qx = fabsf(fx - cx) - ex; if (qx < 0) qx = 0;
            float d  = sqrtf(qx*qx + qy*qy) - rr;
            float cov = cv_clampf(0.5f - d, 0.f, 1.f);
            e->a[(size_t)y*aw + x] = (uint8_t)(cov * 255.f);
        }
    }
    uint8_t *tmp = (uint8_t *)malloc((size_t)aw*ah);
    if (tmp) {
        int pass = blur / 3; if (pass < 1) pass = 1;
        box_blur_u8(e->a, aw, ah, pass, tmp);
        box_blur_u8(e->a, aw, ah, pass, tmp);
        box_blur_u8(e->a, aw, ah, pass, tmp);
        free(tmp);
    }
    e->w=w; e->h=h; e->r=r; e->blur=blur; e->aw=aw; e->ah=ah;
    e->valid = 1; e->stamp = ++g_shadowClock;
    return e;
}

void cv_shadow(Canvas *c, float x,float y,float w,float h,float r,
               float blur, Color col)
{
    int iw = (int)(w + 0.5f), ih = (int)(h + 0.5f);
    int ir = (int)(r + 0.5f), ib = (int)(blur + 0.5f);
    if (iw < 1 || ih < 1 || ib < 1) return;
    if (iw > 1600) iw = 1600;
    if (ih > 1200) ih = 1200;
    ShadowEntry *e = shadow_get(iw, ih, ir, ib);
    if (!e || !e->a) return;
    cv_sync(c);

    int pad = (e->aw - iw) / 2;
    int ox = (int)(x + 0.5f) - pad, oy = (int)(y + 0.5f) - pad;
    int *cl = c->clip[c->nclip];
    int x0 = imax(ox, cl[0]), x1 = imin(ox + e->aw, cl[2]);
    int y0 = imax(oy, cl[1]), y1 = imin(oy + e->ah, cl[3]);
    int ca = (int)COL_A(col);

    for (int py = y0; py < y1; py++) {
        const uint8_t *src = e->a + (size_t)(py - oy)*e->aw + (x0 - ox);
        uint32_t *row = c->px + (size_t)py*c->w + x0;
        for (int px = x0; px < x1; px++, src++, row++) {
            int a = *src;
            if (!a) continue;
            blend_one(row, col, div255(a * ca));
        }
    }
}

void cv_glow(Canvas *c, float cx,float cy,float r, Color col, float alpha)
{
    if (r <= 0.f || alpha <= 0.002f) return;
    cv_sync(c);
    int *cl = c->clip[c->nclip];
    int x0 = imax((int)(cx-r), cl[0]), x1 = imin((int)(cx+r)+1, cl[2]);
    int y0 = imax((int)(cy-r), cl[1]), y1 = imin((int)(cy+r)+1, cl[3]);
    float inv = 1.f/r;
    int ca = (int)(COL_A(col) * cv_clampf(alpha,0,1));
    for (int py = y0; py < y1; py++) {
        float dy = ((float)py + .5f) - cy;
        uint32_t *row = c->px + (size_t)py*c->w;
        for (int px = x0; px < x1; px++) {
            float dx = ((float)px + .5f) - cx;
            float d  = sqrtf(dx*dx + dy*dy) * inv;
            if (d >= 1.f) continue;
            float f = 1.f - d;
            f = f*f*f;                                   /* soft falloff    */
            blend_one(row + px, col, (int)(f * ca));
        }
    }
}

void cv_blur_rect(Canvas *c, int x,int y,int w,int h, int radius)
{
    if (radius < 1 || w < 2 || h < 2) return;
    cv_sync(c);
    int *cl = c->clip[c->nclip];
    if (x < cl[0]) { w -= cl[0]-x; x = cl[0]; }
    if (y < cl[1]) { h -= cl[1]-y; y = cl[1]; }
    if (x + w > cl[2]) w = cl[2] - x;
    if (y + h > cl[3]) h = cl[3] - y;
    if (w < 2 || h < 2) return;

    int n = w*h;
    uint8_t *ch = (uint8_t *)malloc((size_t)n*2);
    if (!ch) return;
    uint8_t *tmp = ch + n;

    for (int comp = 0; comp < 3; comp++) {
        int shift = comp * 8;
        for (int j = 0; j < h; j++) {
            uint32_t *s = c->px + (size_t)(y+j)*c->w + x;
            for (int i = 0; i < w; i++) ch[(size_t)j*w+i] = (uint8_t)((s[i] >> shift) & 0xFF);
        }
        box_blur_u8(ch, w, h, radius, tmp);
        box_blur_u8(ch, w, h, radius, tmp);
        for (int j = 0; j < h; j++) {
            uint32_t *d = c->px + (size_t)(y+j)*c->w + x;
            for (int i = 0; i < w; i++) {
                uint32_t v = d[i] & ~((uint32_t)0xFF << shift);
                d[i] = v | ((uint32_t)ch[(size_t)j*w+i] << shift);
            }
        }
    }
    free(ch);
}

void cv_vignette(Canvas *c, float strength)
{
    cv_sync(c);
    float cx = c->w*0.5f, cy = c->h*0.5f;
    float maxd = sqrtf(cx*cx + cy*cy);
    for (int y = 0; y < c->h; y++) {
        float dy = (float)y - cy;
        uint32_t *row = c->px + (size_t)y*c->w;
        for (int x = 0; x < c->w; x++) {
            float dx = (float)x - cx;
            float d = sqrtf(dx*dx + dy*dy)/maxd;
            float f = d*d*strength;
            if (f <= 0.003f) continue;
            if (f > 1.f) f = 1.f;
            blend_one(row + x, RGBA(0,0,0,255), (int)(f*255));
        }
    }
}

void cv_noise(Canvas *c, int x,int y,int w,int h, int amount, uint32_t seed)
{
    cv_sync(c);
    int *cl = c->clip[c->nclip];
    int x0 = imax(x, cl[0]), x1 = imin(x+w, cl[2]);
    int y0 = imax(y, cl[1]), y1 = imin(y+h, cl[3]);
    uint32_t s = seed | 1u;
    for (int py = y0; py < y1; py++) {
        uint32_t *row = c->px + (size_t)py*c->w;
        for (int px = x0; px < x1; px++) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            int n = (int)(s & 0xFF) - 128;
            n = n * amount / 128;
            uint32_t v = row[px];
            int r = (int)((v>>16)&0xFF) + n;
            int g = (int)((v>>8)&0xFF)  + n;
            int b = (int)(v&0xFF)       + n;
            r = r<0?0:(r>255?255:r); g = g<0?0:(g>255?255:g); b = b<0?0:(b>255?255:b);
            row[px] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | (uint32_t)b;
        }
    }
}

void cv_present(Canvas *c, HDC dst, int x, int y)
{
    cv_sync(c);
    BitBlt(dst, x, y, c->w, c->h, c->hdc, 0, 0, SRCCOPY);
}
