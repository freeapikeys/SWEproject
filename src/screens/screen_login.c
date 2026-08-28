/* ==========================================================================
 *  AURA :: screen_login.c   --   the way in
 *
 *  Registration is open.  A traveller arrives with whatever address they
 *  already use, creates an account and is straight into the application; the
 *  same screen signs them back in afterwards.  Typing an address that has no
 *  account yet does not dead-end -- it offers to create one.
 *
 *  The card is AURA's own and is not dressed up as somebody else's sign-in
 *  page.  Federated sign-in would need a browser, a network round trip and a
 *  client secret, none of which this application has by design, and a screen
 *  that imitated Google while collecting a Google password would be a
 *  phishing page whatever it was built for.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/* --------------------------------------------------------------------------
 *  backdrop
 * ------------------------------------------------------------------------- */

static void draw_backdrop(Canvas *c, float sw, float sh)
{
    Paint bg = paint_linear(0, 0, sw, sh, C_V950, C_V800);
    cv_rect_p(c, 0, 0, sw, sh, &bg);

    float t  = anim_time();
    float cx = sw * 0.26f, cy = sh * 0.52f;

    for (int i = 0; i < 6; i++)
        cv_circle_line(c, cx, cy, 90.f + i*74.f, col_alpha(C_V400, .07f), 1.f);

    float sweep = t * 0.55f;
    for (int i = 0; i < 46; i++) {
        float a0 = sweep - i*0.028f;
        float fade = 1.f - i/46.f;
        cv_line(c, cx, cy, cx + cosf(a0)*430.f, cy + sinf(a0)*430.f,
                col_alpha(C_V300, .10f*fade*fade), 2.6f);
    }
    for (int i = 0; i < 9; i++) {
        float a0 = 0.7f + i*0.71f, rr = 110.f + i*36.f;
        float blip = sinf(t*1.4f - i*0.8f)*0.5f + 0.5f;
        cv_circle(c, cx + cosf(a0)*rr, cy + sinf(a0)*rr, 2.4f,
                  col_alpha(C_MAGENTA, .16f + .40f*blip));
    }
    for (int i = 0; i < 5; i++) {
        float ph = fmodf(t*0.035f + i*0.21f, 1.f);
        float px = -60.f + ph * (sw + 120.f);
        float py = sh*0.18f + i*sh*0.16f + sinf(t*0.3f + i)*14.f;
        draw_aircraft(c, px, py, 0.f, 26.f + i*4.f,
                      col_alpha(C_V300, .10f), col_alpha(C_V200, .13f), 0, 0.4f);
    }
}

/* --------------------------------------------------------------------------
 *  the brand column
 * ------------------------------------------------------------------------- */

static void draw_brand(App *a, float x, float y, float w)
{
    Canvas *c  = &a->cv;
    World  *wo = &a->w;

    draw_logo(c, x + 30.f, y + 26.f, 26.f);

    tx_backdrop(C_V900);
    tx_draw(c, "AURA", x + 76.f, y - 6.f, font_track(TF_DISPLAY, 54, TW_BOLD, 5),
            HEX(0xFFFFFF), AL_L, AV_T);
    tx_draw(c, "AIRPORT UNIFIED RESOURCE ADMINISTRATION", x, y + 78.f,
            font_track(TF_UI, 10, TW_SEMI, 3), C_V200, AL_L, AV_T);
    tx_clipped(c, "Sir Seewoosagur Ramgoolam International  -  Plaisance, Mauritius",
               x, y + 100.f, w, font_make(TF_UI, 12, TW_REG), C_V300, AL_L, AV_T);

    char clk[16];
    fmt_hhmmss(wo->clock, clk);
    tx_draw(c, clk, x, y + 146.f, font_track(TF_DISPLAY, 40, TW_BOLD, 1),
            HEX(0xFFFFFF), AL_L, AV_T);
    char ds[90];
    snprintf(ds, sizeof ds, "%s %d %s %d   -   local time, UTC+4",
             weekday_name(wo->weekday), wo->day, month_name(wo->month), wo->year);
    tx_clipped(c, ds, x, y + 192.f, w, font_make(TF_UI, 12, TW_MED),
               col_alpha(C_V200, .9f), AL_L, AV_T);

    float lp = 0.5f + 0.5f*sinf(anim_time()*2.2f);
    cv_circle(c, x + 5.f, y + 224.f, 4.f + lp*1.4f,
              col_alpha(C_OK, .55f + .45f*lp));
    tx_draw(c, "OPERATIONS CORE ONLINE", x + 18.f, y + 224.f,
            font_track(TF_UI, 9, TW_BOLD, 2), col_alpha(C_OK, .95f), AL_L, AV_M);

    char mv[110];
    snprintf(mv, sizeof mv,
             "%d movements  -  %d passengers today  -  %d bags tracked",
             wo->nFlights, wo->totalPaxToday, wo->nBags);
    tx_clipped(c, mv, x, y + 244.f, w, font_make(TF_UI, 11, TW_REG),
               col_alpha(C_V300, .8f), AL_L, AV_T);

    /* what an account is for, in plain words */
    const char *why[3] = {
        "Find your flight, gate and seat in one search",
        "Get told about gate changes, delays and boarding",
        "Track your bags and leave a review of the airport",
    };
    float yy = y + 288.f;
    for (int i = 0; i < 3; i++) {
        cv_circle(c, x + 5.f, yy + 6.f, 3.f, col_alpha(C_V300, .8f));
        tx_clipped(c, why[i], x + 18.f, yy, w - 18.f,
                   font_make(TF_UI, 12, TW_REG), col_alpha(C_V200, .85f),
                   AL_L, AV_T);
        yy += 24.f;
    }
}

/* --------------------------------------------------------------------------
 *  a small segmented control, for traveller / staff
 * ------------------------------------------------------------------------- */

static void kind_picker(App *a, float x, float y, float w)
{
    Canvas *c = &a->cv;
    float hw = w*0.5f;
    cv_rrect(c, x, y, w, 40.f, 10.f, C_SURF_2);
    float sel = anim_to(uid("lgkind"), a->loginKind == ACC_STAFF ? 1.f : 0.f, 16.f);
    cv_rrect(c, x + 3.f + sel*(hw - 3.f), y + 3.f, hw - 3.f, 34.f, 8.f, C_SURF);
    cv_rrect_line(c, x + 3.f + sel*(hw - 3.f), y + 3.f, hw - 3.f, 34.f, 8.f,
                  col_alpha(C_V400, .5f), 1.2f);

    const char *lab[2] = { "Traveller", "Airport staff" };
    for (int i = 0; i < 2; i++) {
        float bx = x + i*hw;
        if (ui_hit(bx, y, hw, 40.f)) {
            ui_cursor(1);
            if (a->in.pressed) a->loginKind = i ? ACC_STAFF : ACC_TRAVELLER;
        }
        int on = (a->loginKind == ACC_STAFF) == (i == 1);
        tx_backdrop(on ? C_SURF : C_SURF_2);
        tx_draw(c, lab[i], bx + hw*0.5f, y + 20.f,
                font_make(TF_UI, 12, on ? TW_SEMI : TW_MED),
                on ? C_INK : C_INK_3, AL_C, AV_M);
    }
}

/* ==========================================================================
 *  the card
 * ========================================================================== */

void screen_login(App *a, float sw, float sh)
{
    Canvas *c = &a->cv;
    AuthSystem *au = &a->auth;

    draw_backdrop(c, sw, sh);

    int reg = (au->stage == AUTH_REGISTER);
    float cardW = 428.f;
    float cardH = reg ? 640.f : 430.f;
    float cx = sw - cardW - (sw > 1120.f ? 96.f : (sw - cardW)*0.5f);
    if (cx < 24.f) cx = (sw - cardW)*0.5f;
    float cy = (sh - cardH)*0.5f;
    if (cy < 16.f) cy = 16.f;

    if (sw > 1120.f) draw_brand(a, 96.f, (sh - 420.f)*0.5f, cx - 150.f);

    float rise = ease_out_cubic(anim_to(uid("lgrise"), 1.f, 7.f));
    cy += (1.f - rise) * 22.f;

    cv_shadow(c, cx, cy + 14.f, cardW, cardH, R_XL, 44.f, RGBA(8, 2, 30, 150));
    cv_rrect(c, cx, cy, cardW, cardH, R_XL, C_SURF);

    float px = cx + 34.f, pw = cardW - 68.f;
    float y  = cy + 32.f;

    cv_circle(c, px + 17.f, y + 17.f, 17.f, col_alpha(C_V500, .12f));
    icon_draw(c, reg ? IC_USER : IC_LOCK, px + 17.f, y + 17.f, 18.f, C_V600);

    tx_backdrop(C_SURF);
    tx_draw(c, reg ? "Create your account" : "Sign in",
            px + 46.f, y + 1.f, font_track(TF_DISPLAY, 22, TW_BOLD, 0),
            C_INK, AL_L, AV_T);
    tx_draw(c, reg ? "Any email address will do"
                   : "Travellers and airport staff",
            px + 46.f, y + 26.f, font_make(TF_UI, 11, TW_MED), C_INK_3,
            AL_L, AV_T);
    y += 56.f;

    float secsLeft = 0.f;
    int   locked   = auth_locked_out(au, &secsLeft);
    Input *in      = ui_input();
    int   submit   = in->keyHit[VK_RETURN];

    if (reg) {
        /* ---- create an account ------------------------------------------ */
        tx_draw(c, "YOUR NAME", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_text_field(uid("lgname"), px, y, pw, 44.f, a->loginName,
                      (int)sizeof a->loginName, "as it appears on your passport",
                      IC_USER);
        y += 54.f;

        tx_draw(c, "EMAIL", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_text_field(uid("lgmail"), px, y, pw, 44.f, a->loginEmail,
                      (int)sizeof a->loginEmail, "you@example.com", IC_SEND);
        y += 54.f;

        tx_draw(c, "PASSWORD", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_secret_field(uid("lgpw1"), px, y, pw, 44.f, a->loginPw,
                        (int)sizeof a->loginPw, "at least 8 characters", IC_LOCK);
        y += 50.f;

        char why[130];
        int  st = auth_pw_strength(a->loginPw, why, (int)sizeof why);
        Color sc = st == 0 ? C_DANGER : (st == 1 ? C_WARN : C_OK);
        int   lit = a->loginPw[0] ? st + 1 : 0;
        for (int i = 0; i < 3; i++)
            cv_rrect(c, px + i*(pw/3.f), y, pw/3.f - 6.f, 4.f, 2.f,
                     i < lit ? sc : col_alpha(C_LINE_2, .8f));
        tx_clipped(c, a->loginPw[0] ? why : "Choose something only you know",
                   px, y + 10.f, pw, font_make(TF_UI, 11, TW_MED),
                   a->loginPw[0] ? sc : C_INK_3, AL_L, AV_T);
        y += 34.f;

        tx_draw(c, "CONFIRM", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_secret_field(uid("lgpw2"), px, y, pw, 44.f, a->loginPw2,
                        (int)sizeof a->loginPw2, "type it again", IC_LOCK);
        y += 56.f;

        kind_picker(a, px, y, pw);
        y += 52.f;

        if (ui_button(uid("lgcreate"), px, y, pw, 46.f, "Create account",
                      BTN_PRIMARY) || submit) {
            if (auth_register(au, a->loginEmail, a->loginName, a->loginPw,
                              a->loginPw2, (AccountKind)a->loginKind, "data")) {
                memset(a->loginPw,  0, sizeof a->loginPw);
                memset(a->loginPw2, 0, sizeof a->loginPw2);
                ui_focus_clear();
                /*  Staff open on the operations floor, travellers on their
                 *  own journey.  Neither is locked out of the other -- the
                 *  landing screen is a courtesy, not a permission.         */
                a->welcomeMatched = 0; a->selPax = 0;
                a->screen = (a->loginKind == ACC_STAFF) ? SC_OPS : SC_WELCOME;
                store_journal(&a->w, "Account created");
                store_journal(&a->w, "Signed in");
                ui_toast(TOAST_OK, "Welcome to AURA", au->name);
            }
        }
        y += 54.f;
        int back = ui_button(uid("lgback"), px, y, pw, 34.f,
                             "I already have an account", BTN_GHOST);
        y += 44.f;
        if (back) {
            au->stage = AUTH_LOCKED;
            memset(a->loginPw,  0, sizeof a->loginPw);
            memset(a->loginPw2, 0, sizeof a->loginPw2);
            snprintf(au->message, sizeof au->message, "Sign in to continue.");
            au->messageBad = 0;
        }
    } else {
        /* ---- sign in ----------------------------------------------------- */
        tx_draw(c, "EMAIL", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_text_field(uid("lgmail"), px, y, pw, 46.f, a->loginEmail,
                      (int)sizeof a->loginEmail, "you@example.com", IC_USER);
        y += 58.f;

        tx_draw(c, "PASSWORD", px, y, font_track(TF_UI, 9, TW_BOLD, 1),
                C_INK_3, AL_L, AV_T);
        y += 16.f;
        ui_secret_field(uid("lgpw"), px, y, pw, 46.f, a->loginPw,
                        (int)sizeof a->loginPw, "", IC_LOCK);
        y += 62.f;

        if (locked) {
            cv_rrect(c, px, y, pw, 46.f, 11.f, col_alpha(C_DANGER, .10f));
            char t[80];
            snprintf(t, sizeof t, "Locked  -  %d s", (int)secsLeft + 1);
            tx_backdrop(C_SURF);
            tx_draw(c, t, px + pw*0.5f, y + 23.f, font_make(TF_UI, 14, TW_BOLD),
                    C_DANGER, AL_C, AV_M);
        } else if (ui_button(uid("lgin"), px, y, pw, 46.f, "Sign in",
                             BTN_PRIMARY) || submit) {
            if (a->loginEmail[0] && a->loginPw[0]) {
                if (auth_verify(au, a->loginEmail, a->loginPw)) {
                    memset(a->loginPw, 0, sizeof a->loginPw);
                    ui_focus_clear();
                    a->welcomeMatched = 0; a->selPax = 0;
                    a->screen = (au->kind == ACC_STAFF) ? SC_OPS : SC_WELCOME;
                    store_journal(&a->w, "Signed in");
                    ui_toast(TOAST_OK, "Welcome back", au->name);
                } else {
                    memset(a->loginPw, 0, sizeof a->loginPw);
                    if (au->stage != AUTH_REGISTER)
                        store_journal(&a->w, "Failed sign-in attempt");
                }
            } else {
                snprintf(au->message, sizeof au->message,
                         "Enter your email and password.");
                au->messageBad = 1;
            }
        }
        y += 54.f;

        int mk = ui_button(uid("lgnew"), px, y, pw, 36.f,
                           "Create an account", BTN_OUTLINE);
        y += 46.f;
        if (mk) {
            au->stage = AUTH_REGISTER;
            memset(a->loginPw,  0, sizeof a->loginPw);
            memset(a->loginPw2, 0, sizeof a->loginPw2);
            snprintf(au->message, sizeof au->message,
                     "Any email address works. Nothing is sent anywhere.");
            au->messageBad = 0;
        }
    }

    /* ---- message -------------------------------------------------------- */
    if (au->message[0]) {
        Color mc = au->messageBad ? C_DANGER : C_INK_3;
        if (au->messageBad)
            cv_rrect(c, px - 6.f, y - 6.f, pw + 12.f, 40.f, 9.f,
                     col_alpha(C_DANGER, .07f));
        tx_backdrop(C_SURF);
        tx_para(c, au->message, px, y, pw, font_make(TF_UI, 11, TW_MED), mc, 2);
    }

    /* ---- footer --------------------------------------------------------- */
    ui_divider(px, cy + cardH - 50.f, pw);
    tx_backdrop(C_SURF);
    tx_clipped(c, "Local account  -  salted SHA-256, 100000 iterations",
               px, cy + cardH - 36.f, pw, font_make(TF_UI, 10, TW_REG),
               C_INK_4, AL_L, AV_T);
    char cnt[60];
    snprintf(cnt, sizeof cnt, "%d account%s", au->nAcct,
             au->nAcct == 1 ? "" : "s");
    tx_draw(c, cnt, px + pw, cy + cardH - 36.f, font_make(TF_UI, 10, TW_REG),
            C_INK_4, AL_R, AV_T);

    ui_toasts_draw(sw, sh);
}
