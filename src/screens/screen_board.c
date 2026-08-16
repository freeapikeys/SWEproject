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
 * ------------------------------------------------------------------------- */

static void flap_cell(App *a, float x, float y, float cw, float ch,
                      char target, uint64_t id, Color ink, int bright)
{
    Canvas *c = &a->cv;
    float *shown = ui_state(id,            (float)flap_index(target));
    float *want  = ui_state(id ^ 0x11ULL,  (float)flap_index(target));
    float *roll  = ui_state(id ^ 0x22ULL,  0.f);

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
                      const char *text, int cells, uint64_t base, Color ink,
                      int bright)
{
    for (int i = 0; i < cells; i++) {
        char t = text[i] ? text[i] : ' ';
        if (!text[i]) t = ' ';
        flap_cell(a, x + i*cw, y, cw, ch, t, base ^ (uint64_t)(i*2654435761u),
                  ink, bright);
        if (!text[i]) continue;
    }
}

/* ==========================================================================
 *  the board
 * ========================================================================== */

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

void screen_board(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    /* ---- controls -------------------------------------------------------- */
    float cy = y + pad;
    if (ui_tab(uid("tabdep"), x + pad, cy, 150.f, 40.f, "Departures",
               IC_TAKEOFF, !a->boardArrivals)) a->boardArrivals = 0;
    if (ui_tab(uid("tabarr"), x + pad + 154.f, cy, 138.f, 40.f, "Arrivals",
               IC_LANDING, a->boardArrivals)) a->boardArrivals = 1;

    ui_text_field(uid("boardsearch"), x + pad + 306.f, cy, 250.f, 40.f,
                  a->searchBoard, sizeof a->searchBoard,
                  "Search flight or city", IC_SEARCH);

    int total = 0, delayed = 0, onTime = 0;
    for (int i = 0; i < wo->nFlights; i++) {
        if (wo->flight[i].arrival != a->boardArrivals) continue;
        total++;
        if (wo->flight[i].delayMin >= 15) delayed++; else onTime++;
    }
    char st[64];
    snprintf(st, sizeof st, "%d movements   -   %d on time   -   %d delayed",
             total, onTime, delayed);
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

    int idx[64];
    int n = board_rows(a, idx, 64, a->boardArrivals);

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

        uint64_t rb = uidi("flap", f->id);
        flap_text(a, colX[0], ry, cw, chh, tbuf,  5,  rb ^ 1ULL, C_GOLD, 0);
        flap_text(a, colX[1], ry, cw, chh, nbuf,  7,  rb ^ 2ULL, HEX(0xFFFFFF), 1);
        flap_text(a, colX[2], ry, cw, chh, dbuf, 14,  rb ^ 3ULL, HEX(0xE6DCFF), 0);
        flap_text(a, colX[3], ry, cw, chh, gbuf,  3,  rb ^ 4ULL, C_GOLD, 1);
        flap_text(a, colX[4], ry, cw, chh, spad, 12,  rb ^ 5ULL, ink, 0);
    }
    ui_scroll_end();

    if (n == 0) {
        tx_backdrop(C_NIGHT_2);
        tx_draw(c, "No movements match that search", bx + bw*0.5f, by + bh*0.5f,
                font_make(TF_UI, 13, TW_MED), col_alpha(C_V300, .7f), AL_C, AV_M);
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
