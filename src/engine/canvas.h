/* ==========================================================================
 *  AURA  ::  canvas.h
 *  Anti-aliased software rasteriser  --  written from scratch in C.
 *
 *  There is no GDI+, no Direct2D, no Skia and no third-party code in here.
 *  Every pixel this application puts on screen is produced by the scanline
 *  coverage rasteriser implemented in canvas.c, compositing into a 32-bit
 *  BGRA DIB section which is blitted to the window once per frame.
 *
 *  Trois Freres Systems Ltd.  --  SIS 2075 Software Engineering 1
 * ========================================================================== */
#ifndef AURA_CANVAS_H
#define AURA_CANVAS_H

#include <windows.h>
#include <stdint.h>

/* ---------------------------------------------------------------- colour -- */

typedef uint32_t Color;                     /* 0xAARRGGBB (native uint32)   */

/* Components are extracted as signed int on purpose: differences between two
 * channels are taken all over the renderer, and unsigned arithmetic there
 * wraps to ~4 billion instead of going negative. */
#define RGBA(r,g,b,a)  ((Color)((((uint32_t)(a)&0xFFu)<<24)| \
                                (((uint32_t)(r)&0xFFu)<<16)| \
                                (((uint32_t)(g)&0xFFu)<<8) | \
                                 ((uint32_t)(b)&0xFFu)))
#define RGB24(r,g,b)   RGBA(r,g,b,255)
#define HEX(h)         ((Color)(0xFF000000u|(uint32_t)(h)))
#define COL_A(c)       ((int)(((c)>>24)&0xFFu))
#define COL_R(c)       ((int)(((c)>>16)&0xFFu))
#define COL_G(c)       ((int)(((c)>>8)&0xFFu))
#define COL_B(c)       ((int)((c)&0xFFu))

Color col_alpha (Color c, float a);         /* scale existing alpha by a    */
Color col_mix   (Color a, Color b, float t);
Color col_lighten(Color c, float amt);
Color col_darken (Color c, float amt);

/* ----------------------------------------------------------------- paint -- */

typedef enum {
    PAINT_SOLID = 0,
    PAINT_LINEAR,                            /* two-stop linear gradient    */
    PAINT_RADIAL                             /* two-stop radial gradient    */
} PaintKind;

typedef struct {
    PaintKind kind;
    Color     c0, c1;
    float     x0, y0, x1, y1;                /* linear: axis endpoints      */
    float     cxp, cyp, r;                   /* radial: centre + radius     */
} Paint;

Paint paint_solid  (Color c);
Paint paint_linear (float x0,float y0,float x1,float y1, Color a, Color b);
Paint paint_radial (float cx,float cy,float r, Color inner, Color outer);

/* ------------------------------------------------------------------ path -- */

#define PATH_MAX_PTS   4096
#define PATH_MAX_SUBS   512

typedef struct {
    float pts[PATH_MAX_PTS * 2];
    int   sub[PATH_MAX_SUBS];                /* start index of each contour */
    int   nsub;
    int   npt;
    float sx, sy;                            /* current contour start       */
    float cx, cy;                            /* current point               */
} Path;

void path_reset  (Path *p);
void path_move   (Path *p, float x, float y);
void path_line   (Path *p, float x, float y);
void path_cubic  (Path *p, float c1x,float c1y,float c2x,float c2y,float x,float y);
void path_quad   (Path *p, float cx,float cy,float x,float y);
void path_close  (Path *p);

/* convenience shapes (all append a fresh closed contour) */
void path_rect   (Path *p, float x,float y,float w,float h);
void path_rrect  (Path *p, float x,float y,float w,float h,float r);
void path_rrect4 (Path *p, float x,float y,float w,float h,
                  float tl,float tr,float br,float bl);
void path_circle (Path *p, float cx,float cy,float r);
void path_ellipse(Path *p, float cx,float cy,float rx,float ry);
void path_arc    (Path *p, float cx,float cy,float r,float a0,float a1);
void path_ring   (Path *p, float cx,float cy,float rOuter,float rInner);
void path_ring_arc(Path *p,float cx,float cy,float rOuter,float rInner,
                   float a0,float a1);
void path_poly   (Path *p, const float *xy, int n);
void path_ngon   (Path *p, float cx,float cy,float r,int sides,float rot);
void path_star   (Path *p, float cx,float cy,float rOut,float rIn,int pts,float rot);

/* ---------------------------------------------------------------- canvas -- */

#define CLIP_STACK_MAX  32

typedef struct {
    int      w, h;
    uint32_t *px;                            /* w*h BGRA, top-down          */
    HDC      hdc;                            /* DC of the DIB section       */
    HBITMAP  hbm;
    HGDIOBJ  hbmOld;
    int      gdiDirty;                       /* GDI wrote since last flush  */

    /* clip rectangle stack */
    int      clip[CLIP_STACK_MAX][4];
    int      nclip;

    /* rasteriser scratch */
    float   *cov;                            /* w floats of coverage        */
    uint32_t *grad;                          /* w colours, gradient scratch */
    float   *xs;                             /* crossings                   */
    int     *dirs;
} Canvas;

int   cv_create  (Canvas *c, int w, int h);
void  cv_destroy (Canvas *c);
int   cv_resize  (Canvas *c, int w, int h);
void  cv_sync    (Canvas *c);                /* GdiFlush if needed          */
void  cv_gdi_used(Canvas *c);

void  cv_clip_push (Canvas *c, float x,float y,float w,float h);
void  cv_clip_pop  (Canvas *c);
int   cv_clip_test (Canvas *c, float x,float y,float w,float h);

void  cv_clear     (Canvas *c, Color col);

/* filling ---------------------------------------------------------------- */
void  cv_fill      (Canvas *c, const Path *p, const Paint *paint, float alpha);
void  cv_fill_col  (Canvas *c, const Path *p, Color col);
void  cv_stroke    (Canvas *c, const Path *p, const Paint *paint,
                    float width, float alpha);
void  cv_stroke_col(Canvas *c, const Path *p, Color col, float width);

/* fast primitives (bypass the path builder) ------------------------------- */
void  cv_rect      (Canvas *c, float x,float y,float w,float h, Color col);
void  cv_rect_p    (Canvas *c, float x,float y,float w,float h, const Paint *p);
void  cv_rrect     (Canvas *c, float x,float y,float w,float h,float r,Color col);
void  cv_rrect_p   (Canvas *c, float x,float y,float w,float h,float r,
                    const Paint *paint);
void  cv_rrect_line(Canvas *c, float x,float y,float w,float h,float r,
                    Color col, float width);
void  cv_circle    (Canvas *c, float cx,float cy,float r, Color col);
void  cv_circle_p  (Canvas *c, float cx,float cy,float r, const Paint *p);
void  cv_circle_line(Canvas *c,float cx,float cy,float r,Color col,float width);
void  cv_line      (Canvas *c, float x0,float y0,float x1,float y1,
                    Color col, float width);
void  cv_line_round(Canvas *c, float x0,float y0,float x1,float y1,
                    Color col, float width);
void  cv_tri       (Canvas *c, float x0,float y0,float x1,float y1,
                    float x2,float y2, Color col);

/* effects ---------------------------------------------------------------- */
void  cv_shadow    (Canvas *c, float x,float y,float w,float h,float r,
                    float blur, Color col);
void  cv_glow      (Canvas *c, float cx,float cy,float r, Color col,float alpha);
void  cv_blur_rect (Canvas *c, int x,int y,int w,int h, int radius);
void  cv_vignette  (Canvas *c, float strength);
void  cv_noise     (Canvas *c, int x,int y,int w,int h, int amount, uint32_t seed);

/* pixel access ----------------------------------------------------------- */
void  cv_blend_px  (Canvas *c, int x,int y, Color col, float a);
void  cv_present   (Canvas *c, HDC dst, int x, int y);

/* small helpers ---------------------------------------------------------- */
float cv_clampf (float v, float lo, float hi);
float cv_lerp   (float a, float b, float t);

#endif /* AURA_CANVAS_H */
