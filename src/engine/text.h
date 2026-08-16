/* ==========================================================================
 *  AURA :: text.h  --  font cache and text drawing over the software canvas
 *
 *  Glyph rasterisation is delegated to GDI (it owns the hinted, ClearType
 *  outlines), but everything is written straight into the same DIB section
 *  the rasteriser composites into, so text and vector art interleave freely.
 * ========================================================================== */
#ifndef AURA_TEXT_H
#define AURA_TEXT_H

#include "canvas.h"

/* font families  (TF_ prefix: FF_* is taken by wingdi.h) */
enum { TF_UI = 0, TF_DISPLAY, TF_MONO, TF_COUNT };

/* weights  (TW_ prefix: FW_* is taken by wingdi.h) */
enum { TW_LIGHT = 300, TW_REG = 400, TW_MED = 500, TW_SEMI = 600, TW_BOLD = 700,
       TW_BLACK = 900 };

/* alignment  (AL_/AV_ prefix: TA_* is taken by wingdi.h) */
enum { AL_L = 0, AL_C = 1, AL_R = 2 };
enum { AV_T = 0, AV_M = 1, AV_B = 2 };

typedef struct {
    int   family;
    int   size;        /* pixel height                                      */
    int   weight;
    int   italic;
    int   tracking;    /* extra pixels between characters                   */
} Font;

void  tx_init(void);
void  tx_shutdown(void);

Font  font_make(int family, int size, int weight);
Font  font_track(int family, int size, int weight, int tracking);

/* backdrop used to fake alpha for faded text (GDI cannot blend glyphs) */
void  tx_backdrop(Color c);

int   tx_width  (Canvas *c, const char *utf8, Font f);
int   tx_height (Canvas *c, Font f);

void  tx_draw   (Canvas *c, const char *utf8, float x, float y,
                 Font f, Color col, int halign, int valign);
void  tx_draw_a (Canvas *c, const char *utf8, float x, float y,
                 Font f, Color col, int halign, int valign, float alpha);
/* wrapped paragraph; returns the height consumed */
int   tx_para   (Canvas *c, const char *utf8, float x, float y, float w,
                 Font f, Color col, int lineGap);
int   tx_para_h (Canvas *c, const char *utf8, float w, Font f, int lineGap);

/* single line clipped with an ellipsis if it will not fit */
void  tx_clipped(Canvas *c, const char *utf8, float x, float y, float maxw,
                 Font f, Color col, int halign, int valign);

#endif /* AURA_TEXT_H */
