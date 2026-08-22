/* ==========================================================================
 *  AURA :: screen_checkin.c   --   check-in hall and boarding
 *
 *  The desk bank across the top is live: each column is one of the twenty
 *  four positions, the stack of figures beneath it is its queue, and opening
 *  or closing a desk changes how quickly that queue drains in the simulation.
 *
 *  Selecting a passenger issues a boarding pass -- drawn, not templated --
 *  and shows their seat on a cabin plan built from the aircraft type.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 *  a queueing figure: a head and shoulders, small enough to repeat
 * ------------------------------------------------------------------------- */

static void figure(Canvas *c, float x, float y, float s, Color col, float bob)
{
    float b = sinf(bob) * s * 0.10f;
    cv_circle(c, x, y - s*0.62f + b, s*0.27f, col);
    Path p; path_reset(&p);
    path_move(&p, x - s*0.34f, y + s*0.45f + b);
    path_cubic(&p, x - s*0.34f, y - s*0.20f + b,
                   x + s*0.34f, y - s*0.20f + b,
                   x + s*0.34f, y + s*0.45f + b);
    path_close(&p);
    cv_fill_col(c, &p, col);
}

/* --------------------------------------------------------------------------
 *  desk bank
 * ------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------
 *  the live queues, and booking somebody onto a flight
 *
 *  Both lanes are the queue ADT in queue.c, driven from the flight programme
 *  in main.c.  What is drawn here is the actual contents of those structures,
 *  not a number made up for the display.
 * ------------------------------------------------------------------------- */

static void draw_queue_lane(App *a, float x, float y, float w,
                            const char *label, const Fifo *q, Color col)
{
    Canvas *c = &a->cv;
    int n = fifo_size(q);

    tx_backdrop(C_SURF);
    tx_draw(c, label, x, y, font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3,
            AL_L, AV_T);
    char v[40];
    snprintf(v, sizeof v, "%d waiting", n);
    tx_draw(c, v, x + w, y, font_make(TF_UI, 11, TW_SEMI), col, AL_R, AV_T);

    /* one figure per person, up to what fits */
    float fy = y + 18.f;
    int show = n;
    int maxShow = (int)((w - 4.f) / 11.f);
    if (show > maxShow) show = maxShow;
    for (int i = 0; i < show; i++)
        figure(c, x + 4.f + (float)i*11.f, fy, 9.f, col_alpha(col, .85f),
               anim_time()*2.f + (float)i*0.4f);
    if (n > show) {
        char more[14];
        snprintf(more, sizeof more, "+%d", n - show);
        tx_draw(c, more, x + 4.f + (float)show*11.f, fy + 4.f,
                font_make(TF_UI, 10, TW_BOLD), col, AL_L, AV_T);
    }
}

static void draw_queues_and_register(App *a, float x, float y, float w, float h)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "QUEUES", x + 18.f, y + 14.f, font_track(TF_UI, 10, TW_BOLD, 2),
            C_V600, AL_L, AV_T);

    float est = pq_wait_estimate(&a->qSecurity, 0.2f);
    char hd[110];
    snprintf(hd, sizeof hd, "security about %.0f min  -  %d served today",
             est, pq_served(&a->qSecurity));
    tx_clipped(c, hd, x + 18.f, y + 31.f, w - 36.f, font_make(TF_UI, 11, TW_REG),
               C_INK_3, AL_L, AV_T);

    draw_queue_lane(a, x + 18.f, y + 52.f, w - 36.f, "SECURITY  -  FAST TRACK",
                    &a->qSecurity.priority, C_MAGENTA);
    draw_queue_lane(a, x + 18.f, y + 90.f, w - 36.f, "SECURITY  -  STANDARD",
                    &a->qSecurity.standard, C_V600);
    draw_queue_lane(a, x + 18.f, y + 128.f, w - 36.f, "BOARDING GATE",
                    &a->qBoarding.standard, C_TEAL);

    /* ---- book somebody on ------------------------------------------------
     *  Laid out from the top rather than from the bottom edge: anchoring it
     *  to y + h put the caption straight through the boarding lane above. */
    ui_divider(x + 18.f, y + 172.f, w - 36.f);
    float ry = y + 202.f;

    /*  The flight the new passenger goes on is whichever is selected on the
     *  board, falling back to the next departure -- so the control never
     *  needs a picker of its own.                                          */
    Flight *target = a->selFlight ? flight_by_id(wo, a->selFlight) : NULL;
    if (!target || target->arrival || target->state == FS_CANCELLED) {
        target = NULL;
        int best = 1 << 30;
        for (int i = 0; i < wo->nFlights; i++) {
            Flight *f = &wo->flight[i];
            if (f->arrival || f->state == FS_CANCELLED) continue;
            if (f->estMin < (int)wo->clock) continue;
            if (f->estMin < best) { best = f->estMin; target = f; }
        }
    }

    float fw = w - 36.f - 116.f;
    ui_text_field(uid("cinname"), x + 18.f, ry, fw, 38.f, a->ciName,
                  (int)sizeof a->ciName, "Book a passenger by name", IC_USER);

    int can = (strlen(a->ciName) >= 3) && target != NULL;
    if (can) {
        if (ui_button(uid("cinbook"), x + 18.f + fw + 8.f, ry, 108.f, 38.f,
                      "Book", BTN_PRIMARY)) {
            int id = ops_pax_register(wo, a->ciName, target->id);
            if (id) {
                ops_pax_checkin(wo, id, 1);
                a->selPax = id;
                a->ciName[0] = 0;
                ui_focus_clear();
                Passenger *np = NULL;
                for (int i = 0; i < wo->nPax; i++)
                    if (wo->pax[i].id == id) { np = &wo->pax[i]; break; }
                char m[130];
                snprintf(m, sizeof m, "%s on %s, seat %s, reference %s",
                         np ? np->name : "", target->no,
                         np ? np->seat : "--", np ? np->pnr : "--");
                ui_toast(TOAST_OK, "Booked and checked in", m);
            } else {
                ui_toast(TOAST_ERR, "Not booked",
                         "That flight is full or has closed.");
            }
        }
    } else {
        tx_backdrop(C_SURF);
        cv_rrect(c, x + 18.f + fw + 8.f, ry, 108.f, 38.f, 9.f, C_SURF_2);
        tx_backdrop(C_SURF_2);
        tx_clipped(c, target ? "3+ letters" : "no flight",
                   x + 18.f + fw + 62.f, ry + 19.f, 100.f,
                   font_make(TF_UI, 10, TW_REG), C_INK_4, AL_C, AV_M);
    }

    tx_backdrop(C_SURF);
    if (target) {
        char t[110];
        char hm[8]; fmt_hhmm(target->estMin, hm);
        snprintf(t, sizeof t, "BOOK ONTO %s TO %s AT %s", target->no,
                 wo->airport[target->airport].city, hm);
        tx_clipped(c, t, x + 18.f, y + 182.f, w - 36.f,
                   font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    } else {
        tx_draw(c, "NO DEPARTURE LEFT TODAY", x + 18.f, y + 182.f,
                font_track(TF_UI, 9, TW_BOLD, 1), C_INK_4, AL_L, AV_T);
    }
}

static void draw_desks(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;

    ui_panel_dark(x, y, w, h, R_LG);
    cv_clip_push(c, x + 1.f, y + 1.f, w - 2.f, h - 2.f);

    tx_backdrop(C_NIGHT_2);
    tx_draw(c, "CHECK-IN HALL  -  24 POSITIONS", x + 18.f, y + 14.f,
            font_track(TF_UI, 10, TW_BOLD, 2), col_alpha(HEX(0xFFFFFF), .9f),
            AL_L, AV_T);

    int open = 0, q = 0;
    for (int i = 0; i < wo->nDesks; i++)
        if (wo->desk[i].open) { open++; q += wo->desk[i].queue; }
    char sub[80];
    snprintf(sub, sizeof sub, "%d open   -   %d waiting   -   click a desk to "
             "open or close it", open, q);
    tx_draw(c, sub, x + 18.f, y + 32.f, font_make(TF_UI, 11, TW_MED),
            col_alpha(C_V200, .7f), AL_L, AV_T);

    float dw = (w - 36.f) / wo->nDesks;
    float dy = y + 58.f;
    float deskH = 34.f;
    float qTop = dy + deskH + 8.f;
    float qSpace = h - (qTop - y) - 12.f;

    for (int i = 0; i < wo->nDesks; i++) {
        Desk *d = &wo->desk[i];
        float dx = x + 18.f + i*dw;
        uint64_t id = uidi("desk", i);
        int hov = ui_hit(dx, dy, dw - 3.f, deskH + qSpace);
        float e = ui_hover_f(id, hov);
        if (hov) ui_cursor(1);
        if (hov && a->in.pressed) {
            d->open = !d->open;
            world_log(wo, LG_INFO, "Desk %d %s", d->number,
                      d->open ? "opened" : "closed");
        }

        Color dc = d->open ? col_mix(C_V600, C_V500, e) : HEX(0x2A1651);
        cv_rrect(c, dx, dy, dw - 3.f, deskH, 5.f, dc);
        if (d->open) {
            float glow = 0.5f + 0.5f*sinf(anim_time()*2.f + i*0.6f);
            cv_rrect(c, dx, dy, dw - 3.f, 3.f, 1.5f,
                     col_alpha(C_V200, .5f + .4f*glow));
        }
        char nb[8]; snprintf(nb, sizeof nb, "%d", d->number);
        tx_backdrop(dc);
        tx_draw(c, nb, dx + (dw-3.f)*0.5f, dy + deskH*0.5f,
                font_make(TF_UI, 11, TW_BOLD),
                d->open ? HEX(0xFFFFFF) : col_alpha(C_V300, .55f), AL_C, AV_M);

        /* the queue, one figure per waiting passenger */
        int show = d->queue > 12 ? 12 : d->queue;
        for (int k = 0; k < show; k++) {
            float fy = qTop + 14.f + k * (qSpace / 13.f);
            if (fy > y + h - 8.f) break;
            figure(c, dx + (dw-3.f)*0.5f, fy, 13.f,
                   col_alpha(k == 0 ? C_GOLD : C_V300, .85f - k*0.045f),
                   anim_time()*2.2f + i*0.8f + k*0.5f);
        }
        if (d->queue > 12) {
            char more[12]; snprintf(more, sizeof more, "+%d", d->queue - 12);
            tx_backdrop(C_NIGHT_2);
            tx_draw(c, more, dx + (dw-3.f)*0.5f, y + h - 12.f,
                    font_make(TF_UI, 9, TW_BOLD), col_alpha(C_V200,.8f),
                    AL_C, AV_B);
        }
    }
    cv_clip_pop(c);
}

/* --------------------------------------------------------------------------
 *  boarding pass
 * ------------------------------------------------------------------------- */

static void boarding_pass(App *a, float x, float y, float w, Passenger *p)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    Flight *f = flight_by_id(wo, p->flight);
    if (!f) return;

    float h = 208.f;
    float appear = anim_to(uidi("bp", p->id), 1.f, 8.f);
    anim_set(uidi("bpx", p->id), 1.f);
    float lift = (1.f - ease_out_cubic(appear)) * 10.f;
    y += lift;

    cv_shadow(c, x, y + 8.f, w, h, R_LG, 22.f, RGBA(60,20,130,60));

    Color ac = wo->airline[f->airline].col;
    Paint g = paint_linear(x, y, x + w, y + h, C_V700, C_V900);
    cv_rrect_p(c, x, y, w, h, R_LG, &g);

    /* the stub, separated by a perforation */
    float stubX = x + w - 128.f;
    cv_rrect(c, stubX, y + 8.f, 120.f, h - 16.f, 10.f, col_alpha(HEX(0xFFFFFF), .08f));
    for (float dy2 = y + 16.f; dy2 < y + h - 16.f; dy2 += 9.f)
        cv_rect(c, stubX - 5.f, dy2, 2.f, 5.f, col_alpha(HEX(0xFFFFFF), .30f));
    cv_circle(c, stubX - 4.f, y, 7.f, C_SURF);
    cv_circle(c, stubX - 4.f, y + h, 7.f, C_SURF);

    tx_backdrop(C_V800);
    tx_draw(c, "BOARDING PASS", x + 22.f, y + 18.f,
            font_track(TF_UI, 9, TW_BOLD, 3), col_alpha(C_V200, .9f), AL_L, AV_T);
    tx_clipped(c, wo->airline[f->airline].name, x + 22.f, y + 34.f, 200.f,
               font_make(TF_UI, 12, TW_SEMI), HEX(0xFFFFFF), AL_L, AV_T);
    cv_rrect(c, x + w - 148.f, y + 16.f, 4.f, 18.f, 2.f, ac);

    tx_clipped(c, p->name, x + 22.f, y + 62.f, 260.f,
               font_track(TF_DISPLAY, 21, TW_BOLD, 0), HEX(0xFFFFFF), AL_L, AV_T);

    struct { const char *k; char v[24]; float x, y; } cells[6];
    int n = 0;
    snprintf(cells[n].v, 24, "MRU");  cells[n].k = "FROM"; cells[n].x = 22.f;  cells[n].y = 100.f; n++;
    snprintf(cells[n].v, 24, "%s", wo->airport[f->airport].iata);
    cells[n].k = "TO";   cells[n].x = 96.f;  cells[n].y = 100.f; n++;
    snprintf(cells[n].v, 24, "%s", f->no);
    cells[n].k = "FLIGHT"; cells[n].x = 170.f; cells[n].y = 100.f; n++;
    char hhmm[8]; fmt_hhmm(f->estMin, hhmm);
    snprintf(cells[n].v, 24, "%s", hhmm);
    cells[n].k = "DEPARTS"; cells[n].x = 22.f; cells[n].y = 150.f; n++;
    char gb[8]; ordinal_gate(f->gate, gb);
    snprintf(cells[n].v, 24, "%s", gb);
    cells[n].k = "GATE"; cells[n].x = 96.f; cells[n].y = 150.f; n++;
    snprintf(cells[n].v, 24, "%s", p->seat);
    cells[n].k = "SEAT"; cells[n].x = 170.f; cells[n].y = 150.f; n++;

    for (int i = 0; i < n; i++) {
        tx_draw(c, cells[i].k, x + cells[i].x, y + cells[i].y,
                font_track(TF_UI, 8, TW_BOLD, 1), col_alpha(C_V200, .75f),
                AL_L, AV_T);
        tx_draw(c, cells[i].v, x + cells[i].x, y + cells[i].y + 13.f,
                font_track(TF_DISPLAY, 19, TW_BOLD, 0), HEX(0xFFFFFF), AL_L, AV_T);
    }

    /* stub content */
    tx_backdrop(C_V800);
    tx_draw(c, p->pnr, stubX + 60.f, y + 30.f, font_track(TF_MONO, 15, TW_BOLD, 1),
            HEX(0xFFFFFF), AL_C, AV_T);
    tx_draw(c, p->seat, stubX + 60.f, y + 54.f, font_track(TF_DISPLAY, 24, TW_BOLD, 0),
            C_V200, AL_C, AV_T);

    /* a barcode built from the booking reference so it is stable per pass */
    uint32_t seed = 0x1234u;
    for (const char *q = p->pnr; *q; q++) seed = seed*31u + (unsigned char)*q;
    float bx = stubX + 14.f, bw = 92.f;
    for (float px = 0.f; px < bw; ) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        float thick = 1.2f + (float)(seed & 3);
        if ((seed >> 4) & 1)
            cv_rect(c, bx + px, y + h - 66.f, thick, 40.f,
                    col_alpha(HEX(0xFFFFFF), .92f));
        px += thick + 1.6f;
    }
    tx_draw(c, "SEQ 041", stubX + 60.f, y + h - 22.f,
            font_make(TF_UI, 9, TW_MED), col_alpha(C_V200, .8f), AL_C, AV_T);

    if (p->loyalty >= 2) {
        ui_badge(x + 22.f, y + h - 30.f,
                 p->loyalty == 3 ? "PLATINUM" : "GOLD", C_V900, C_GOLD);
    }
}

/* --------------------------------------------------------------------------
 *  cabin plan
 * ------------------------------------------------------------------------- */

static void seat_map(App *a, float x, float y, float w, float h, Flight *f,
                     Passenger *sel)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    int wide = wo->actype[f->acType].widebody;
    int cols = wide ? 9 : 6;
    int rows = wide ? 34 : 26;

    tx_backdrop(C_SURF);
    tx_draw(c, "CABIN", x, y, font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    char t[60];
    snprintf(t, sizeof t, "%s   %d seats", wo->actype[f->acType].name,
             wo->actype[f->acType].seats);
    tx_draw(c, t, x + w, y, font_make(TF_UI, 10, TW_MED), C_INK_3, AL_R, AV_T);

    float gy = y + 18.f, gh = h - 18.f;
    float cellW = w / (float)rows;
    float cellH = gh / (float)(cols + 1);
    if (cellH > 13.f) cellH = 13.f;
    float sz = (cellW < cellH ? cellW : cellH) - 1.6f;
    if (sz < 3.f) sz = 3.f;

    int selRow = 0; char selCol = 0;
    if (sel) { selRow = atoi(sel->seat); selCol = sel->seat[strlen(sel->seat)-1]; }
    const char *colLetters = "ABCDEFGHJK";

    float ox = x + (w - rows*cellW)*0.5f;
    float oy = gy + (gh - (cols+1)*cellH)*0.5f;

    for (int r = 0; r < rows; r++) {
        for (int cc = 0; cc < cols; cc++) {
            int aisle = wide ? (cc == 2 || cc == 6) : (cc == 2);
            float cy2 = oy + cc*cellH + (aisle ? cellH*0.35f : 0.f);
            float cx2 = ox + r*cellW;

            /* occupied is deterministic from the seat, so the plan is stable */
            uint32_t hsh = (uint32_t)(r*73856093u ^ cc*19349663u ^ (uint32_t)f->id*83492791u);
            int loadPct  = f->paxCap ? (100 * f->pax / f->paxCap) : 60;
            int occupied = (int)((hsh >> 7) % 100u) < loadPct;

            int isSel = (sel && (r + 1) == selRow && colLetters[cc] == selCol);
            Color col = isSel ? C_MAGENTA
                      : (occupied ? col_alpha(C_V400, .60f) : C_SURF_3);
            cv_rrect(c, cx2, cy2, sz, sz, sz*0.28f, col);
            if (isSel) {
                float pulse = 0.5f + 0.5f*sinf(anim_time()*4.f);
                cv_circle_line(c, cx2 + sz*0.5f, cy2 + sz*0.5f, sz*1.1f,
                               col_alpha(C_MAGENTA, .4f + .5f*pulse), 1.4f);
            }
        }
    }
    /* nose marker */
    cv_tri(c, ox - 10.f, oy + (cols*cellH)*0.5f - 6.f,
              ox - 10.f, oy + (cols*cellH)*0.5f + 6.f,
              ox - 2.f,  oy + (cols*cellH)*0.5f, C_INK_4);
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_checkin(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    float deskH = 254.f;
    float fullW = w - pad*2.f;
    float deskW = fullW * 0.62f;
    draw_desks(a, x + pad, y + pad, deskW, deskH);
    draw_queues_and_register(a, x + pad + deskW + 14.f, y + pad,
                             fullW - deskW - 14.f, deskH);

    float ly = y + pad + deskH + 14.f;
    float lh = h - (ly - y) - pad;
    float listW = 372.f;

    /* ---- roster ---------------------------------------------------------- */
    ui_card(x + pad, ly, listW, lh, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "PASSENGER ROSTER", x + pad + 18.f, ly + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    ui_text_field(uid("paxsearch"), x + pad + 16.f, ly + 38.f, listW - 32.f, 40.f,
                  a->searchPax, sizeof a->searchPax,
                  "Name, booking reference or flight", IC_SEARCH);

    char q[64];
    snprintf(q, sizeof q, "%s", a->searchPax);
    for (char *p = q; *p; p++) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32);

    int idx[MAX_PASSENGERS], n = 0;
    for (int i = 0; i < wo->nPax; i++) {
        Passenger *p = &wo->pax[i];
        if (q[0]) {
            char nm[40], fn[12];
            snprintf(nm, sizeof nm, "%s", p->name);
            for (char *s = nm; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32;
            Flight *f = flight_by_id(wo, p->flight);
            snprintf(fn, sizeof fn, "%s", f ? f->no : "");
            if (!strstr(nm, q) && !strstr(p->pnr, q) && !strstr(fn, q)) continue;
        }
        idx[n++] = i;
    }

    char cnt[40]; snprintf(cnt, sizeof cnt, "%d records", n);
    tx_draw(c, cnt, x + pad + listW - 18.f, ly + 16.f,
            font_make(TF_UI, 11, TW_SEMI), C_INK_3, AL_R, AV_T);

    float rowH = 58.f;
    float rly = ly + 88.f, rlh = lh - 100.f;
    float off = ui_scroll_begin(uid("paxscroll"), x + pad + 6.f, rly,
                                listW - 12.f, rlh, n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Passenger *p = &wo->pax[idx[i]];
        float ry = rly + i*rowH - off;
        if (ry + rowH < rly || ry > rly + rlh) continue;

        uint64_t id = uidi("paxrow", p->id);
        int hov = ui_hit(x + pad + 12.f, ry, listW - 24.f, rowH - 6.f);
        float e = ui_hover_f(id, hov);
        int sel = (a->selPax == p->id);
        if (hov) ui_cursor(1);
        if (hov && a->in.pressed) { a->selPax = p->id; a->selFlight = p->flight; }

        Color bg = col_mix(col_mix(C_SURF, C_V50, e), C_V100, sel ? 1.f : 0.f);
        cv_rrect(c, x + pad + 12.f, ry, listW - 24.f, rowH - 6.f, 10.f, bg);
        tx_backdrop(bg);

        cv_circle(c, x + pad + 34.f, ry + 26.f, 15.f, col_alpha(C_V400, .18f));
        char ini[4] = { p->name[0], 0, 0, 0 };
        tx_draw(c, ini, x + pad + 34.f, ry + 26.f, font_make(TF_UI, 14, TW_BOLD),
                C_V700, AL_C, AV_M);

        tx_clipped(c, p->name, x + pad + 56.f, ry + 9.f, listW - 130.f,
                   font_make(TF_UI, 13, TW_SEMI), C_INK, AL_L, AV_T);
        Flight *f = flight_by_id(wo, p->flight);
        char sub[64];
        snprintf(sub, sizeof sub, "%s  %s  seat %s", p->pnr, f ? f->no : "----",
                 p->seat);
        tx_clipped(c, sub, x + pad + 56.f, ry + 28.f, listW - 130.f,
                   font_make(TF_UI, 11, TW_MED), C_INK_3, AL_L, AV_T);

        if (p->loyalty >= 2)
            cv_circle(c, x + pad + listW - 36.f, ry + 26.f, 5.f, C_GOLD);
        if (p->bags) {
            char bg2[8]; snprintf(bg2, sizeof bg2, "%d", p->bags);
            icon_draw(c, IC_LUGGAGE, x + pad + listW - 58.f, ry + 26.f, 14.f,
                      C_INK_4);
            tx_draw(c, bg2, x + pad + listW - 48.f, ry + 32.f,
                    font_make(TF_UI, 9, TW_BOLD), C_INK_3, AL_L, AV_T);
        }
    }
    ui_scroll_end();

    /* ---- detail ---------------------------------------------------------- */
    float dx = x + pad + listW + 14.f;
    float dw = w - pad*2.f - listW - 14.f;

    Passenger *sp = NULL;
    for (int i = 0; i < wo->nPax; i++)
        if (wo->pax[i].id == a->selPax) { sp = &wo->pax[i]; break; }

    ui_card(dx, ly, dw, lh, R_LG);
    if (!sp) {
        icon_draw(c, IC_TICKET, dx + dw*0.5f, ly + lh*0.5f - 20.f, 34.f, C_INK_4);
        tx_backdrop(C_SURF);
        tx_draw(c, "Select a passenger to issue a boarding pass",
                dx + dw*0.5f, ly + lh*0.5f + 12.f, font_make(TF_UI, 13, TW_MED),
                C_INK_3, AL_C, AV_M);
        return;
    }

    Flight *f = flight_by_id(wo, sp->flight);
    if (!f) return;

    boarding_pass(a, dx + 24.f, ly + 24.f, dw - 48.f > 520.f ? 520.f : dw - 48.f, sp);

    float iy = ly + 250.f;
    tx_backdrop(C_SURF);

    struct { const char *k; char v[52]; } rows[6];
    int nr = 0;
    snprintf(rows[nr].v, 52, "%s (%s)", wo->airport[f->airport].city,
             wo->airport[f->airport].iata); rows[nr++].k = "Destination";
    snprintf(rows[nr].v, 52, "%s", wo->actype[f->acType].name); rows[nr++].k = "Aircraft";
    snprintf(rows[nr].v, 52, "%d checked", sp->bags); rows[nr++].k = "Bags";
    snprintf(rows[nr].v, 52, "%s", sp->checkedIn ? "Checked in" : "Not checked in");
    rows[nr++].k = "Status";
    snprintf(rows[nr].v, 52, "%s%s%s",
             sp->fastTrack ? "Fast track  " : "",
             sp->wheelchair ? "Wheelchair  " : "",
             sp->infant ? "Infant" : (!sp->fastTrack && !sp->wheelchair ? "None" : ""));
    rows[nr++].k = "Assistance";

    float cw2 = (dw - 48.f) * 0.5f;
    for (int i = 0; i < nr; i++) {
        float rx = dx + 24.f + (i % 2) * cw2;
        float ry = iy + (i / 2) * 42.f;
        tx_draw(c, rows[i].k, rx, ry, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        tx_clipped(c, rows[i].v, rx, ry + 15.f, cw2 - 20.f,
                   font_make(TF_UI, 13, TW_SEMI), C_INK, AL_L, AV_T);
    }

    float smy = iy + 3*42.f + 8.f;
    float smh = ly + lh - smy - 74.f;
    if (smh > 60.f) seat_map(a, dx + 24.f, smy, dw - 48.f, smh, f, sp);

    /* actions */
    float by = ly + lh - 58.f;
    float bw = (dw - 48.f - 20.f)/3.f;
    if (ui_button_i(uid("ckin"), dx + 24.f, by, bw, 42.f,
                    sp->checkedIn ? "Checked in" : "Check in", IC_CHECK,
                    sp->checkedIn ? BTN_SOFT : BTN_PRIMARY)) {
        if (!sp->checkedIn) {
            sp->checkedIn = 1;
            f->checkedIn++;
            ui_toast(TOAST_OK, "Passenger accepted", sp->name);
            world_log(wo, LG_OK, "%s checked in for %s", sp->name, f->no);
        }
    }
    if (ui_button_i(uid("ckbags"), dx + 24.f + bw + 10.f, by, bw, 42.f,
                    "Trace bags", IC_LUGGAGE, BTN_OUTLINE)) {
        for (int i = 0; i < wo->nBags; i++)
            if (wo->bag[i].pax == sp->id) { a->selBag = wo->bag[i].id; break; }
        a->screen = SC_BAGGAGE;
    }
    if (ui_button_i(uid("ckask"), dx + 24.f + (bw + 10.f)*2.f, by, bw, 42.f,
                    "Ask AURA", IC_SPARK, BTN_OUTLINE)) {
        char qq[80]; snprintf(qq, sizeof qq, "passenger %s", sp->pnr);
        ai_chat_send(wo, &a->chat, qq);
        a->chatDock = 1;
    }
}
