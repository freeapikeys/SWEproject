/* ==========================================================================
 *  AURA :: screen_records.c   --   files, persistence and the audit trail
 *
 *  The brief asks for the use of files, and this screen is where that becomes
 *  visible.  Each card is a real file under data/: its size is read from
 *  disk, writing it serialises live state, and reading it back restores that
 *  state into the running application.  The journal is append-only, so the
 *  history of a session survives it.
 * ========================================================================== */

#include "../app.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    const char *file;
    const char *title;
    const char *desc;
    int         icon;
    Color       accent;
} FileCard;

static const FileCard CARDS[] = {
    { "flights.csv",    "Movement schedule",
      "Every arrival and departure with its stand, gate, times and live state.",
      IC_PLANE,    0 },
    { "passengers.csv", "Passenger roster",
      "Booking references, seats, bag counts and acceptance status.",
      IC_USERS,    0 },
    { "baggage.csv",    "Baggage register",
      "Each bag, its owner, its position in the system and its screening score.",
      IC_LUGGAGE,  0 },
    { "staff.csv",      "Duty roster",
      "Staff, roles and shift allocation across the operating day.",
      IC_USER,     0 },
};
#define NCARDS ((int)(sizeof CARDS / sizeof CARDS[0]))

static void human_size(int bytes, char *out)
{
    if (bytes <= 0)          sprintf(out, "not written yet");
    else if (bytes < 1024)   sprintf(out, "%d bytes", bytes);
    else                     sprintf(out, "%.1f KB", bytes / 1024.f);
}

void screen_records(App *a, float x, float y, float w, float h)
{
    Canvas *c = &a->cv;
    World *wo = &a->w;
    float pad = PAD;

    float leftW = (w - pad*2.f) * 0.56f;

    /* ---- header actions -------------------------------------------------- */
    ui_card(x + pad, y + pad, leftW, 92.f, R_LG);
    tx_backdrop(C_SURF);
    ui_section(x + pad + 20.f, y + pad + 16.f, "PERSISTENCE",
               "Operational records");
    char root[220];
    snprintf(root, sizeof root, "Writing to  .\\%s\\", store_root());
    tx_draw(c, root, x + pad + 20.f, y + pad + 62.f,
            font_track(TF_MONO, 11, TW_MED, 0), C_INK_3, AL_L, AV_T);

    if (ui_button_i(uid("recsaveall"), x + pad + leftW - 320.f, y + pad + 26.f,
                    150.f, 42.f, "Write all", IC_SAVE, BTN_PRIMARY)) {
        StoreResult r = store_save_all(wo);
        char m[140];
        snprintf(m, sizeof m, "%d records across four files", r.records);
        ui_toast(r.ok ? TOAST_OK : TOAST_ERR, "Records written", m);
    }
    if (ui_button_i(uid("recreport"), x + pad + leftW - 160.f, y + pad + 26.f,
                    140.f, 42.f, "Export report", IC_FILE, BTN_OUTLINE)) {
        StoreResult r = store_export_report(wo);
        ui_toast(r.ok ? TOAST_OK : TOAST_ERR, "Daily report exported",
                 "data/daily_report.txt");
    }

    /* ---- file cards ------------------------------------------------------ */
    float cy = y + pad + 104.f;
    float cardH = 116.f;
    for (int i = 0; i < NCARDS; i++) {
        float ry = cy + i*(cardH + 12.f);
        if (ry + cardH > y + h - pad) break;
        ui_card(x + pad, ry, leftW, cardH, R_LG);

        Color ac = (i == 0) ? C_V600 : (i == 1) ? C_TEAL
                 : (i == 2) ? C_MAGENTA : C_GOLD;
        cv_circle(c, x + pad + 42.f, ry + 40.f, 20.f, col_alpha(ac, .14f));
        icon_draw(c, CARDS[i].icon, x + pad + 42.f, ry + 40.f, 20.f, ac);

        tx_backdrop(C_SURF);
        tx_draw(c, CARDS[i].title, x + pad + 74.f, ry + 18.f,
                font_make(TF_UI, 15, TW_SEMI), C_INK, AL_L, AV_T);
        tx_draw(c, CARDS[i].file, x + pad + 74.f, ry + 39.f,
                font_track(TF_MONO, 11, TW_MED, 0), C_V600, AL_L, AV_T);
        tx_para(c, CARDS[i].desc, x + pad + 74.f, ry + 58.f, leftW - 240.f,
                font_make(TF_UI, 11, TW_REG), C_INK_3, 2);

        int sz = store_file_size(CARDS[i].file);
        char szt[40]; human_size(sz, szt);
        tx_draw(c, szt, x + pad + leftW - 20.f, ry + 18.f,
                font_make(TF_UI, 11, TW_SEMI), sz ? C_INK_2 : C_INK_4,
                AL_R, AV_T);

        float bx = x + pad + leftW - 168.f;
        if (ui_button(uidi("wr", i), bx, ry + 62.f, 74.f, 34.f, "Write",
                      BTN_SOFT)) {
            StoreResult r;
            switch (i) {
            case 0: r = store_save_flights(wo);    break;
            case 1: r = store_save_passengers(wo); break;
            case 2: r = store_save_baggage(wo);    break;
            default:r = store_save_staff(wo);      break;
            }
            char m[140];
            snprintf(m, sizeof m, "%d records -> %s", r.records, CARDS[i].file);
            ui_toast(r.ok ? TOAST_OK : TOAST_ERR, "File written", m);
        }
        if (i < 3 && ui_button(uidi("rd", i), bx + 82.f, ry + 62.f, 74.f, 34.f,
                               "Read", BTN_OUTLINE)) {
            StoreResult r;
            switch (i) {
            case 0: r = store_load_flights(wo);    break;
            case 1: r = store_load_passengers(wo); break;
            default:r = store_load_baggage(wo);    break;
            }
            char m[140];
            snprintf(m, sizeof m, "%d records restored", r.records);
            ui_toast(r.ok ? TOAST_OK : TOAST_WARN,
                     r.ok ? "File read" : "Nothing to read", m);
            world_log(wo, LG_INFO, "%s read back from disk", CARDS[i].file);
        }
    }

    /* ---- journal --------------------------------------------------------- */
    float rx = x + pad + leftW + 14.f;
    float rw = w - pad*2.f - leftW - 14.f;
    float jh = (h - pad*2.f) * 0.52f;

    ui_card(rx, y + pad, rw, jh, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "JOURNAL.LOG", rx + 20.f, y + pad + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    tx_draw(c, "append-only audit trail", rx + 20.f, y + pad + 32.f,
            font_make(TF_UI, 11, TW_REG), C_INK_3, AL_L, AV_T);

    int jsz = store_file_size("journal.log");
    char jt[40]; human_size(jsz, jt);
    tx_draw(c, jt, rx + rw - 20.f, y + pad + 16.f, font_make(TF_UI, 11, TW_SEMI),
            C_INK_2, AL_R, AV_T);

    static char tail[4200];
    static float refresh = 0.f;
    refresh += anim_dt();
    if (refresh > 1.2f) { refresh = 0.f; store_read_tail("journal.log", tail,
                                                         sizeof tail, 60); }
    if (!tail[0]) store_read_tail("journal.log", tail, sizeof tail, 60);

    float ty = y + pad + 54.f, th = jh - 66.f;
    cv_rrect(c, rx + 16.f, ty, rw - 32.f, th, 10.f, C_NIGHT);
    cv_clip_push(c, rx + 16.f, ty, rw - 32.f, th);
    tx_backdrop(C_NIGHT);

    /* count the lines so the view can scroll */
    int lines = 0;
    for (const char *p = tail; *p; p++) if (*p == '\n') lines++;
    float lineH = 17.f;
    float off = ui_scroll_begin(uid("jscroll"), rx + 16.f, ty, rw - 32.f, th,
                                lines*lineH + 16.f);
    {
        const char *p = tail;
        int i = 0;
        char buf[240];
        while (*p) {
            int k = 0;
            while (*p && *p != '\n' && k < 239) buf[k++] = *p++;
            buf[k] = 0;
            if (*p == '\n') p++;
            float ly = ty + 8.f + i*lineH - off;
            if (ly > ty - lineH && ly < ty + th)
                tx_clipped(c, buf, rx + 26.f, ly, rw - 56.f,
                           font_track(TF_MONO, 11, TW_REG, 0),
                           col_alpha(C_V200, .88f), AL_L, AV_T);
            i++;
        }
        if (!lines) {
            tx_draw(c, "The journal is written when records are saved.",
                    rx + 26.f, ty + 12.f, font_make(TF_UI, 11, TW_MED),
                    col_alpha(C_V300, .7f), AL_L, AV_T);
        }
    }
    ui_scroll_end();
    cv_clip_pop(c);

    /* ---- session event log ---------------------------------------------- */
    float ey = y + pad + jh + 14.f;
    float eh = h - (ey - y) - pad;
    ui_card(rx, ey, rw, eh, R_LG);
    tx_backdrop(C_SURF);
    tx_draw(c, "SESSION EVENTS", rx + 20.f, ey + 16.f,
            font_track(TF_UI, 10, TW_BOLD, 2), C_V600, AL_L, AV_T);
    char cnt[40]; snprintf(cnt, sizeof cnt, "%d entries", wo->nLog);
    tx_draw(c, cnt, rx + rw - 20.f, ey + 16.f, font_make(TF_UI, 11, TW_SEMI),
            C_INK_3, AL_R, AV_T);

    float ly2 = ey + 42.f, lh2 = eh - 52.f;
    float rowH = 44.f;
    float off2 = ui_scroll_begin(uid("evscroll"), rx + 6.f, ly2, rw - 12.f, lh2,
                                 wo->nLog*rowH + 8.f);
    for (int i = 0; i < wo->nLog; i++) {
        LogEntry *e = &wo->log[wo->nLog - 1 - i];
        float ry = ly2 + i*rowH - off2;
        if (ry + rowH < ly2 || ry > ly2 + lh2) continue;
        Color kc = e->kind == LG_OK ? C_OK : (e->kind == LG_WARN ? C_WARN
                 : (e->kind == LG_ERR ? C_DANGER
                 : (e->kind == LG_AI ? C_MAGENTA : C_INFO)));
        if (i & 1) cv_rrect(c, rx + 12.f, ry, rw - 24.f, rowH - 6.f, 8.f, C_SURF_2);
        cv_circle(c, rx + 26.f, ry + 16.f, 4.f, kc);
        char hhmm[8]; fmt_hhmm(e->minute, hhmm);
        tx_backdrop(i & 1 ? C_SURF_2 : C_SURF);
        tx_draw(c, hhmm, rx + 38.f, ry + 8.f, font_track(TF_MONO, 11, TW_BOLD, 0),
                C_INK_3, AL_L, AV_T);
        tx_para(c, e->text, rx + 82.f, ry + 6.f, rw - 100.f,
                font_make(TF_UI, 11, TW_MED), C_INK_2, 1);
    }
    ui_scroll_end();
}
