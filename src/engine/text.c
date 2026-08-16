/* ==========================================================================
 *  AURA :: text.c
 * ========================================================================== */

#include "text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define FONT_CACHE 96
#define WBUF       2048

typedef struct {
    int   used;
    Font  key;
    HFONT h;
    int   ascent, descent, height;
} FontSlot;

static FontSlot g_fonts[FONT_CACHE];
static int      g_nfonts = 0;
static Color    g_backdrop = 0xFFFFFFFF;

static const wchar_t *g_family[TF_COUNT] = {
    L"Segoe UI",
    L"Bahnschrift",          /* condensed technical face; falls back nicely */
    L"Consolas"
};

void tx_init(void)
{
    memset(g_fonts, 0, sizeof g_fonts);
    g_nfonts = 0;
}

void tx_shutdown(void)
{
    for (int i = 0; i < g_nfonts; i++)
        if (g_fonts[i].used && g_fonts[i].h) DeleteObject(g_fonts[i].h);
    g_nfonts = 0;
}

void tx_backdrop(Color c) { g_backdrop = c; }

Font font_make(int family, int size, int weight)
{
    Font f; f.family = family; f.size = size; f.weight = weight;
    f.italic = 0; f.tracking = 0;
    return f;
}

Font font_track(int family, int size, int weight, int tracking)
{
    Font f = font_make(family, size, weight);
    f.tracking = tracking;
    return f;
}

static FontSlot *font_slot(Canvas *c, Font f)
{
    for (int i = 0; i < g_nfonts; i++) {
        FontSlot *s = &g_fonts[i];
        if (s->used && s->key.family == f.family && s->key.size == f.size &&
            s->key.weight == f.weight && s->key.italic == f.italic)
            return s;
    }
    if (g_nfonts >= FONT_CACHE) g_nfonts = 0;      /* crude recycle          */
    FontSlot *s = &g_fonts[g_nfonts++];
    if (s->used && s->h) DeleteObject(s->h);

    LOGFONTW lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight         = -f.size;
    lf.lfWeight         = f.weight;
    lf.lfItalic         = (BYTE)(f.italic ? 1 : 0);
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfOutPrecision   = OUT_TT_PRECIS;
    lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    int fam = (f.family >= 0 && f.family < TF_COUNT) ? f.family : TF_UI;
    wcscpy(lf.lfFaceName, g_family[fam]);

    s->h    = CreateFontIndirectW(&lf);
    s->key  = f;
    s->used = 1;

    HGDIOBJ old = SelectObject(c->hdc, s->h);
    TEXTMETRICW tm;
    GetTextMetricsW(c->hdc, &tm);
    s->ascent  = tm.tmAscent;
    s->descent = tm.tmDescent;
    s->height  = tm.tmHeight;
    SelectObject(c->hdc, old);
    return s;
}

static int to_wide(const char *utf8, wchar_t *out, int cap)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
    return n > 0 ? n - 1 : 0;
}

static COLORREF to_cref(Color c)
{
    return RGB(COL_R(c), COL_G(c), COL_B(c));
}

int tx_height(Canvas *c, Font f)
{
    FontSlot *s = font_slot(c, f);
    return s->height;
}

int tx_width(Canvas *c, const char *utf8, Font f)
{
    wchar_t wb[WBUF];
    int n = to_wide(utf8, wb, WBUF);
    if (!n) return 0;
    FontSlot *s = font_slot(c, f);
    HGDIOBJ old = SelectObject(c->hdc, s->h);
    SetTextCharacterExtra(c->hdc, f.tracking);
    SIZE sz;
    GetTextExtentPoint32W(c->hdc, wb, n, &sz);
    SetTextCharacterExtra(c->hdc, 0);
    SelectObject(c->hdc, old);
    return sz.cx;
}

void tx_draw(Canvas *c, const char *utf8, float x, float y,
             Font f, Color col, int halign, int valign)
{
    tx_draw_a(c, utf8, x, y, f, col, halign, valign, 1.f);
}

void tx_draw_a(Canvas *c, const char *utf8, float x, float y,
               Font f, Color col, int halign, int valign, float alpha)
{
    if (!utf8 || !*utf8) return;
    float a = cv_clampf(alpha, 0.f, 1.f) * (COL_A(col) / 255.f);
    if (a <= 0.01f) return;
    if (a < 0.995f) col = col_mix(g_backdrop, col, a);   /* fake blending    */

    wchar_t wb[WBUF];
    int n = to_wide(utf8, wb, WBUF);
    if (!n) return;

    FontSlot *s = font_slot(c, f);
    HGDIOBJ old = SelectObject(c->hdc, s->h);
    SetTextCharacterExtra(c->hdc, f.tracking);
    SetTextColor(c->hdc, to_cref(col));
    SetBkMode(c->hdc, TRANSPARENT);

    SIZE sz;
    GetTextExtentPoint32W(c->hdc, wb, n, &sz);

    float px = x, py = y;
    if (halign == AL_C) px = x - sz.cx * 0.5f;
    else if (halign == AL_R) px = x - sz.cx;
    if (valign == AV_M) py = y - s->height * 0.5f;
    else if (valign == AV_B) py = y - s->height;

    ExtTextOutW(c->hdc, (int)(px + 0.5f), (int)(py + 0.5f), 0, NULL, wb, n, NULL);

    SetTextCharacterExtra(c->hdc, 0);
    SelectObject(c->hdc, old);
    cv_gdi_used(c);
}

void tx_clipped(Canvas *c, const char *utf8, float x, float y, float maxw,
                Font f, Color col, int halign, int valign)
{
    int w = tx_width(c, utf8, f);
    if (w <= (int)maxw) { tx_draw(c, utf8, x, y, f, col, halign, valign); return; }

    char buf[512];
    size_t len = strlen(utf8);
    if (len > sizeof buf - 4) len = sizeof buf - 4;
    memcpy(buf, utf8, len);
    buf[len] = 0;
    while (len > 1) {
        len--;
        /* do not cut a UTF-8 continuation byte in half */
        while (len > 1 && ((unsigned char)buf[len] & 0xC0) == 0x80) len--;
        buf[len]   = '.';
        buf[len+1] = '.';
        buf[len+2] = '.';
        buf[len+3] = 0;
        if (tx_width(c, buf, f) <= (int)maxw) break;
        buf[len] = 0;
    }
    tx_draw(c, buf, x, y, f, col, halign, valign);
}

/* ------------------------------------------------------------- paragraph -- */

static int para_run(Canvas *c, const char *utf8, float x, float y, float w,
                    Font f, Color col, int lineGap, int draw)
{
    if (!utf8 || !*utf8) return 0;
    FontSlot *s = font_slot(c, f);
    int lineH = s->height + lineGap;

    char line[1024]; int ll = 0;
    const char *p = utf8;
    float cy = y;
    int total = 0;

    while (*p) {
        /* take the next word (including any leading spaces) */
        const char *ws = p;
        while (*p == ' ') p++;
        const char *w0 = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        int wlen = (int)(p - ws);

        char cand[1024];
        int  cl = ll;
        if (cl + wlen < (int)sizeof cand - 1) {
            memcpy(cand, line, (size_t)ll);
            memcpy(cand + ll, ws, (size_t)wlen);
            cl = ll + wlen;
            cand[cl] = 0;
        } else { cand[0] = 0; cl = 0; }

        int fits = (tx_width(c, cand[0] ? cand : "", f) <= (int)w) || ll == 0;
        if (!fits) {
            line[ll] = 0;
            if (draw) tx_draw(c, line, x, cy, f, col, AL_L, AV_T);
            cy += lineH; total += lineH;
            ll = 0;
            wlen = (int)(p - w0);
            if (wlen < (int)sizeof line - 1) { memcpy(line, w0, (size_t)wlen); ll = wlen; }
        } else {
            memcpy(line, cand, (size_t)cl); ll = cl;
        }

        if (*p == '\n') {
            line[ll] = 0;
            if (draw) tx_draw(c, line, x, cy, f, col, AL_L, AV_T);
            cy += lineH; total += lineH; ll = 0; p++;
        }
    }
    if (ll > 0) {
        line[ll] = 0;
        if (draw) tx_draw(c, line, x, cy, f, col, AL_L, AV_T);
        total += lineH;
    }
    return total;
}

int tx_para(Canvas *c, const char *utf8, float x, float y, float w,
            Font f, Color col, int lineGap)
{
    return para_run(c, utf8, x, y, w, f, col, lineGap, 1);
}

int tx_para_h(Canvas *c, const char *utf8, float w, Font f, int lineGap)
{
    return para_run(c, utf8, 0, 0, w, f, 0, lineGap, 0);
}
