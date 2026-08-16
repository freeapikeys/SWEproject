/* ==========================================================================
 *  AURA :: screen_ai.c   --   the AI suite
 *
 *  Five engines, each with its own view: the assistant, the delay regression,
 *  the baggage classifier, the stand allocator and the flow forecaster.
 *  Every panel shows the model's real internals -- weights, loss curves,
 *  activations, convergence -- rather than a headline number, because a
 *  controller who cannot see why a model said something will not use it.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ==========================================================================
 *  the assistant
 * ========================================================================== */

void draw_chat_panel(App *a, float x, float y, float w, float h, int compact)
{
    Canvas *c = &a->cv;
    ChatSession *s = &a->chat;

    /* header */
    float hh = 62.f;
    Paint hg = paint_linear(x, y, x + w, y + hh, C_V600, C_MAGENTA);
    {
        Path p; path_reset(&p);
        path_rrect4(&p, x, y, w, hh, R_XL, R_XL, 0.f, 0.f);
        cv_fill(c, &p, &hg, 1.f);
    }
    float pulse = 0.5f + 0.5f*sinf(anim_time()*2.f);
    cv_circle(c, x + 34.f, y + 31.f, 16.f, col_alpha(HEX(0xFFFFFF), .16f + .08f*pulse));
    icon_draw(c, IC_SPARK, x + 34.f, y + 31.f, 18.f, HEX(0xFFFFFF));
    tx_backdrop(C_V600);
    tx_draw(c, "AURA Assistant", x + 58.f, y + 15.f, font_make(TF_UI, 15, TW_SEMI),
            HEX(0xFFFFFF), AL_L, AV_T);
    tx_draw_a(c, s->thinking ? "thinking..." : "reading live operations",
              x + 58.f, y + 35.f, font_make(TF_UI, 11, TW_REG), HEX(0xFFFFFF),
              AL_L, AV_T, .8f);
    if (compact && ui_icon_btn(uid("chatclose"), x + w - 44.f, y + 14.f, 34.f,
                               IC_CLOSE, BTN_GHOST))
        a->chatDock = 0;

    /* transcript */
    float inputH = 56.f;
    float listY = y + hh + 10.f;
    float listH = h - hh - inputH - 24.f;

    /* measure */
    float total = 0.f;
    Font bodyF = font_make(TF_UI, 13, TW_REG);
    float bubbleW = w - 60.f;
    for (int i = 0; i < s->n; i++) {
        ChatTurn *t = &s->turn[i];
        float th = (float)tx_para_h(c, t->text, bubbleW - 28.f, bodyF, 3) + 26.f;
        if (!t->fromUser && t->chips) th += 34.f;
        total += th + 12.f;
    }
    if (s->thinking) total += 48.f;

    float off = ui_scroll_begin(uid("chatscroll"), x + 6.f, listY, w - 12.f,
                                listH, total);
    /* keep the newest message in view */
    float *pinned = ui_state(uid("chatpin"), 0.f);
    if (*pinned != total) {
        *pinned = total;
        float *so = ui_state(uid("chatscroll"), 0.f);
        *so = total - listH > 0.f ? total - listH : 0.f;
    }

    float cy = listY - off;
    for (int i = 0; i < s->n; i++) {
        ChatTurn *t = &s->turn[i];
        float th = (float)tx_para_h(c, t->text, bubbleW - 28.f, bodyF, 3) + 26.f;
        float chipH = (!t->fromUser && t->chips) ? 34.f : 0.f;

        float appear = ease_out_cubic(cv_clampf(t->t / 0.32f, 0.f, 1.f));
        float slide = (1.f - appear) * 12.f;

        if (cy + th + chipH > listY - 40.f && cy < listY + listH + 40.f) {
            if (t->fromUser) {
                float bw = bubbleW * 0.86f;
                float bx = x + w - bw - 16.f;
                cv_shadow(c, bx, cy + 3.f + slide, bw, th, 13.f, 10.f,
                          RGBA(80,40,160,40));
                Paint ug = paint_linear(bx, cy, bx + bw, cy + th, C_V600, C_V700);
                {
                    Path p; path_reset(&p);
                    path_rrect4(&p, bx, cy + slide, bw, th, 13.f, 13.f, 4.f, 13.f);
                    cv_fill(c, &p, &ug, appear);
                }
                tx_backdrop(C_V600);
                tx_para(c, t->text, bx + 14.f, cy + 13.f + slide, bw - 28.f,
                        bodyF, HEX(0xFFFFFF), 3);
            } else {
                float bw = bubbleW;
                float bx = x + 16.f;
                {
                    Path p; path_reset(&p);
                    path_rrect4(&p, bx, cy + slide, bw, th, 13.f, 13.f, 13.f, 4.f);
                    cv_fill_col(c, &p, C_SURF_2);
                }
                cv_rrect_line(c, bx, cy + slide, bw, th, 13.f, C_LINE, 1.f);
                tx_backdrop(C_SURF_2);
                tx_para(c, t->text, bx + 14.f, cy + 13.f + slide, bw - 28.f,
                        bodyF, C_INK, 3);

                if (t->intent >= 0 && t->confidence > 0.f) {
                    char m[64];
                    snprintf(m, sizeof m, "%s  %.0f%%", ai_intent_name(t->intent),
                             t->confidence * 100.f);
                    tx_draw_a(c, m, bx + bw - 12.f, cy + th - 15.f,
                              font_make(TF_UI, 9, TW_MED), C_INK_4, AL_R, AV_T, .9f);
                }
                if (t->chips) {
                    float chx = bx;
                    for (int k = 0; k < t->chips; k++) {
                        Font cf = font_make(TF_UI, 11, TW_SEMI);
                        float cwid = tx_width(c, t->chip[k], cf) + 22.f;
                        if (ui_chip(uidii("chip", i, k), chx, cy + th + 6.f + slide,
                                    26.f, t->chip[k], 0)) {
                            ai_chat_send(&a->w, &a->chat, t->chip[k]);
                        }
                        chx += cwid + 6.f;
                    }
                }
            }
        }
        cy += th + chipH + 12.f;
    }

    if (s->thinking) {
        float bx = x + 16.f;
        cv_rrect(c, bx, cy, 74.f, 36.f, 13.f, C_SURF_2);
        for (int i = 0; i < 3; i++) {
            float ph = anim_time()*4.f - i*0.5f;
            float bump = sinf(ph);
            if (bump < 0.f) bump = 0.f;
            cv_circle(c, bx + 22.f + i*15.f, cy + 18.f - bump*4.f, 4.f,
                      col_mix(C_INK_4, C_V600, bump));
        }
    }
    ui_scroll_end();

    /* composer */
    float iy = y + h - inputH - 8.f;
    int send = 0;
    if (ui_text_field(uid("chatfield"), x + 14.f, iy, w - 76.f, 44.f,
                      a->chatInput, sizeof a->chatInput,
                      "Ask about a flight, a bag, the weather...", 0))
        { }
    if (ui_field_focused(uid("chatfield"))) {
        for (int i = 0; i < a->in.nchars; i++)
            if (a->in.chars[i] == 13) send = 1;
    }
    if (ui_icon_btn(uid("chatsend"), x + w - 56.f, iy + 2.f, 40.f, IC_SEND,
                    BTN_PRIMARY)) send = 1;

    if (send && a->chatInput[0]) {
        ai_chat_send(&a->w, &a->chat, a->chatInput);
        a->chatInput[0] = 0;
        *ui_state(uid("chatfield") ^ 0x22ULL, 0.f) = 0.f;
    }
}

/* ==========================================================================
 *  shared chart helpers
 * ========================================================================== */

static void loss_chart(Canvas *c, float x, float y, float w, float h,
                       const float *v, int n, const char *title, Color col)
{
    tx_backdrop(C_SURF);
    tx_draw(c, title, x, y, font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    float cy = y + 16.f, ch = h - 16.f;
    cv_rrect(c, x, cy, w, ch, 8.f, C_SURF_2);
    for (int i = 1; i < 4; i++)
        cv_line(c, x + 6.f, cy + ch*i/4.f, x + w - 6.f, cy + ch*i/4.f,
                col_alpha(C_LINE_2, .6f), 1.f);
    if (n > 1) draw_spark(c, x + 8.f, cy + 8.f, w - 16.f, ch - 16.f, v, n, col, 1);
}

static void weight_bars(App *a, Canvas *c, float x, float y, float w,
                        const char **names, const float *vals, int n,
                        const char *title)
{
    tx_backdrop(C_SURF);
    tx_draw(c, title, x, y, font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    float ry = y + 20.f;
    float maxv = 0.001f;
    for (int i = 0; i < n; i++) if (fabsf(vals[i]) > maxv) maxv = fabsf(vals[i]);

    for (int i = 0; i < n; i++) {
        float t = anim_to(uidi("wb", i), fabsf(vals[i]) / maxv, 7.f);
        Color col = vals[i] >= 0.f ? C_V600 : C_TEAL;
        tx_clipped(c, names[i], x, ry, w*0.46f, font_make(TF_UI, 11, TW_MED),
                   C_INK_2, AL_L, AV_T);
        float bx = x + w*0.48f, bw = w*0.38f;
        cv_rrect(c, bx, ry + 3.f, bw, 8.f, 4.f, C_SURF_3);
        cv_rrect(c, bx, ry + 3.f, bw*t, 8.f, 4.f, col);
        char v[16]; snprintf(v, sizeof v, "%+.2f", vals[i]);
        tx_draw(c, v, x + w, ry, font_track(TF_MONO, 11, TW_SEMI, 0),
                C_INK_3, AL_R, AV_T);
        ry += 21.f;
    }
    (void)a;
}

/* ==========================================================================
 *  delay oracle
 * ========================================================================== */

static void view_delay(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    DelayModel *m = &a->delay;
    float colW = (w - 16.f) * 0.44f;

    /* model card */
    ui_card(x, y, colW, h, R_LG);
    tx_backdrop(C_SURF);
    ui_section(x + 20.f, y + 18.f, "MODEL 02", "Delay Oracle");
    tx_para(c, "Logistic regression estimating the probability that a "
               "departure leaves fifteen minutes or more behind schedule. "
               "Trained by batch gradient descent with L2 regularisation.",
            x + 20.f, y + 62.f, colW - 40.f, font_make(TF_UI, 12, TW_REG),
            C_INK_2, 3);

    float sy = y + 128.f;
    float tw3 = (colW - 40.f - 16.f) / 3.f;
    char v[32];
    snprintf(v, sizeof v, "%.1f%%", m->accuracy*100.f);
    draw_stat_tile(c, x + 20.f, sy, tw3, 76.f, "ACCURACY", v, "", 0, C_OK);
    snprintf(v, sizeof v, "%d", m->samples);
    draw_stat_tile(c, x + 20.f + tw3 + 8.f, sy, tw3, 76.f, "SAMPLES", v, "", 0, C_V600);
    snprintf(v, sizeof v, "%.3f", m->loss);
    draw_stat_tile(c, x + 20.f + (tw3+8.f)*2.f, sy, tw3, 76.f, "LOG LOSS", v, "",
                   0, C_MAGENTA);

    loss_chart(c, x + 20.f, sy + 92.f, colW - 40.f, 130.f, m->lossCurve,
               m->nCurve, "TRAINING LOSS", C_V600);

    weight_bars(a, c, x + 20.f, sy + 238.f, colW - 40.f, DELAY_FEATURE_NAME,
                m->w, DELAY_FEATURES, "LEARNED FEATURE WEIGHTS");

    char ep[64];
    snprintf(ep, sizeof ep, "%d epochs  -  bias %+.2f", m->epochs, m->b);
    tx_draw(c, ep, x + 20.f, y + h - 30.f, font_make(TF_UI, 11, TW_MED),
            C_INK_3, AL_L, AV_T);

    /* ranked risk list */
    float rx = x + colW + 16.f, rw = w - colW - 16.f;
    ui_card(rx, y, rw, h, R_LG);
    tx_draw(c, "DEPARTURES RANKED BY RISK", rx + 20.f, y + 18.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    int idx[80], n = 0;
    for (int i = 0; i < wo->nFlights && n < 80; i++) {
        Flight *f = &wo->flight[i];
        if (f->arrival || f->state >= FS_DEPARTED) continue;
        idx[n++] = i;
    }
    for (int i = 1; i < n; i++) {
        int k = idx[i], j = i - 1;
        while (j >= 0 && wo->flight[idx[j]].delayRisk < wo->flight[k].delayRisk) {
            idx[j+1] = idx[j]; j--;
        }
        idx[j+1] = k;
    }

    float ly = y + 44.f, lh = h - 60.f;
    float rowH = 54.f;
    float off = ui_scroll_begin(uid("riskscroll"), rx + 8.f, ly, rw - 16.f, lh,
                                n*rowH + 8.f);
    for (int i = 0; i < n; i++) {
        Flight *f = &wo->flight[idx[i]];
        float ry = ly + i*rowH - off;
        if (ry + rowH < ly || ry > ly + lh) continue;
        draw_flight_row(a, rx + 14.f, ry, rw - 28.f, rowH - 6.f, f,
                        a->selFlight == f->id, 1);
    }
    ui_scroll_end();

    /* explanation for the selected flight */
    Flight *sf = a->selFlight ? flight_by_id(wo, a->selFlight) : NULL;
    if (sf && !sf->arrival) {
        float ex = rx + 14.f, ey = y + h - 176.f, ew = rw - 28.f;
        cv_rrect(c, ex, ey, ew, 162.f, 12.f, C_V50);
        cv_rrect_line(c, ex, ey, ew, 162.f, 12.f, C_V200, 1.f);
        tx_backdrop(C_V50);
        char t[80];
        snprintf(t, sizeof t, "WHY %s SCORES %.0f%%", sf->no, sf->delayRisk*100.f);
        tx_draw(c, t, ex + 14.f, ey + 12.f, font_track(TF_UI, 10, TW_BOLD, 1),
                C_V700, AL_L, AV_T);

        float contrib[DELAY_FEATURES];
        ai_delay_predict(&a->delay, wo, sf, contrib);
        float maxc = 0.01f;
        for (int i = 0; i < DELAY_FEATURES; i++)
            if (fabsf(contrib[i]) > maxc) maxc = fabsf(contrib[i]);
        float by = ey + 34.f;
        for (int i = 0; i < DELAY_FEATURES; i++) {
            if (by > ey + 148.f) break;
            float t2 = fabsf(contrib[i]) / maxc;
            tx_clipped(c, DELAY_FEATURE_NAME[i], ex + 14.f, by, ew*0.42f,
                       font_make(TF_UI, 10, TW_MED), C_INK_2, AL_L, AV_T);
            float bx = ex + ew*0.46f;
            cv_rrect(c, bx, by + 3.f, ew*0.42f, 7.f, 3.5f, C_SURF_3);
            cv_rrect(c, bx, by + 3.f, ew*0.42f*t2, 7.f, 3.5f,
                     contrib[i] >= 0.f ? C_DANGER : C_OK);
            by += 14.f;
        }
    }
}

/* ==========================================================================
 *  bagscan network
 * ========================================================================== */

static void view_bagscan(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    BagNet *n = a->bagnet;
    float colW = (w - 16.f) * 0.40f;

    ui_card(x, y, colW, h, R_LG);
    tx_backdrop(C_SURF);
    ui_section(x + 20.f, y + 18.f, "MODEL 03", "BagScan Neural");
    tx_para(c, "A 7-12-8-1 multilayer perceptron trained by backpropagation "
               "with momentum. It triages hold baggage: anything scoring above "
               "0.38 leaves the sortation loop for manual search.",
            x + 20.f, y + 62.f, colW - 40.f, font_make(TF_UI, 12, TW_REG),
            C_INK_2, 3);

    float sy = y + 132.f;
    float tw2 = (colW - 40.f - 8.f) / 2.f;
    char v[32];
    snprintf(v, sizeof v, "%.1f%%", n->accuracy*100.f);
    draw_stat_tile(c, x + 20.f, sy, tw2, 78.f, "ACCURACY", v, "", 0, C_OK);
    snprintf(v, sizeof v, "%.0f%%", n->recall*100.f);
    draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy, tw2, 78.f, "RECALL", v,
                   "tuned to favour recall", 0, C_MAGENTA);
    snprintf(v, sizeof v, "%.0f%%", n->precision*100.f);
    draw_stat_tile(c, x + 20.f, sy + 86.f, tw2, 78.f, "PRECISION", v, "", 0, C_V600);
    snprintf(v, sizeof v, "%d", n->samples);
    draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy + 86.f, tw2, 78.f, "TRAINING SET",
                   v, "synthetic scans", 0, C_TEAL);

    loss_chart(c, x + 20.f, sy + 178.f, colW - 40.f, 132.f, n->lossCurve,
               n->nCurve, "BACKPROPAGATION LOSS", C_MAGENTA);

    /* --- the network itself ---------------------------------------------- */
    float nx = x + colW + 16.f, nw = w - colW - 16.f;
    ui_card(nx, y, nw, h, R_LG);
    tx_draw(c, "LIVE NETWORK", nx + 20.f, y + 18.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    /* pick a bag to visualise: the selection, else one in the tunnel */
    Bag *b = NULL;
    for (int i = 0; i < wo->nBags; i++) if (wo->bag[i].id == a->selBag) b = &wo->bag[i];
    if (!b) for (int i = 0; i < wo->nBags; i++)
        if (wo->bag[i].state == BG_SCREEN) { b = &wo->bag[i]; break; }
    if (!b && wo->nBags) b = &wo->bag[(int)(anim_time()*0.4f) % wo->nBags];

    float feat[BAG_IN], h1[BAG_H1], h2[BAG_H2];
    float score = 0.f;
    if (b) {
        ai_bag_features(wo, b, feat);
        score = ai_bagnet_eval(n, feat, h1, h2);
    } else {
        memset(feat, 0, sizeof feat);
        memset(h1, 0, sizeof h1);
        memset(h2, 0, sizeof h2);
    }

    float gx = nx + 30.f, gy = y + 56.f, gw = nw - 60.f, gh = h - 190.f;
    float layerX[4] = { gx + 30.f, gx + gw*0.36f, gx + gw*0.68f, gx + gw - 20.f };
    int   counts[4] = { BAG_IN, BAG_H1, BAG_H2, 1 };
    float ny[4][BAG_H1];

    for (int L = 0; L < 4; L++)
        for (int i = 0; i < counts[L]; i++)
            ny[L][i] = gy + gh*(i + 0.5f)/counts[L];

    /* connections, brightness follows the weight magnitude */
    for (int i = 0; i < BAG_H1; i++)
        for (int j = 0; j < BAG_IN; j++) {
            float mag = cv_clampf(fabsf(n->w1[i][j])*0.55f, 0.f, 1.f);
            cv_line(c, layerX[0], ny[0][j], layerX[1], ny[1][i],
                    col_alpha(n->w1[i][j] >= 0.f ? C_V400 : C_TEAL, .05f + mag*.16f),
                    1.f);
        }
    for (int i = 0; i < BAG_H2; i++)
        for (int j = 0; j < BAG_H1; j++) {
            float mag = cv_clampf(fabsf(n->w2[i][j])*0.55f, 0.f, 1.f);
            cv_line(c, layerX[1], ny[1][j], layerX[2], ny[2][i],
                    col_alpha(n->w2[i][j] >= 0.f ? C_V400 : C_TEAL, .04f + mag*.14f),
                    1.f);
        }
    for (int j = 0; j < BAG_H2; j++) {
        float mag = cv_clampf(fabsf(n->w3[j])*0.5f, 0.f, 1.f);
        cv_line(c, layerX[2], ny[2][j], layerX[3], ny[3][0],
                col_alpha(n->w3[j] >= 0.f ? C_V400 : C_TEAL, .08f + mag*.30f), 1.4f);
    }

    /* neurons */
    tx_backdrop(C_SURF);
    for (int i = 0; i < BAG_IN; i++) {
        float act = cv_clampf(feat[i], 0.f, 1.f);
        cv_circle(c, layerX[0], ny[0][i], 9.f, col_mix(C_SURF_3, C_V600, act));
        cv_circle_line(c, layerX[0], ny[0][i], 9.f, C_LINE_2, 1.f);
        tx_clipped(c, BAG_FEATURE_NAME[i], layerX[0] - 16.f, ny[0][i] - 6.f,
                   120.f, font_make(TF_UI, 10, TW_MED), C_INK_3, AL_R, AV_T);
    }
    float maxh1 = 0.01f;
    for (int i = 0; i < BAG_H1; i++) if (h1[i] > maxh1) maxh1 = h1[i];
    for (int i = 0; i < BAG_H1; i++) {
        float act = cv_clampf(h1[i]/maxh1, 0.f, 1.f);
        cv_circle(c, layerX[1], ny[1][i], 8.f, col_mix(C_SURF_3, C_V500, act));
    }
    float maxh2 = 0.01f;
    for (int i = 0; i < BAG_H2; i++) if (h2[i] > maxh2) maxh2 = h2[i];
    for (int i = 0; i < BAG_H2; i++) {
        float act = cv_clampf(h2[i]/maxh2, 0.f, 1.f);
        cv_circle(c, layerX[2], ny[2][i], 8.f, col_mix(C_SURF_3, C_MAGENTA, act));
    }
    Color oc = score > BAG_THRESHOLD ? C_DANGER : C_OK;
    cv_glow(c, layerX[3], ny[3][0], 34.f, oc, .35f);
    cv_circle(c, layerX[3], ny[3][0], 20.f, oc);
    char sc[12]; snprintf(sc, sizeof sc, "%.2f", score);
    tx_backdrop(oc);
    tx_draw(c, sc, layerX[3], ny[3][0], font_make(TF_DISPLAY, 14, TW_BOLD),
            HEX(0xFFFFFF), AL_C, AV_M);

    const char *lay[4] = { "input", "hidden 1  (12)", "hidden 2  (8)", "output" };
    tx_backdrop(C_SURF);
    for (int L = 0; L < 4; L++)
        tx_draw(c, lay[L], layerX[L], gy + gh + 14.f,
                font_make(TF_UI, 10, TW_SEMI), C_INK_3, AL_C, AV_T);

    /* the bag under the beam */
    float by = y + h - 108.f;
    cv_rrect(c, nx + 20.f, by, nw - 40.f, 88.f, 12.f, C_SURF_2);
    if (b) {
        Flight *f = flight_by_id(wo, b->flight);
        tx_draw(c, b->tag, nx + 36.f, by + 14.f,
                font_track(TF_MONO, 16, TW_BOLD, 1), C_INK, AL_L, AV_T);
        char l[110];
        snprintf(l, sizeof l, "%.1f kg   %s   %s", b->weight,
                 f ? f->no : "----", bs_name(b->state));
        tx_draw(c, l, nx + 36.f, by + 38.f, font_make(TF_UI, 12, TW_MED),
                C_INK_2, AL_L, AV_T);
        tx_draw(c, score > BAG_THRESHOLD ? "DIVERT TO MANUAL SEARCH" : "CLEARED TO SORTATION",
                nx + 36.f, by + 60.f, font_make(TF_UI, 12, TW_SEMI), oc, AL_L, AV_T);
        if (ui_button(uid("nextbag"), nx + nw - 150.f, by + 26.f, 116.f, 38.f,
                      "Scan another", BTN_SOFT)) {
            for (int i = 0; i < wo->nBags; i++) {
                int k = (b->id + i) % wo->nBags;
                if (wo->bag[k].state <= BG_SORT) { a->selBag = wo->bag[k].id; break; }
            }
        }
    }
}

/* ==========================================================================
 *  stand allocator
 * ========================================================================== */

static void view_stand(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    StandPlan *p = &a->plan;
    float colW = (w - 16.f) * 0.36f;

    ui_card(x, y, colW, h, R_LG);
    tx_backdrop(C_SURF);
    ui_section(x + 20.f, y + 18.f, "MODEL 04", "Stand Allocator");
    tx_para(c, "Simulated annealing over the whole day's assignment. It "
               "always accepts an improvement and sometimes accepts a "
               "worsening move while the temperature is high, which is how "
               "it escapes the local optimum a greedy allocator settles for.",
            x + 20.f, y + 62.f, colW - 40.f, font_make(TF_UI, 12, TW_REG),
            C_INK_2, 3);

    float by = y + 156.f;
    if (ui_button_i(uid("runsa"), x + 20.f, by, colW - 40.f, 46.f,
                    "Run optimiser", IC_CPU, BTN_PRIMARY)) {
        ai_stand_optimise(p, wo, 9000);
        ui_toast(TOAST_OK, "Optimiser finished", "9,000 annealing iterations");
    }
    float bw2 = (colW - 40.f - 10.f) * 0.5f;
    if (ui_button_i(uid("scramble"), x + 20.f, by + 54.f, bw2, 40.f,
                    "Scramble", IC_REFRESH, BTN_OUTLINE)) {
        ai_stand_scramble(wo);
        p->ready = 0;
        ui_toast(TOAST_WARN, "Plan scrambled",
                 "Now run the optimiser and watch it recover.");
    }
    if (p->ready) {
        if (ui_button_i(uid("applysa"), x + 30.f + bw2, by + 54.f, bw2, 40.f,
                        "Apply", IC_CHECK, BTN_SUCCESS)) {
            ai_stand_apply(p, wo);
            ui_toast(TOAST_OK, "Plan applied", "Stands re-assigned across the day");
        }
    }

    if (p->ready) {
        float sy = by + 106.f;
        float tw2 = (colW - 40.f - 8.f)/2.f;
        char v[32];
        snprintf(v, sizeof v, "%.0f", p->cost);
        char impr[48];
        snprintf(impr, sizeof impr, "was %.0f  (-%.0f%%)", p->startCost,
                 p->startCost > 0.f ? 100.f*(p->startCost - p->cost)/p->startCost
                                    : 0.f);
        draw_stat_tile(c, x + 20.f, sy, tw2, 76.f, "COST", v, impr, 0, C_V600);
        snprintf(v, sizeof v, "%d", p->conflicts);
        draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy, tw2, 76.f, "CONFLICTS", v, "",
                       0, p->conflicts ? C_DANGER : C_OK);
        snprintf(v, sizeof v, "%d", p->contactUsed);
        draw_stat_tile(c, x + 20.f, sy + 84.f, tw2, 76.f, "ON CONTACT", v,
                       "airbridge served", 0, C_TEAL);
        snprintf(v, sizeof v, "%d", p->towMoves);
        draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy + 84.f, tw2, 76.f, "TOWS", v,
                       "aircraft moved", 0, C_WARN);

        loss_chart(c, x + 20.f, sy + 172.f, colW - 40.f, 128.f, p->bestCurve,
                   p->nCurve, "ANNEALING CONVERGENCE", C_V600);
    } else {
        tx_draw(c, "Run the optimiser to see convergence", x + 20.f, by + 118.f,
                font_make(TF_UI, 12, TW_MED), C_INK_3, AL_L, AV_T);
    }

    /* --- stand occupancy chart ------------------------------------------- */
    float cx = x + colW + 16.f, cw = w - colW - 16.f;
    ui_card(cx, y, cw, h, R_LG);
    tx_draw(c, "STAND OCCUPANCY  -  06:00 TO 24:00", cx + 20.f, y + 18.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);

    float gx = cx + 60.f, gy = y + 48.f;
    float gw = cw - 90.f, gh = h - 78.f;
    float rowH = gh / (float)wo->nStands;
    float t0 = 300.f, t1 = 1440.f;

    for (int hh = 6; hh <= 24; hh += 2) {
        float px = gx + gw * ((hh*60.f) - t0) / (t1 - t0);
        cv_line(c, px, gy, px, gy + gh, col_alpha(C_LINE_2, .7f), 1.f);
        char lb[8]; snprintf(lb, sizeof lb, "%02d", hh % 24);
        tx_backdrop(C_SURF);
        tx_draw(c, lb, px, gy + gh + 6.f, font_make(TF_UI, 10, TW_MED),
                C_INK_3, AL_C, AV_T);
    }

    for (int s = 0; s < wo->nStands; s++) {
        float ry = gy + s*rowH;
        if (s & 1) cv_rect(c, gx, ry, gw, rowH - 2.f, col_alpha(C_SURF_2, .8f));
        tx_backdrop(C_SURF);
        tx_draw(c, wo->stand[s].name, gx - 10.f, ry + rowH*0.5f,
                font_make(TF_UI, 11, TW_SEMI),
                wo->stand[s].kind == ST_CONTACT ? C_INK : C_INK_3, AL_R, AV_M);
    }

    for (int i = 0; i < wo->nFlights; i++) {
        Flight *f = &wo->flight[i];
        int si = (p->ready ? p->assign[i] : f->stand);
        if (si < 0 || si >= wo->nStands) continue;
        float a0 = f->arrival ? (float)f->estMin - 5.f : (float)f->estMin - 105.f;
        float a1 = f->arrival ? (float)f->estMin + 95.f : (float)f->estMin + 12.f;
        if (a1 < t0 || a0 > t1) continue;
        float bx0 = gx + gw * (a0 - t0)/(t1 - t0);
        float bx1 = gx + gw * (a1 - t0)/(t1 - t0);
        if (bx0 < gx) bx0 = gx;
        if (bx1 > gx + gw) bx1 = gx + gw;
        /* An inbound and its outbound share one stand for one continuous
         * period, so their windows necessarily overlap.  Drawing arrivals in
         * the upper band and departures in the lower one makes a turnaround
         * read as a turnaround instead of as a double booking. */
        float band = (rowH - 8.f) * 0.56f;
        float ry = gy + si*rowH + 3.f + (f->arrival ? 0.f : (rowH - 8.f) - band);
        Color bc = wo->airline[f->airline].col;
        int sel = (a->selFlight == f->id);
        cv_rrect(c, bx0, ry, bx1 - bx0, band, 3.5f,
                 col_alpha(bc, sel ? .97f : .72f));
        if (bx1 - bx0 > 42.f) {
            tx_backdrop(bc);
            tx_clipped(c, f->no, bx0 + 5.f, ry + band*0.5f, bx1 - bx0 - 10.f,
                       font_make(TF_UI, 9, TW_BOLD), HEX(0xFFFFFF), AL_L, AV_M);
        }
        if (ui_hit(bx0, ry, bx1-bx0, band)) {
            ui_cursor(1);
            if (a->in.pressed) a->selFlight = f->id;
        }
    }

    /* the clock hand */
    float nowX = gx + gw * (wo->clock - t0)/(t1 - t0);
    if (nowX > gx && nowX < gx + gw) {
        cv_line(c, nowX, gy - 4.f, nowX, gy + gh, C_MAGENTA, 1.8f);
        cv_tri(c, nowX - 5.f, gy - 10.f, nowX + 5.f, gy - 10.f, nowX, gy - 2.f,
               C_MAGENTA);
    }
}

/* ==========================================================================
 *  flow forecaster
 * ========================================================================== */

static void view_flow_ai(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    FlowModel *m = &a->flow;
    float colW = (w - 16.f) * 0.34f;

    ui_card(x, y, colW, h, R_LG);
    tx_backdrop(C_SURF);
    ui_section(x + 20.f, y + 18.f, "MODEL 05", "Flow Forecast");
    tx_para(c, "Holt's linear smoothing carries a level and a trend through "
               "five minute bins of passengers presenting at central search. "
               "The projection then feeds an M/M/c queue, which turns demand "
               "into the number a duty manager can act on: lanes.",
            x + 20.f, y + 62.f, colW - 40.f, font_make(TF_UI, 12, TW_REG),
            C_INK_2, 3);

    float sy = y + 176.f;
    float tw2 = (colW - 40.f - 8.f)/2.f;
    char v[32];
    /* Below a minute, "0 min" reads as a broken readout rather than a very
     * short queue, so sub-minute waits are shown in seconds. */
    if (m->waitNow < 1.f) snprintf(v, sizeof v, "%.0f sec", m->waitNow * 60.f);
    else                  snprintf(v, sizeof v, "%.0f min", m->waitNow);
    draw_stat_tile(c, x + 20.f, sy, tw2, 78.f, "WAIT NOW", v, "",
                   0, m->waitNow > 14.f ? C_DANGER : C_OK);
    if (m->waitPeak < 1.f) snprintf(v, sizeof v, "%.0f sec", m->waitPeak * 60.f);
    else                   snprintf(v, sizeof v, "%.0f min", m->waitPeak);
    draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy, tw2, 78.f, "AT PEAK", v, "",
                   0, m->waitPeak > 14.f ? C_WARN : C_OK);
    snprintf(v, sizeof v, "%.0f%%", m->utilisation*100.f);
    draw_stat_tile(c, x + 20.f, sy + 86.f, tw2, 78.f, "UTILISATION", v, "", 0, C_V600);
    snprintf(v, sizeof v, "%.1f%%", m->mape);
    draw_stat_tile(c, x + 20.f + tw2 + 8.f, sy + 86.f, tw2, 78.f, "MAPE", v,
                   "measured, not claimed", 0, C_TEAL);

    float ry = sy + 180.f;
    tx_draw(c, "LANES OPEN", x + 20.f, ry, font_track(TF_UI, 9, TW_BOLD, 1),
            C_INK_3, AL_L, AV_T);
    ui_stepper(uid("lanes"), x + 20.f, ry + 18.f, colW - 40.f, 40.f,
               &m->lanesOpen, 1, 12);

    char rec[120];
    snprintf(rec, sizeof rec,
             m->lanesNeeded > m->lanesOpen
             ? "Recommendation: open %d lanes to hold the ten minute target."
             : "Recommendation: %d lanes is sufficient for the forecast peak.",
             m->lanesNeeded);
    cv_rrect(c, x + 20.f, ry + 70.f, colW - 40.f, 62.f, 10.f,
             m->lanesNeeded > m->lanesOpen ? C_WARN_BG : C_OK_BG);
    tx_backdrop(m->lanesNeeded > m->lanesOpen ? C_WARN_BG : C_OK_BG);
    tx_para(c, rec, x + 32.f, ry + 82.f, colW - 64.f,
            font_make(TF_UI, 11, TW_SEMI),
            m->lanesNeeded > m->lanesOpen ? HEX(0x8A5A05) : HEX(0x0A6B42), 2);

    /* the queue model's own inputs, so the numbers above can be checked */
    float qy = ry + 148.f;
    tx_backdrop(C_SURF);
    tx_draw(c, "QUEUE MODEL  M/M/c", x + 20.f, qy,
            font_track(TF_UI, 9, TW_BOLD, 1), C_INK_3, AL_L, AV_T);
    float lam = m->nHist ? m->history[m->nHist-1] / 5.f : 0.f;
    struct { const char *k; char v[40]; } qr[4];
    snprintf(qr[0].v, 40, "%.2f pax/min", lam);              qr[0].k = "Arrival rate";
    snprintf(qr[1].v, 40, "2.85 pax/min/lane");              qr[1].k = "Service rate";
    snprintf(qr[2].v, 40, "%d", m->lanesOpen);               qr[2].k = "Servers";
    snprintf(qr[3].v, 40, "%.2f erlang", lam / 2.85f);       qr[3].k = "Offered load";
    for (int i = 0; i < 4; i++) {
        float rr = qy + 20.f + i*21.f;
        tx_draw(c, qr[i].k, x + 20.f, rr, font_make(TF_UI, 11, TW_MED),
                C_INK_2, AL_L, AV_T);
        tx_draw(c, qr[i].v, x + colW - 20.f, rr,
                font_track(TF_MONO, 11, TW_SEMI, 0), C_INK, AL_R, AV_T);
    }

    /* --- the chart -------------------------------------------------------- */
    float cx = x + colW + 16.f, cw = w - colW - 16.f;
    ui_card(cx, y, cw, h, R_LG);
    tx_draw(c, "PASSENGERS PRESENTING AT SEARCH  -  FIVE MINUTE BINS",
            cx + 20.f, y + 18.f, font_track(TF_UI, 10, TW_BOLD, 2), C_V600,
            AL_L, AV_T);

    float gx = cx + 52.f, gy = y + 52.f, gw = cw - 78.f, gh = h - 108.f;
    float maxv = 1.f;
    for (int i = 0; i < m->nHist; i++) if (m->history[i] > maxv) maxv = m->history[i];
    for (int i = 0; i < FLOW_HORIZON; i++) if (m->upper[i] > maxv) maxv = m->upper[i];
    maxv *= 1.15f;

    for (int i = 0; i <= 4; i++) {
        float yy = gy + gh*i/4.f;
        cv_line(c, gx, yy, gx + gw, yy, col_alpha(C_LINE_2, .7f), 1.f);
        char lb[16]; snprintf(lb, sizeof lb, "%.0f", maxv*(4-i)/4.f);
        tx_backdrop(C_SURF);
        tx_draw(c, lb, gx - 8.f, yy, font_make(TF_UI, 10, TW_MED), C_INK_3,
                AL_R, AV_M);
    }

    int totalPts = m->nHist + FLOW_HORIZON;
    float stepX = gw / (float)(totalPts - 1);
    float splitX = gx + stepX * (m->nHist - 1);

    /* forecast band */
    Path band; path_reset(&band);
    for (int i = 0; i < FLOW_HORIZON; i++) {
        float px = splitX + stepX*(i+1);
        float py = gy + gh - (m->upper[i]/maxv)*gh;
        if (i == 0) path_move(&band, splitX, gy + gh - (m->history[m->nHist-1]/maxv)*gh);
        path_line(&band, px, py);
    }
    for (int i = FLOW_HORIZON - 1; i >= 0; i--) {
        float px = splitX + stepX*(i+1);
        float py = gy + gh - (m->lower[i]/maxv)*gh;
        path_line(&band, px, py);
    }
    path_close(&band);
    cv_fill_col(c, &band, col_alpha(C_V400, .18f));

    /* observed */
    Path obs; path_reset(&obs);
    for (int i = 0; i < m->nHist; i++) {
        float px = gx + stepX*i;
        float py = gy + gh - (m->history[i]/maxv)*gh;
        if (i == 0) path_move(&obs, px, py); else path_line(&obs, px, py);
    }
    cv_stroke_col(c, &obs, C_V700, 2.4f);

    /* forecast */
    Path fc; path_reset(&fc);
    path_move(&fc, splitX, gy + gh - (m->history[m->nHist-1]/maxv)*gh);
    for (int i = 0; i < FLOW_HORIZON; i++)
        path_line(&fc, splitX + stepX*(i+1), gy + gh - (m->forecast[i]/maxv)*gh);
    cv_stroke_col(c, &fc, C_MAGENTA, 2.4f);

    cv_line(c, splitX, gy, splitX, gy + gh, col_alpha(C_INK_3, .6f), 1.4f);
    tx_backdrop(C_SURF);
    tx_draw(c, "now", splitX, gy - 6.f, font_make(TF_UI, 10, TW_SEMI),
            C_INK_3, AL_C, AV_B);
    tx_draw(c, "observed", gx + 8.f, gy + gh + 10.f, font_make(TF_UI, 11, TW_SEMI),
            C_V700, AL_L, AV_T);
    tx_draw(c, "forecast, 80% interval", splitX + 12.f, gy + gh + 10.f,
            font_make(TF_UI, 11, TW_SEMI), C_MAGENTA, AL_L, AV_T);
    char ml[80];
    snprintf(ml, sizeof ml, "alpha %.2f   beta %.2f   level %.1f   trend %+.2f",
             m->alpha, m->beta, m->level, m->trend);
    tx_draw(c, ml, cx + cw - 20.f, y + 18.f, font_track(TF_MONO, 11, TW_MED, 0),
            C_INK_3, AL_R, AV_T);
}

/* ==========================================================================
 *  screen
 * ========================================================================== */

void screen_ai(App *a, float x, float y, float w, float h)
{
    float pad = PAD;
    static const char *TABS[AIT_COUNT] = {
        "Assistant", "Delay Oracle", "BagScan", "Stand Allocator", "Flow Forecast"
    };
    static const int TABIC[AIT_COUNT] = {
        IC_CHAT, IC_TREND, IC_SHIELD, IC_GATE, IC_USERS
    };

    float tx0 = x + pad;
    for (int i = 0; i < AIT_COUNT; i++) {
        float tw = 150.f;
        if (ui_tab(uidi("aitab", i), tx0, y + pad, tw, 42.f, TABS[i], TABIC[i],
                   a->aiTab == i))
            a->aiTab = i;
        tx0 += tw + 4.f;
    }

    float cy = y + pad + 54.f;
    float ch = h - (cy - y) - pad;

    switch (a->aiTab) {
    case AIT_ASSISTANT: {
        float cw = w - pad*2.f;
        float chatW = cw > 900.f ? 620.f : cw*0.62f;
        Canvas *c = &a->cv;
        ui_card(x + pad, cy, chatW, ch, R_XL);
        draw_chat_panel(a, x + pad, cy, chatW, ch, 0);

        /* a short brief on what the assistant is, beside it */
        float ix = x + pad + chatW + 16.f, iw = cw - chatW - 16.f;
        ui_card(ix, cy, iw, ch, R_LG);
        tx_backdrop(C_SURF);
        ui_section(ix + 20.f, cy + 18.f, "MODEL 01", "Operations Assistant");
        tx_para(c,
          "Intent classification over a hand-built corpus, weighted by inverse "
          "document frequency so common words count for little and specific "
          "ones carry the decision. Entities -- flight numbers, bag tags, "
          "booking references, airports, stands -- are extracted separately, "
          "which is why a bare flight number works and why follow-up questions "
          "resolve against what you asked last.\n\n"
          "Try these:",
          ix + 20.f, cy + 62.f, iw - 40.f, font_make(TF_UI, 12, TW_REG),
          C_INK_2, 3);

        const char *ex[] = {
            "where is MK046",
            "which flights are delayed",
            "how is the baggage system",
            "what is the security wait",
            "what is the crosswind",
            "give me a summary",
            "how do your models work",
        };
        float ey = cy + 222.f;
        for (int i = 0; i < 7; i++) {
            if (ey > cy + ch - 46.f) break;
            if (ui_button(uidi("aiex", i), ix + 20.f, ey, iw - 40.f, 36.f,
                          ex[i], BTN_OUTLINE)) {
                ai_chat_send(&a->w, &a->chat, ex[i]);
            }
            ey += 42.f;
        }
    } break;

    case AIT_DELAY:   view_delay  (a, x + pad, cy, w - pad*2.f, ch); break;
    case AIT_BAGSCAN: view_bagscan(a, x + pad, cy, w - pad*2.f, ch); break;
    case AIT_STAND:   view_stand  (a, x + pad, cy, w - pad*2.f, ch); break;
    case AIT_FLOW:    view_flow_ai(a, x + pad, cy, w - pad*2.f, ch); break;
    default: break;
    }
}
