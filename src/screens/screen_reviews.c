/* ==========================================================================
 *  AURA :: screen_reviews.c   --   what passengers say
 *
 *  Every rating is tied to one service rather than to the airport in general,
 *  because "three stars" tells a duty manager nothing and "three stars for
 *  security, five for the lounge" tells them where to put a person.  The
 *  summary column ranks the services by score so the worst one is impossible
 *  to miss.
 *
 *  Anybody signed in can leave a review, and it appears immediately in the
 *  carousel with the averages recomputed.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static Color score_colour(float avg)
{
    if (avg <= 0.f)   return C_INK_4;
    if (avg >= 4.2f)  return C_OK;
    if (avg >= 3.4f)  return C_TEAL;
    if (avg >= 2.6f)  return C_WARN;
    return C_DANGER;
}

/* --------------------------------------------------------------------------
 *  the summary column: every service, worst first
 * ------------------------------------------------------------------------- */

static void panel_summary(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ReviewBook *B = &a->reviews;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);

    float overall = rv_average(B, RV_SERVICE_COUNT);
    tx_draw(c, "PASSENGER RATING", x + 20.f, y + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    char big[12];
    snprintf(big, sizeof big, "%.1f", overall);
    tx_draw(c, big, x + 20.f, y + 36.f, font_track(TF_DISPLAY, 44, TW_BOLD, 0),
            C_INK, AL_L, AV_T);
    draw_stars(c, x + 96.f, y + 52.f, 17.f, overall, 5, C_GOLD,
               col_alpha(C_LINE_2, .9f));
    char cnt[50];
    snprintf(cnt, sizeof cnt, "%d review%s", B->n, B->n == 1 ? "" : "s");
    tx_draw(c, cnt, x + 96.f, y + 74.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_L, AV_T);

    /* the five-bar distribution */
    int dist[5];
    int total = rv_distribution(B, RV_SERVICE_COUNT, dist);
    float dy = y + 100.f;
    for (int s = 5; s >= 1; s--) {
        float ry = dy + (5 - s)*17.f;
        char lab[4]; snprintf(lab, sizeof lab, "%d", s);
        tx_draw(c, lab, x + 22.f, ry + 6.f, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_L, AV_T);
        float frac = total ? (float)dist[s-1] / (float)total : 0.f;
        cv_rrect(c, x + 34.f, ry + 4.f, w - 78.f, 7.f, 3.5f,
                 col_alpha(C_LINE_2, .7f));
        if (frac > 0.f)
            cv_rrect(c, x + 34.f, ry + 4.f, (w - 78.f)*frac, 7.f, 3.5f, C_GOLD);
        char n[8]; snprintf(n, sizeof n, "%d", dist[s-1]);
        tx_draw(c, n, x + w - 20.f, ry + 6.f, font_make(TF_UI, 10, TW_MED),
                C_INK_4, AL_R, AV_T);
    }

    ui_divider(x + 20.f, dy + 96.f, w - 40.f);
    tx_draw(c, "BY SERVICE", x + 20.f, dy + 108.f,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);

    /*  Ordered worst first.  A list in declaration order buries the problem;
     *  the point of this panel is to make the weakest service the first
     *  thing on it.                                                        */
    int order[RV_SERVICE_COUNT], n = 0;
    for (int s = 0; s < RV_SERVICE_COUNT; s++)
        if (rv_count(B, (ReviewService)s) > 0) order[n++] = s;
    for (int i = 1; i < n; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && rv_average(B, (ReviewService)order[j]) >
                         rv_average(B, (ReviewService)k)) {
            order[j+1] = order[j]; j--;
        }
        order[j+1] = k;
    }

    float ry = dy + 128.f;
    float rowH = 34.f;
    for (int i = 0; i < n; i++) {
        if (ry + rowH > y + h - 8.f) break;
        ReviewService s = (ReviewService)order[i];
        float avg = rv_average(B, s);
        Color sc  = score_colour(avg);
        int sel   = (a->rvService == (int)s);

        if (ui_hit(x + 14.f, ry, w - 28.f, rowH - 4.f)) {
            ui_cursor(1);
            if (a->in.pressed) {
                a->rvService = (int)s;
                a->rvIndex   = 0;
            }
        }
        if (sel) cv_rrect(c, x + 14.f, ry, w - 28.f, rowH - 4.f, 8.f,
                          col_alpha(C_V500, .10f));
        tx_backdrop(sel ? C_SURF : C_SURF);
        cv_rrect(c, x + 20.f, ry + 8.f, 3.f, rowH - 20.f, 1.5f, sc);
        tx_clipped(c, rv_service_name(s), x + 30.f, ry + 8.f, w - 118.f,
                   font_make(TF_UI, 12, sel ? TW_SEMI : TW_MED),
                   sel ? C_INK : C_INK_2, AL_L, AV_T);
        char av[10]; snprintf(av, sizeof av, "%.1f", avg);
        tx_draw(c, av, x + w - 52.f, ry + rowH*0.5f - 2.f,
                font_make(TF_UI, 12, TW_BOLD), sc, AL_R, AV_M);
        char nn[10]; snprintf(nn, sizeof nn, "(%d)", rv_count(B, s));
        tx_draw(c, nn, x + w - 20.f, ry + rowH*0.5f - 2.f,
                font_make(TF_UI, 10, TW_MED), C_INK_4, AL_R, AV_M);
        ry += rowH;
    }
}

/* --------------------------------------------------------------------------
 *  one review card
 * ------------------------------------------------------------------------- */

static void review_card(App *a, float x, float y, float w, float h,
                        const Review *r)
{
    Canvas *c = &a->cv;

    cv_rrect(c, x, y, w, h, R_LG, C_SURF_2);
    if (r->ownReview)
        cv_rrect_line(c, x, y, w, h, R_LG, col_alpha(C_V500, .5f), 1.4f);

    tx_backdrop(C_SURF_2);
    draw_stars(c, x + 20.f, y + 18.f, 15.f, (float)r->stars, 5,
               C_V600, col_alpha(C_LINE_2, .9f));

    ui_badge(x + w - 128.f, y + 18.f, rv_service_name(r->service),
             C_V700, col_alpha(C_V400, .18f));

    tx_para(c, r->text, x + 20.f, y + 46.f, w - 40.f,
            font_make(TF_UI, 12, TW_REG), C_INK, 6);

    tx_draw(c, r->author, x + 20.f, y + h - 44.f,
            font_make(TF_UI, 12, TW_SEMI), C_INK, AL_L, AV_T);
    char d[40];
    snprintf(d, sizeof d, "%d %.3s %d", r->day, month_name(r->month), r->year);
    tx_draw(c, d, x + 20.f, y + h - 26.f, font_make(TF_UI, 10, TW_REG),
            C_INK_3, AL_L, AV_T);

    if (r->verified) {
        icon_draw(c, IC_CHECK, x + w - 96.f, y + h - 32.f, 12.f, C_OK);
        tx_draw(c, "Verified", x + w - 86.f, y + h - 38.f,
                font_make(TF_UI, 10, TW_MED), C_OK, AL_L, AV_T);
    }
    if (r->ownReview)
        tx_draw(c, "Yours", x + w - 20.f, y + h - 38.f,
                font_make(TF_UI, 10, TW_SEMI), C_V600, AL_R, AV_T);
    (void)a;
}

/* --------------------------------------------------------------------------
 *  the carousel
 * ------------------------------------------------------------------------- */

static void panel_carousel(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    ReviewBook *B = &a->reviews;

    tx_backdrop(C_BG);
    tx_draw(c, "What our passengers say", x, y + 4.f,
            font_track(TF_DISPLAY, 26, TW_BOLD, 0), C_INK, AL_L, AV_T);
    char sub[120];
    snprintf(sub, sizeof sub, "%s  -  %s",
             rv_service_name((ReviewService)a->rvService),
             rv_service_blurb((ReviewService)a->rvService));
    tx_clipped(c, sub, x, y + 36.f, w - 120.f, font_make(TF_UI, 12, TW_MED),
               C_INK_3, AL_L, AV_T);

    int idx[MAX_REVIEWS];
    int n = rv_filter(B, (ReviewService)a->rvService, idx, MAX_REVIEWS);

    /* two cards at a time, like the site the layout is borrowed from */
    int perPage = (w > 700.f) ? 2 : 1;
    int pages   = n ? (n + perPage - 1) / perPage : 1;
    if (a->rvIndex >= pages) a->rvIndex = 0;
    if (a->rvIndex < 0)      a->rvIndex = pages - 1;

    if (ui_icon_btn(uid("rvprev"), x + w - 92.f, y + 6.f, 38.f, IC_CHEV_L,
                    BTN_OUTLINE)) a->rvIndex--;
    if (ui_icon_btn(uid("rvnext"), x + w - 46.f, y + 6.f, 38.f, IC_CHEV_R,
                    BTN_OUTLINE)) a->rvIndex++;

    float cy = y + 64.f;
    float ch = h - 118.f;
    if (ch > 260.f) ch = 260.f;
    float cw = (w - (perPage - 1)*16.f) / (float)perPage;

    if (!n) {
        ui_card(x, cy, w, ch, R_LG);
        tx_backdrop(C_SURF);
        tx_draw(c, "No reviews for this service yet", x + w*0.5f, cy + ch*0.5f,
                font_make(TF_UI, 13, TW_MED), C_INK_3, AL_C, AV_M);
        tx_draw(c, "Be the first -- the form is on the right",
                x + w*0.5f, cy + ch*0.5f + 22.f, font_make(TF_UI, 11, TW_REG),
                C_INK_4, AL_C, AV_M);
    } else {
        for (int k = 0; k < perPage; k++) {
            int i = a->rvIndex*perPage + k;
            if (i >= n) break;
            review_card(a, x + k*(cw + 16.f), cy, cw, ch, &B->r[idx[i]]);
        }
    }

    /* page dots */
    float dy = cy + ch + 14.f;
    float dw = (float)pages * 14.f;
    for (int p = 0; p < pages && pages > 1; p++) {
        int on = (p == a->rvIndex);
        if (ui_hit(x + w*0.5f - dw*0.5f + p*14.f - 5.f, dy - 5.f, 14.f, 14.f)) {
            ui_cursor(1);
            if (a->in.pressed) a->rvIndex = p;
        }
        cv_circle(c, x + w*0.5f - dw*0.5f + p*14.f + 2.f, dy + 2.f,
                  on ? 4.f : 3.f, on ? C_V600 : col_alpha(C_INK_4, .6f));
    }

    /*  With one card at a time the panel would otherwise be two thirds
     *  empty, so the rest of the height goes to the full list.            */
    if (n > perPage) {
        float by = dy + 18.f;
        if (by + 40.f < y + h) {
            char lab[50];
            snprintf(lab, sizeof lab, a->rvShowAll ? "Hide the full list"
                                                   : "Show all %d reviews", n);
            if (ui_button_i(uid("rvall"), x + w*0.5f - 100.f, by, 200.f, 36.f,
                            lab, IC_CHAT, BTN_SOFT))
                a->rvShowAll = !a->rvShowAll;
            by += 44.f;

            if (a->rvShowAll && by + 60.f < y + h) {
                float lh = y + h - by;
                ui_card(x, by, w, lh, R_LG);
                float rowH = 74.f;
                float off = ui_scroll_begin(uid("rvlist"), x + 6.f, by + 6.f,
                                            w - 12.f, lh - 12.f,
                                            n*rowH + 8.f);
                for (int i = 0; i < n; i++) {
                    const Review *r = &B->r[idx[i]];
                    float ry = by + 10.f + i*rowH - off;
                    if (ry + rowH < by || ry > by + lh) continue;
                    tx_backdrop(C_SURF);
                    draw_stars(c, x + 18.f, ry, 12.f, (float)r->stars, 5,
                               C_V600, col_alpha(C_LINE_2, .9f));
                    tx_draw(c, r->author, x + 106.f, ry,
                            font_make(TF_UI, 11, TW_SEMI), C_INK, AL_L, AV_T);
                    char d[36];
                    snprintf(d, sizeof d, "%d %.3s", r->day,
                             month_name(r->month));
                    tx_draw(c, d, x + w - 18.f, ry,
                            font_make(TF_UI, 10, TW_REG), C_INK_4, AL_R, AV_T);
                    tx_para(c, r->text, x + 18.f, ry + 18.f, w - 36.f,
                            font_make(TF_UI, 11, TW_REG), C_INK_2, 3);
                }
                ui_scroll_end();
            }
        }
    }
}

/* --------------------------------------------------------------------------
 *  leave one
 * ------------------------------------------------------------------------- */

static void panel_write(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World  *wo = &a->w;

    ui_card(x, y, w, h, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "LEAVE A REVIEW", x + 18.f, y + 15.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char who[90];
    snprintf(who, sizeof who, "posting as %s",
             a->auth.name[0] ? a->auth.name : "a guest");
    tx_clipped(c, who, x + 18.f, y + 32.f, w - 36.f,
               font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);

    float ry = y + 54.f;
    tx_draw(c, "SERVICE", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    float cw = (w - 36.f - 12.f) / 3.f;
    for (int s = 0; s < RV_SERVICE_COUNT; s++) {
        float bx = x + 18.f + (s % 3)*(cw + 6.f);
        float by = ry + (s / 3)*34.f;
        if (ui_chip(uidi("rvsvc", s), bx, by, 30.f, rv_service_name((ReviewService)s),
                    a->rvService == s)) {
            a->rvService = s;
            a->rvIndex   = 0;
        }
    }
    ry += 3*34.f + 12.f;

    tx_draw(c, "YOUR RATING", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    if (a->rvStars < 1) a->rvStars = 5;
    for (int s = 1; s <= 5; s++) {
        float sx = x + 18.f + (float)(s-1)*38.f;
        int hov = ui_hit(sx, ry, 34.f, 34.f);
        if (hov) { ui_cursor(1); if (a->in.pressed) a->rvStars = s; }
        int lit = (s <= a->rvStars);
        icon_draw(c, IC_STAR, sx + 16.f, ry + 16.f, hov ? 30.f : 27.f,
                  lit ? C_GOLD : col_alpha(C_LINE_2, .95f));
    }
    static const char *WORD[6] = { "", "Poor", "Not great", "Fine",
                                   "Good", "Excellent" };
    tx_draw(c, WORD[a->rvStars], x + 18.f + 5*38.f + 6.f, ry + 17.f,
            font_make(TF_UI, 12, TW_SEMI), C_INK_2, AL_L, AV_M);
    ry += 46.f;

    tx_draw(c, "WHAT HAPPENED", x + 18.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ry += 18.f;
    ui_text_field(uid("rvtext"), x + 18.f, ry, w - 36.f, 44.f, a->rvText,
                  (int)sizeof a->rvText, "Tell other passengers about it",
                  IC_EDIT);
    ry += 52.f;

    /* the text wraps into a preview so long entries are not typed blind */
    if (a->rvText[0]) {
        float ph = y + h - ry - 62.f;
        if (ph > 20.f) {
            cv_rrect(c, x + 18.f, ry, w - 36.f, ph, 9.f, C_SURF_2);
            tx_backdrop(C_SURF_2);
            tx_para(c, a->rvText, x + 28.f, ry + 8.f, w - 56.f,
                    font_make(TF_UI, 11, TW_REG), C_INK_2,
                    (int)(ph / 17.f));
            ry += ph + 8.f;
        }
    }

    float by = y + h - 54.f;
    int len = (int)strlen(a->rvText);
    if (len < 12) {
        tx_backdrop(C_SURF);
        char need[70];
        snprintf(need, sizeof need,
                 "A few more words, please -- %d of 12 so far.", len);
        tx_draw(c, need, x + 18.f, by + 14.f, font_make(TF_UI, 11, TW_MED),
                C_INK_4, AL_L, AV_T);
    } else if (ui_button_i(uid("rvpost"), x + 18.f, by, w - 36.f, 42.f,
                           "Post this review", IC_SEND, BTN_PRIMARY)) {
        /*  Verified means the account holder actually has a booking today.
         *  Claiming it for everybody would make the badge meaningless.    */
        int verified = 0;
        for (int i = 0; i < wo->nPax; i++)
            if (a->auth.name[0] && strcmp(wo->pax[i].name, a->auth.name) == 0) {
                verified = 1; break;
            }
        rv_add(&a->reviews, wo, (ReviewService)a->rvService, a->rvStars,
               a->auth.name[0] ? a->auth.name : "Guest", a->rvText,
               verified, 1);
        a->rvText[0] = 0;
        a->rvIndex   = 0;
        ui_focus_clear();
        ui_toast(TOAST_OK, "Thank you", "Your review is now on the board.");
    }
}

/* ==========================================================================
 *  the screen
 * ========================================================================== */

void screen_reviews(App *a, float x, float y, float w, float h)
{
    float pad = PAD;
    float bx = x + pad, by = y + pad;
    float bw = w - pad*2.f, bh = h - pad*2.f;

    float sumW = 280.f;
    float wrW  = 330.f;
    if (bw < 1000.f) { sumW = 240.f; wrW = 280.f; }
    float carW = bw - sumW - wrW - 32.f;

    panel_summary (a, bx, by, sumW, bh);
    panel_carousel(a, bx + sumW + 16.f, by, carW, bh);
    panel_write   (a, bx + sumW + carW + 32.f, by, wrW, bh);
}
