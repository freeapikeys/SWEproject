/* ==========================================================================
 *  AURA :: screen_board.c   --   the movement board
 *
 *  The centrepiece is a working split-flap display.  Each character sits in
 *  its own cell holding the glyph currently shown and the glyph it is trying
 *  to reach; when they differ the cell rolls forward through the alphabet a
 *  flap at a time, exactly like a Solari board, and the two halves of the
 *  card are shaded separately so the split reads.
 *
 *  Because the board is driven from live flight state, a status changing in
 *  the simulation makes the relevant cells physically flip.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* the flap alphabet, in the order the drum is wound */
static const char *FLAP = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./";
#define FLAPN 41

static int flap_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    for (int i = 0; i < FLAPN; i++) if (FLAP[i] == ch) return i;
    return 0;
}

/* --------------------------------------------------------------------------
 *  one character cell
 *
 *  The drum state lives here rather than in the generic ui_state table.  A
 *  board of 72 movements is 41 cells a row and three values a cell -- nearly
 *  nine thousand entries -- which swamped that table, evicted unrelated
 *  widgets' state (the scroll offset among them, so the board would not
 *  scroll at all) and turned every lookup into a long failed probe.
 *
 *  Keying on screen position rather than flight identity is also the correct
 *  behaviour for a real board: when the list moves, the character in a given
 *  cell changes, and the flap turns.  Which is exactly what one does.
 * ------------------------------------------------------------------------- */

#define FLAP_ROWS  64
#define FLAP_CELLS 48

typedef struct { float shown, want, roll; int primed; } FlapCell;
static FlapCell g_flap[FLAP_ROWS][FLAP_CELLS];

static void flap_cell(App *a, float x, float y, float cw, float ch,
                      char target, int row, int cell, Color ink, int bright)
{
    Canvas *c = &a->cv;
    if (row < 0 || row >= FLAP_ROWS || cell < 0 || cell >= FLAP_CELLS) return;
    FlapCell *fc = &g_flap[row][cell];
    if (!fc->primed) {
        fc->primed = 1;
        fc->shown = fc->want = (float)flap_index(target);
        fc->roll  = 0.f;
    }
    float *shown = &fc->shown;
    float *want  = &fc->want;
    float *roll  = &fc->roll;

    int ti = flap_index(target);
    if ((int)*want != ti) { *want = (float)ti; }

    /* roll forward one flap at a time until the drum reaches the target */
    if ((int)*shown != (int)*want) {
        *roll += anim_dt() * 26.f;
        while (*roll >= 1.f && (int)*shown != (int)*want) {
            *roll -= 1.f;
            *shown = (float)(((int)*shown + 1) % FLAPN);
        }
    } else {
        *roll = 0.f;
    }
    int si = (int)*shown;
    char glyph[2] = { FLAP[si % FLAPN], 0 };

    /* the card */
    Color top = bright ? HEX(0x241452) : HEX(0x1B0F3E);
    Color bot = bright ? HEX(0x1C0F42) : HEX(0x150B32);
    cv_rrect(c, x, y, cw - 1.6f, ch*0.5f, 2.f, top);
    cv_rrect(c, x, y + ch*0.5f, cw - 1.6f, ch*0.5f - 0.6f, 2.f, bot);
    cv_line(c, x, y + ch*0.5f, x + cw - 1.6f, y + ch*0.5f,
            col_alpha(HEX(0x000000), .55f), 1.f);

    /* the flap in motion catches a little light */
    float motion = (int)*shown != (int)*want ? 1.f : 0.f;
    if (motion > 0.f) {
        float p = *roll;
        cv_rrect(c, x, y + ch*0.5f - ch*0.5f*(1.f - p), cw - 1.6f,
                 ch*0.5f*(1.f - p), 2.f, col_alpha(HEX(0xFFFFFF), .10f));
    }

    tx_backdrop(top);
    if (glyph[0] != ' ')
        tx_draw(c, glyph, x + (cw - 1.6f)*0.5f, y + ch*0.5f,
                font_track(TF_MONO, (int)(ch*0.62f), TW_BOLD, 0),
                ink, AL_C, AV_M);
}

static void flap_text(App *a, float x, float y, float cw, float ch,
                      const char *text, int cells, int row, int cell0,
                      Color ink, int bright)
{
    int atEnd = 0;
    for (int i = 0; i < cells; i++) {
        if (!text[i]) atEnd = 1;
        char t = atEnd ? ' ' : text[i];
        flap_cell(a, x + i*cw, y, cw, ch, t, row, cell0 + i, ink, bright);
    }
}

/* ==========================================================================
 *  the board
 * ========================================================================== */

/* how long a completed movement stays on the board, minutes */
#define BOARD_RETAIN_MIN 40.f

static int board_rows(App *a, int *idx, int cap, int arrivals)
{
    World *w = &a->w;
    int n = 0;
    char q[64];
    snprintf(q, sizeof q, "%s", a->searchBoard);
    for (char *p = q; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);

    for (int i = 0; i < w->nFlights && n < cap; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival != arrivals) continue;

        /*  A published board lists what is about to happen; it is not a log
         *  of the day.  Once an aeroplane has actually gone the row stays up
         *  a little longer and is then dropped, which is what the real board
         *  at Plaisance does.  Without this the display is honest but
         *  useless: opened at nine in the evening it is a wall of DEPARTED
         *  with the next movement somewhere below the fold.
         *
         *  A search overrides the window, so an earlier movement can still
         *  be looked up by number or by city.                              */
        if (!q[0] && (f->state == FS_DEPARTED || f->state == FS_ONBLOCK) &&
            w->clock - (float)f->estMin > BOARD_RETAIN_MIN) continue;

        if (q[0]) {
            char no[16], city[32];
            snprintf(no, sizeof no, "%s", f->no);
            snprintf(city, sizeof city, "%s", w->airport[f->airport].city);
            for (char *p = no;   *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            for (char *p = city; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
            if (!strstr(no, q) && !strstr(city, q)) continue;
        }
        idx[n++] = i;
    }
    /* sort by estimate */
    for (int i = 1; i < n; i++) {
        int k = idx[i], j = i - 1;
        while (j >= 0 && w->flight[idx[j]].estMin > w->flight[k].estMin) {
            idx[j+1] = idx[j]; j--;
        }
        idx[j+1] = k;
    }
    return n;
}

static const char *short_status(Flight *f, char *buf)
{
    switch (f->state) {
    case FS_BOARDING:  sprintf(buf, "BOARDING");  break;
    case FS_FINAL:     sprintf(buf, "FINAL CALL");break;
    case FS_CLOSED:    sprintf(buf, "GATE CLOSED");break;
    case FS_CHECKIN:   sprintf(buf, "CHECK-IN");  break;
    case FS_PUSHBACK:  sprintf(buf, "PUSHBACK");  break;
    case FS_TAXI_OUT:  sprintf(buf, "TAXIING");   break;
    case FS_LINEUP:    sprintf(buf, "LINE UP");   break;
    case FS_DEPARTED:  sprintf(buf, "DEPARTED");  break;
    case FS_ENROUTE:   sprintf(buf, "EN ROUTE");  break;
    case FS_APPROACH:  sprintf(buf, "APPROACHING");break;
    case FS_LANDED:    sprintf(buf, "LANDED");    break;
    case FS_TAXI_IN:   sprintf(buf, "TAXIING IN");break;
    case FS_ONBLOCK:   sprintf(buf, "ARRIVED");   break;
    case FS_CANCELLED: sprintf(buf, "CANCELLED"); break;
    case FS_DELAYED:   sprintf(buf, "DELAYED");   break;
    default:           sprintf(buf, "SCHEDULED"); break;
    }
    return buf;
}


/* ==========================================================================
 *  the forward timetable
 *
 *  The live board shows what is happening now.  This shows what is published
 *  for any of the next ninety-two days -- which is a different question, and
 *  the one a passenger booking a flight in October is actually asking.
 *
 *  None of it is stored.  A date is handed to schedule_for_day(), which reads
 *  the same weekly operating patterns the generator uses, so what is shown
 *  here is exactly what the airport will run that day.
 * ========================================================================== */

/*  Recomputed only when the date changes.  Rebuilding sixty rows and sorting
 *  them every frame would be wasted work on a panel that changes when a
 *  button is pressed and at no other time.                                 */
static long     g_schedDay = -1;
static SchedRow g_schedRow[MAX_FLIGHTS];
static int      g_schedN   = 0;

static void sched_ensure(const World *wo, long day)
{
    if (day == g_schedDay) return;
    g_schedDay = day;
    g_schedN   = schedule_for_day(wo, day, g_schedRow, MAX_FLIGHTS);
}

/*  Movements per day across the whole season, for the strip.  Fixed for the
 *  life of the process, so it is built once.                               */
static int  g_seasonN[SCHEDULE_DAYS];
static long g_seasonBase = -1;

static void season_ensure(const World *wo)
{
    if (g_seasonBase == wo->baseDay) return;
    g_seasonBase = wo->baseDay;
    for (int i = 0; i < SCHEDULE_DAYS; i++)
        g_seasonN[i] = schedule_movements(wo, wo->baseDay + i);
}

static void draw_schedule(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;
    float pad  = PAD;

    if (a->schedOffset < 0)               a->schedOffset = 0;
    if (a->schedOffset >= SCHEDULE_DAYS)  a->schedOffset = SCHEDULE_DAYS - 1;

    season_ensure(wo);
    long day = wo->baseDay + a->schedOffset;
    sched_ensure(wo, day);

    int yy, mm, dd;
    civil_from_days(day, &yy, &mm, &dd);
    int wd = (int)(((day % 7) + 7 + 4) % 7);

    float bx = x + pad, by = y + pad + 56.f;
    float bw = w - pad*2.f - 348.f;
    float bh = h - (by - y) - pad;

    /* ---- date navigation ------------------------------------------------ */
    ui_card(bx, by, bw, 78.f, R_LG);
    tx_backdrop(C_SURF);

    if (ui_icon_btn(uid("schprev"), bx + 14.f, by + 21.f, 36.f, IC_CHEV_L,
                    BTN_SOFT)) a->schedOffset--;
    if (ui_icon_btn(uid("schnext"), bx + 56.f, by + 21.f, 36.f, IC_CHEV_R,
                    BTN_SOFT)) a->schedOffset++;

    char dstr[80];
    snprintf(dstr, sizeof dstr, "%s %d %s %d",
             weekday_name(wd), dd, month_name(mm), yy);
    tx_draw(c, dstr, bx + 108.f, by + 16.f,
            font_track(TF_DISPLAY, 21, TW_BOLD, 0), C_INK, AL_L, AV_T);

    char rel[64];
    if (a->schedOffset == 0)      snprintf(rel, sizeof rel, "today");
    else if (a->schedOffset == 1) snprintf(rel, sizeof rel, "tomorrow");
    else snprintf(rel, sizeof rel, "in %d days", a->schedOffset);
    char sub2[110];
    snprintf(sub2, sizeof sub2, "%s   -   %d movements published", rel, g_schedN);
    tx_clipped(c, sub2, bx + 108.f, by + 46.f, bw - 300.f,
               font_make(TF_UI, 12, TW_MED), C_INK_3, AL_L, AV_T);

    if (a->schedOffset != 0 &&
        ui_button(uid("schtoday"), bx + bw - 190.f, by + 21.f, 84.f, 36.f,
                  "Today", BTN_OUTLINE)) a->schedOffset = 0;
    if (ui_button(uid("schweek"), bx + bw - 98.f, by + 21.f, 84.f, 36.f,
                  "+1 week", BTN_SOFT)) a->schedOffset += 7;

    /* ---- the season strip ----------------------------------------------- */
    float sy = by + 90.f, sh2 = 60.f;
    ui_card(bx, sy, bw, sh2, R_LG);
    float inner = bw - 28.f;
    float colw  = inner / (float)SCHEDULE_DAYS;
    int   peak  = 1;
    for (int i = 0; i < SCHEDULE_DAYS; i++)
        if (g_seasonN[i] > peak) peak = g_seasonN[i];

    for (int i = 0; i < SCHEDULE_DAYS; i++) {
        float cxx = bx + 14.f + i*colw;
        float frac = (float)g_seasonN[i] / (float)peak;
        /* capped so the tallest bar still clears the month ticks above it */
        float barH = 6.f + frac * 22.f;
        int   sel  = (i == a->schedOffset);
        int   hov  = ui_hit(cxx, sy + 8.f, colw, sh2 - 16.f);
        if (hov) {
            ui_cursor(1);
            if (a->in.pressed) a->schedOffset = i;
        }
        Color bc = sel ? C_V600 : (hov ? C_V400 : col_alpha(C_V400, .40f));
        /* the first of each month gets a tick, so the strip reads as a
         * calendar rather than an undifferentiated run of bars */
        int y2, m2, d2;
        civil_from_days(wo->baseDay + i, &y2, &m2, &d2);
        if (d2 == 1) {
            cv_rect(c, cxx, sy + 18.f, 1.f, sh2 - 26.f, col_alpha(C_LINE_2, .9f));
            char mn[6];
            snprintf(mn, sizeof mn, "%.3s", month_name(m2));
            tx_backdrop(C_SURF);
            tx_draw(c, mn, cxx + 3.f, sy + 5.f,
                    font_track(TF_UI, 8, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
        }
        cv_rrect(c, cxx + 0.6f, sy + sh2 - 10.f - barH, colw - 1.2f, barH,
                 1.5f, bc);
        if (sel)
            cv_rrect(c, cxx - 1.f, sy + 16.f, colw + 2.f, sh2 - 24.f, 4.f,
                     col_alpha(C_V600, .12f));
    }

    /* ---- the timetable -------------------------------------------------- */
    float ly = sy + sh2 + 14.f;
    float lh = bh - (ly - by);
    ui_panel_dark(bx, ly, bw, lh, R_LG);
    cv_clip_push(c, bx + 1.f, ly + 1.f, bw - 2.f, lh - 2.f);

    Paint hg = paint_linear(bx, ly, bx, ly + 36.f, HEX(0x2A1560), HEX(0x1B0C39));
    cv_rect_p(c, bx, ly, bw, 36.f, &hg);
    tx_backdrop(HEX(0x2A1560));
    const char *cap[5] = { "TIME", "FLIGHT", "", "ROUTE", "AIRCRAFT" };
    float cX[5] = { bx + 18.f, bx + 84.f, bx + 168.f, bx + 214.f,
                    bx + bw - 130.f };
    for (int i = 0; i < 5; i++)
        if (cap[i][0])
            tx_draw(c, cap[i], cX[i], ly + 18.f, font_track(TF_UI, 9, TW_BOLD, 2),
                    col_alpha(C_V300, .8f), AL_L, AV_M);

    float rowH = 26.f;
    float listY = ly + 40.f, listH = lh - 44.f;
    float off = ui_scroll_begin(uid("schscroll"), bx + 2.f, listY, bw - 4.f,
                                listH, g_schedN*rowH + 8.f);
    for (int i = 0; i < g_schedN; i++) {
        SchedRow *r = &g_schedRow[i];
        float ry = listY + i*rowH - off;
        if (ry + rowH < listY || ry > listY + listH) continue;

        if (i & 1) cv_rect(c, bx + 6.f, ry, bw - 12.f, rowH,
                           col_alpha(HEX(0xFFFFFF), .028f));
        tx_backdrop(C_NIGHT_2);

        char hm[8]; fmt_hhmm(r->schedMin, hm);
        tx_draw(c, hm, cX[0], ry + rowH*0.5f, font_track(TF_MONO, 13, TW_BOLD, 1),
                C_GOLD, AL_L, AV_M);
        tx_draw(c, r->no, cX[1], ry + rowH*0.5f,
                font_track(TF_MONO, 13, TW_SEMI, 0), HEX(0xFFFFFF), AL_L, AV_M);

        icon_draw(c, r->arrival ? IC_LANDING : IC_TAKEOFF,
                  cX[2] + 9.f, ry + rowH*0.5f, 14.f,
                  r->arrival ? HEX(0x7CD4FF) : HEX(0x9BE8AE));

        char route[64];
        snprintf(route, sizeof route, "%s  %s  %s",
                 r->arrival ? r->destIata : "MRU", "->",
                 r->arrival ? "MRU" : r->destIata);
        tx_draw(c, route, cX[3], ry + rowH*0.5f,
                font_track(TF_MONO, 11, TW_MED, 0), col_alpha(C_V300, .95f),
                AL_L, AV_M);
        tx_clipped(c, r->dest, cX[3] + 96.f, ry + rowH*0.5f - 7.f,
                   cX[4] - cX[3] - 108.f, font_make(TF_UI, 12, TW_MED),
                   HEX(0xE6DCFF), AL_L, AV_T);
        tx_draw(c, r->ac, bx + bw - 18.f, ry + rowH*0.5f,
                font_track(TF_MONO, 11, TW_MED, 0), col_alpha(C_V300, .8f),
                AL_R, AV_M);
    }
    ui_scroll_end();
    for (float g = ly + 36.f; g < ly + lh; g += 3.f)
        cv_rect(c, bx, g, bw, 1.f, col_alpha(HEX(0x000000), .07f));
    cv_clip_pop(c);

    /* ---- the day at a glance -------------------------------------------- */
    float dx = bx + bw + 16.f, dw = w - pad*2.f - bw - 16.f;
    ui_card(dx, by, dw, h - (by - y) - pad, R_LG);
    tx_backdrop(C_SURF);

    Paint g2 = paint_linear(dx, by, dx + dw, by, col_alpha(C_V500, .14f),
                            col_alpha(C_V500, .02f));
    cv_rrect_p(c, dx, by, dw, 84.f, R_LG, &g2);
    tx_draw(c, "THE DAY", dx + 20.f, by + 16.f, font_track(TF_UI, 9, TW_BOLD, 2),
            C_V600, AL_L, AV_T);
    tx_clipped(c, dstr, dx + 20.f, by + 34.f, dw - 40.f,
               font_track(TF_DISPLAY, 19, TW_BOLD, 0), C_INK, AL_L, AV_T);

    int arr = 0, dep = 0, wide = 0, first = 1441, last = -1;
    int perHour[24];
    memset(perHour, 0, sizeof perHour);
    for (int i = 0; i < g_schedN; i++) {
        SchedRow *r = &g_schedRow[i];
        if (r->arrival) arr++; else dep++;
        if (strcmp(r->ac, "77W") == 0 || strcmp(r->ac, "789") == 0 ||
            strcmp(r->ac, "359") == 0 || strcmp(r->ac, "339") == 0 ||
            strcmp(r->ac, "333") == 0 || strcmp(r->ac, "332") == 0) wide++;
        if (r->schedMin < first) first = r->schedMin;
        if (r->schedMin > last)  last  = r->schedMin;
        int hh = r->schedMin / 60;
        if (hh >= 0 && hh < 24) perHour[hh]++;
    }

    float ry2 = by + 100.f;
    float tw2 = (dw - 40.f - 16.f) / 3.f;
    struct { const char *k; char v[12]; Color col; } T[3];
    snprintf(T[0].v, 12, "%d", arr);  T[0].k = "ARRIVALS";   T[0].col = C_TEAL;
    snprintf(T[1].v, 12, "%d", dep);  T[1].k = "DEPARTURES"; T[1].col = C_V600;
    snprintf(T[2].v, 12, "%d", wide); T[2].k = "WIDEBODY";   T[2].col = C_MAGENTA;
    for (int i = 0; i < 3; i++) {
        float tx2 = dx + 20.f + i*(tw2 + 8.f);
        cv_rrect(c, tx2, ry2, tw2, 58.f, 10.f, col_alpha(T[i].col, .09f));
        tx_draw(c, T[i].k, tx2 + 11.f, ry2 + 9.f, font_track(TF_UI, 8, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        tx_draw(c, T[i].v, tx2 + 11.f, ry2 + 22.f,
                font_track(TF_DISPLAY, 22, TW_BOLD, 0), T[i].col, AL_L, AV_T);
    }
    ry2 += 74.f;

    char band[64];
    if (last >= 0) {
        char f1[8], f2[8];
        fmt_hhmm(first, f1); fmt_hhmm(last, f2);
        snprintf(band, sizeof band, "First %s   -   last %s", f1, f2);
    } else snprintf(band, sizeof band, "No movements");
    tx_clipped(c, band, dx + 20.f, ry2, dw - 40.f, font_make(TF_UI, 12, TW_MED),
               C_INK_2, AL_L, AV_T);
    ry2 += 26.f;

    /* movements by hour */
    tx_draw(c, "BY HOUR", dx + 20.f, ry2, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry2 += 18.f;
    int hpeak = 1;
    for (int i = 0; i < 24; i++) if (perHour[i] > hpeak) hpeak = perHour[i];
    float hw = (dw - 40.f) / 24.f;
    for (int i = 0; i < 24; i++) {
        float bh2 = 4.f + 44.f * (float)perHour[i] / (float)hpeak;
        Color hc = perHour[i] >= hpeak ? C_MAGENTA : C_V400;
        cv_rrect(c, dx + 20.f + i*hw, ry2 + 48.f - bh2, hw - 2.f, bh2, 2.f,
                 col_alpha(hc, perHour[i] ? .85f : .18f));
    }
    for (int i = 0; i < 24; i += 6) {
        char lab[6]; snprintf(lab, sizeof lab, "%02d", i);
        tx_draw(c, lab, dx + 20.f + i*hw, ry2 + 52.f, font_make(TF_UI, 9, TW_MED),
                C_INK_4, AL_L, AV_T);
    }
    ry2 += 76.f;

    ui_divider(dx + 20.f, ry2, dw - 40.f);
    ry2 += 14.f;
    /* not "today" -- this panel describes whichever date is selected */
    tx_draw(c, "CARRIERS THAT DAY", dx + 20.f, ry2,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    ry2 += 20.f;

    int count[MAX_AIRLINES];
    memset(count, 0, sizeof count);
    for (int i = 0; i < g_schedN; i++)
        if (g_schedRow[i].airline >= 0 && g_schedRow[i].airline < MAX_AIRLINES)
            count[g_schedRow[i].airline]++;

    float limit = by + h - (by - y) - pad - 20.f;
    for (int pass = 0; pass < wo->nAirlines && ry2 < limit; pass++) {
        int best = -1;
        for (int i = 0; i < wo->nAirlines; i++)
            if (count[i] > 0 && (best < 0 || count[i] > count[best])) best = i;
        if (best < 0) break;

        cv_rrect(c, dx + 20.f, ry2 + 3.f, 4.f, 13.f, 2.f, wo->airline[best].col);
        tx_clipped(c, wo->airline[best].name, dx + 32.f, ry2, dw - 100.f,
                   font_make(TF_UI, 12, TW_MED), C_INK_2, AL_L, AV_T);
        char nb[16];
        snprintf(nb, sizeof nb, "%d", count[best]);
        tx_draw(c, nb, dx + dw - 20.f, ry2, font_make(TF_UI, 12, TW_SEMI),
                C_INK, AL_R, AV_T);
        count[best] = 0;
        ry2 += 21.f;
    }
}

void screen_board(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    /* ---- controls -------------------------------------------------------- */
    float cy = y + pad;
    if (ui_tab(uid("tabdep"), x + pad, cy, 140.f, 40.f, "Departures",
               IC_TAKEOFF, !a->boardMode && !a->boardArrivals)) {
        a->boardArrivals = 0; a->boardMode = 0;
    }
    if (ui_tab(uid("tabarr"), x + pad + 144.f, cy, 126.f, 40.f, "Arrivals",
               IC_LANDING, !a->boardMode && a->boardArrivals)) {
        a->boardArrivals = 1; a->boardMode = 0;
    }
    if (ui_tab(uid("tabsch"), x + pad + 274.f, cy, 136.f, 40.f, "Schedule",
               IC_CALENDAR, a->boardMode)) a->boardMode = 1;

    /*  The live board and the published timetable answer different
     *  questions, so they are different panels rather than one panel with a
     *  date on it: nothing on the forward view has a stand, a gate or a
     *  state, because none of that is decided until the day arrives.       */
    if (a->boardMode) { draw_schedule(a, x, y, w, h); return; }

    ui_text_field(uid("boardsearch"), x + pad + 420.f, cy, 238.f, 40.f,
                  a->searchBoard, sizeof a->searchBoard,
                  "Search flight or city", IC_SEARCH);

    /*  Counted from the rows that are actually displayed.  A summary that
     *  described the whole day while the board showed a window of it would
     *  be quietly wrong, and that is the kind of wrong nobody notices.     */
    int idx[64];
    int n = board_rows(a, idx, 64, a->boardArrivals);
    int delayed = 0, onTime = 0;
    for (int i = 0; i < n; i++) {
        if (wo->flight[idx[i]].delayMin >= 15) delayed++; else onTime++;
    }
    char st[80];
    snprintf(st, sizeof st, "%d showing   -   %d on time   -   %d delayed",
             n, onTime, delayed);
    tx_backdrop(C_BG);
    tx_draw(c, st, x + w - pad, cy + 20.f, font_make(TF_UI, 12, TW_MED),
            C_INK_3, AL_R, AV_M);

    /* ---- the split-flap board ------------------------------------------- */
    float bx = x + pad, by = cy + 56.f;
    float bw = w - pad*2.f - 348.f;
    float bh = h - (by - y) - pad;

    ui_panel_dark(bx, by, bw, bh, R_LG);
    cv_clip_push(c, bx + 1.f, by + 1.f, bw - 2.f, bh - 2.f);

    /* header */
    Paint hg = paint_linear(bx, by, bx, by + 44.f, HEX(0x2A1560), HEX(0x1B0C39));
    cv_rect_p(c, bx, by, bw, 44.f, &hg);
    tx_backdrop(HEX(0x2A1560));
    tx_draw(c, a->boardArrivals ? "ARRIVALS" : "DEPARTURES", bx + 18.f, by + 22.f,
            font_track(TF_DISPLAY, 16, TW_BOLD, 4), HEX(0xFFFFFF), AL_L, AV_M);
    char clk[16]; fmt_hhmmss(wo->clock, clk);
    tx_draw(c, clk, bx + bw - 18.f, by + 22.f,
            font_track(TF_MONO, 16, TW_BOLD, 1), C_GOLD, AL_R, AV_M);

    /* column captions */
    float colX[5];
    float cw = 12.4f, chh = 24.f;
    float ox = bx + 18.f;
    colX[0] = ox;                       /* time   5  */
    colX[1] = ox + 5*cw + 16.f;         /* flight 7  */
    colX[2] = colX[1] + 7*cw + 16.f;    /* dest  14  */
    colX[3] = colX[2] + 14*cw + 16.f;   /* gate   3  */
    colX[4] = colX[3] + 3*cw + 16.f;    /* status 12 */

    const char *caps[5] = { "TIME", "FLIGHT", a->boardArrivals ? "ORIGIN"
                            : "DESTINATION", a->boardArrivals ? "BELT" : "GATE",
                            "REMARKS" };
    tx_backdrop(C_NIGHT_2);
    for (int i = 0; i < 5; i++)
        tx_draw(c, caps[i], colX[i], by + 56.f, font_track(TF_UI, 9, TW_BOLD, 2),
                col_alpha(C_V300, .8f), AL_L, AV_T);

    float listY = by + 74.f;
    float listH = bh - (listY - by) - 12.f;
    float rowH = chh + 8.f;
    float off = ui_scroll_begin(uid("boardscroll"), bx + 2.f, listY, bw - 4.f,
                                listH, n*rowH + 6.f);

    for (int i = 0; i < n; i++) {
        Flight *f = &wo->flight[idx[i]];
        float ry = listY + i*rowH - off;
        if (ry + rowH < listY || ry > listY + listH) continue;

        int sel = (a->selFlight == f->id);
        if (ui_hit(bx + 8.f, ry - 3.f, bw - 16.f, rowH)) {
            ui_cursor(1);
            cv_rrect(c, bx + 8.f, ry - 3.f, bw - 16.f, rowH,
                     6.f, col_alpha(C_V400, .10f));
            if (a->in.pressed) a->selFlight = f->id;
        }
        if (sel) cv_rrect(c, bx + 8.f, ry - 3.f, bw - 16.f, rowH, 6.f,
                          col_alpha(C_V400, .16f));

        char tbuf[8];  fmt_hhmm(f->estMin, tbuf);
        char nbuf[10]; snprintf(nbuf, sizeof nbuf, "%-7s", f->no);
        char dbuf[20];
        snprintf(dbuf, sizeof dbuf, "%-14s", wo->airport[f->airport].city);
        char gbuf[6];
        if (a->boardArrivals) snprintf(gbuf, sizeof gbuf, "%-3s", f->belt);
        else { char g[6]; ordinal_gate(f->gate, g); snprintf(gbuf, sizeof gbuf, "%-3s", g); }
        char sbuf[16]; short_status(f, sbuf);
        char spad[16]; snprintf(spad, sizeof spad, "%-12s", sbuf);

        Color ink = C_GOLD;
        if (f->state == FS_BOARDING || f->state == FS_ONBLOCK) ink = HEX(0x5CE07A);
        if (f->state == FS_FINAL)     ink = HEX(0xFFB33C);
        if (f->state == FS_CANCELLED || f->state == FS_DELAYED) ink = HEX(0xFF6B6B);
        if (f->state == FS_APPROACH)  ink = HEX(0x7CD4FF);

        flap_text(a, colX[0], ry, cw, chh, tbuf,  5, i,  0, C_GOLD, 0);
        flap_text(a, colX[1], ry, cw, chh, nbuf,  7, i,  5, HEX(0xFFFFFF), 1);
        flap_text(a, colX[2], ry, cw, chh, dbuf, 14, i, 12, HEX(0xE6DCFF), 0);
        flap_text(a, colX[3], ry, cw, chh, gbuf,  3, i, 26, C_GOLD, 1);
        flap_text(a, colX[4], ry, cw, chh, spad, 12, i, 29, ink, 0);
    }
    ui_scroll_end();

    if (n == 0) {
        tx_backdrop(C_NIGHT_2);
        const char *msg = a->searchBoard[0]
            ? "No movements match that search"
            : (a->boardArrivals ? "No further arrivals today"
                                : "No further departures today");
        tx_draw(c, msg, bx + bw*0.5f, by + bh*0.5f - 10.f,
                font_make(TF_UI, 13, TW_MED), col_alpha(C_V300, .8f), AL_C, AV_M);
        if (!a->searchBoard[0])
            tx_draw(c, "Open Schedule to see the days ahead",
                    bx + bw*0.5f, by + bh*0.5f + 12.f,
                    font_make(TF_UI, 11, TW_REG), col_alpha(C_V300, .55f),
                    AL_C, AV_M);
    }

    /* a faint scanline texture sells the panel as a physical board */
    for (float sy = by + 44.f; sy < by + bh; sy += 3.f)
        cv_rect(c, bx, sy, bw, 1.f, col_alpha(HEX(0x000000), .07f));

    cv_clip_pop(c);

    /* ---- detail column --------------------------------------------------- */
    float dx = bx + bw + 16.f, dw = w - pad*2.f - bw - 16.f;
    Flight *f = a->selFlight ? flight_by_id(wo, a->selFlight) : NULL;

    ui_card(dx, by, dw, bh, R_LG);
    tx_backdrop(C_SURF);
    if (!f) {
        icon_draw(c, IC_PLANE, dx + dw*0.5f, by + bh*0.5f - 20.f, 32.f, C_INK_4);
        tx_draw(c, "Select a movement", dx + dw*0.5f, by + bh*0.5f + 10.f,
                font_make(TF_UI, 13, TW_MED), C_INK_3, AL_C, AV_M);
        tx_draw(c, "Rows are live -- watch the flaps turn", dx + dw*0.5f,
                by + bh*0.5f + 30.f, font_make(TF_UI, 11, TW_REG), C_INK_4,
                AL_C, AV_M);
        return;
    }

    Color ac = wo->airline[f->airline].col;
    Paint g = paint_linear(dx, by, dx + dw, by + 92.f, col_alpha(ac, .16f),
                           col_alpha(ac, .02f));
    cv_rrect_p(c, dx, by, dw, 92.f, R_LG, &g);
    tx_draw(c, f->no, dx + 20.f, by + 16.f, font_track(TF_DISPLAY, 27, TW_BOLD, 1),
            C_INK, AL_L, AV_T);
    tx_clipped(c, wo->airline[f->airline].name, dx + 20.f, by + 50.f, dw - 40.f,
               font_make(TF_UI, 12, TW_SEMI), C_INK_2, AL_L, AV_T);
    char rt[64];
    snprintf(rt, sizeof rt, "%s  %s  %s",
             f->arrival ? wo->airport[f->airport].iata : "MRU",
             f->arrival ? "->" : "->",
             f->arrival ? "MRU" : wo->airport[f->airport].iata);
    tx_draw(c, rt, dx + 20.f, by + 68.f, font_track(TF_MONO, 13, TW_BOLD, 1),
            C_V600, AL_L, AV_T);

    /* codeshare line, as the live arrivals board prints it */
    if (f->nShares > 0) {
        char cs[64];
        int u = snprintf(cs, sizeof cs, "also ");
        for (int i = 0; i < f->nShares; i++)
            u += snprintf(cs + u, sizeof cs - u, "%s%s",
                          i ? " / " : "", f->shares[i]);
        tx_clipped(c, cs, dx + 20.f, by + 84.f, dw - 40.f,
                   font_track(TF_MONO, 11, TW_MED, 0), C_INK_3, AL_L, AV_T);
    }

    float ry = by + 106.f;
    struct { const char *k; char v[46]; } rows[9];
    int nr = 0;
    char sch[8], est[8];
    fmt_hhmm(f->schedMin, sch); fmt_hhmm(f->estMin, est);

    snprintf(rows[nr].v, 46, "%s", wo->airport[f->airport].city); rows[nr++].k = "City";
    snprintf(rows[nr].v, 46, "%s", wo->airport[f->airport].country); rows[nr++].k = "Country";
    snprintf(rows[nr].v, 46, "%s", wo->actype[f->acType].name); rows[nr++].k = "Aircraft";
    snprintf(rows[nr].v, 46, "%s", f->reg); rows[nr++].k = "Registration";
    snprintf(rows[nr].v, 46, "%s", sch); rows[nr++].k = "Scheduled";
    snprintf(rows[nr].v, 46, "%s%s", est, f->delayMin ? "  late" : ""); rows[nr++].k = "Estimated";
    snprintf(rows[nr].v, 46, "%s", f->stand >= 0 ? wo->stand[f->stand].name : "--");
    rows[nr++].k = "Stand";
    snprintf(rows[nr].v, 46, "%d / %d  (%.0f%%)", f->pax, f->paxCap,
             f->paxCap ? 100.f*f->pax/f->paxCap : 0.f); rows[nr++].k = "Load";
    snprintf(rows[nr].v, 46, "%dh %02dm", wo->airport[f->airport].flightMin/60,
             wo->airport[f->airport].flightMin%60); rows[nr++].k = "Block time";

    for (int i = 0; i < nr; i++) {
        if (i & 1) cv_rrect(c, dx + 12.f, ry - 4.f, dw - 24.f, 26.f, 6.f, C_SURF_2);
        tx_draw(c, rows[i].k, dx + 20.f, ry, font_make(TF_UI, 11, TW_MED),
                C_INK_3, AL_L, AV_T);
        tx_clipped(c, rows[i].v, dx + dw - 20.f, ry, dw - 130.f,
                   font_make(TF_UI, 12, TW_SEMI), C_INK, AL_R, AV_T);
        ry += 27.f;
    }

    ry += 8.f;
    ui_divider(dx + 20.f, ry, dw - 40.f);
    ry += 14.f;

    if (!f->arrival) {
        tx_draw(c, "BOARDING PROGRESS", dx + 20.f, ry,
                font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
        float bp = f->pax ? (float)f->boarded / f->pax : 0.f;
        ui_meter(dx + 20.f, ry + 18.f, dw - 40.f, 8.f, bp, C_V400, C_V700);
        char bt[48];
        snprintf(bt, sizeof bt, "%d of %d boarded", f->boarded, f->pax);
        tx_draw(c, bt, dx + 20.f, ry + 32.f, font_make(TF_UI, 11, TW_MED),
                C_INK_3, AL_L, AV_T);
        ry += 56.f;

        tx_draw(c, "BAGS LOADED", dx + 20.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        int bagsFor = 0;
        for (int i = 0; i < wo->nBags; i++) if (wo->bag[i].flight == f->id) bagsFor++;
        float gp = bagsFor ? (float)f->bagsLoaded / bagsFor : 0.f;
        ui_meter(dx + 20.f, ry + 18.f, dw - 40.f, 8.f, gp, C_TEAL,
                 col_darken(C_TEAL, .25f));
        char gt[48];
        snprintf(gt, sizeof gt, "%d of %d in the register", f->bagsLoaded, bagsFor);
        tx_draw(c, gt, dx + 20.f, ry + 32.f, font_make(TF_UI, 11, TW_MED),
                C_INK_3, AL_L, AV_T);
        ry += 60.f;
    }

    if (ry < by + bh - 50.f) {
        if (ui_button_i(uid("boardask"), dx + 20.f, by + bh - 56.f, dw - 40.f, 40.f,
                        "Ask AURA about this flight", IC_SPARK, BTN_PRIMARY)) {
            char q[80];
            snprintf(q, sizeof q, "status of %s", f->no);
            ai_chat_send(&a->w, &a->chat, q);
            a->chatDock = 1;
        }
    }
}
