/* ==========================================================================
 *  AURA :: store.c
 * ========================================================================== */

#include "store.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <direct.h>

static char g_root[240] = "data";

void store_set_root(const char *dir)
{
    snprintf(g_root, sizeof g_root, "%s", dir);
    _mkdir(g_root);
}

const char *store_root(void) { return g_root; }

static void path_of(const char *name, char *out, int cap)
{
    snprintf(out, (size_t)cap, "%s/%s", g_root, name);
}

static StoreResult result(int ok, const char *path, int n, const char *msg)
{
    StoreResult r;
    r.ok = ok; r.records = n;
    snprintf(r.path, sizeof r.path, "%s", path ? path : "");
    snprintf(r.message, sizeof r.message, "%s", msg ? msg : "");
    return r;
}

int store_exists(const char *name)
{
    char p[300]; path_of(name, p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int store_file_size(const char *name)
{
    char p[300]; path_of(name, p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return (int)n;
}

int store_read_tail(const char *name, char *out, int cap, int maxLines)
{
    char p[300]; path_of(name, p, sizeof p);
    FILE *f = fopen(p, "rb");
    out[0] = 0;
    if (!f) return 0;

    static char lines[220][200];
    int n = 0;
    char buf[200];
    while (fgets(buf, sizeof buf, f)) {
        size_t L = strlen(buf);
        while (L && (buf[L-1] == '\n' || buf[L-1] == '\r')) buf[--L] = 0;
        snprintf(lines[n % 220], 200, "%s", buf);
        n++;
    }
    fclose(f);

    int total = n < 220 ? n : 220;
    int show  = total < maxLines ? total : maxLines;
    int start = n - show;
    int used  = 0;
    for (int i = 0; i < show; i++) {
        int idx = (start + i) % 220;
        if (n < 220) idx = start + i;
        int w = snprintf(out + used, (size_t)(cap - used), "%s\n", lines[idx]);
        if (w < 0 || used + w >= cap - 1) break;
        used += w;
    }
    return show;
}

/* ==========================================================================
 *  csv helpers
 * ========================================================================== */

static char *csv_field(char **cursor)
{
    char *s = *cursor;
    if (!s || !*s) return NULL;
    char *start = s;
    while (*s && *s != ',' && *s != '\n' && *s != '\r') s++;
    if (*s) { *s = 0; s++; }
    *cursor = s;
    return start;
}

/* ==========================================================================
 *  flights
 * ========================================================================== */

StoreResult store_save_flights(World *w)
{
    char p[300]; path_of("flights.csv", p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return result(0, p, 0, "Could not open flights.csv for writing");

    fprintf(f, "id,flight_no,airline,type,reg,airport,arrival,sched,est,"
               "delay,state,stand,gate,belt,pax_cap,pax,checked_in,boarded,"
               "bags,bags_loaded,delay_risk\n");
    for (int i = 0; i < w->nFlights; i++) {
        Flight *fl = &w->flight[i];
        fprintf(f, "%d,%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%s,%d,%s,%d,%d,%d,%d,%d,%d,%.4f\n",
                fl->id, fl->no, w->airline[fl->airline].iata,
                w->actype[fl->acType].code, fl->reg,
                w->airport[fl->airport].iata, fl->arrival,
                fl->schedMin, fl->estMin, fl->delayMin, (int)fl->state,
                fl->stand >= 0 ? w->stand[fl->stand].name : "--",
                fl->gate, fl->belt[0] ? fl->belt : "-",
                fl->paxCap, fl->pax, fl->checkedIn, fl->boarded,
                fl->bags, fl->bagsLoaded, fl->delayRisk);
    }
    fclose(f);
    return result(1, p, w->nFlights, "Movement schedule written");
}

StoreResult store_load_flights(World *w)
{
    char p[300]; path_of("flights.csv", p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return result(0, p, 0, "flights.csv not found");

    char line[512];
    int n = 0, updated = 0;
    while (fgets(line, sizeof line, f)) {
        if (n++ == 0) continue;                       /* header             */
        char *cur = line;
        char *id    = csv_field(&cur);
        char *no    = csv_field(&cur);
        if (!id || !no) continue;
        Flight *fl = flight_by_no(w, no);
        if (!fl) continue;
        for (int skip = 0; skip < 5; skip++) csv_field(&cur);  /* al..arr   */
        char *sched = csv_field(&cur);
        char *est   = csv_field(&cur);
        char *dly   = csv_field(&cur);
        char *st    = csv_field(&cur);
        if (sched) fl->schedMin = atoi(sched);
        if (est)   fl->estMin   = atoi(est);
        if (dly)   fl->delayMin = atoi(dly);
        if (st)    fl->state    = (FlightState)atoi(st);
        updated++;
    }
    fclose(f);
    return result(1, p, updated, "Movement schedule restored");
}

/* ==========================================================================
 *  passengers
 * ========================================================================== */

StoreResult store_save_passengers(World *w)
{
    char p[300]; path_of("passengers.csv", p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return result(0, p, 0, "Could not open passengers.csv");

    fprintf(f, "id,pnr,name,flight,seat,bags,checked_in,security,boarded,"
               "loyalty,fast_track,wheelchair,infant\n");
    for (int i = 0; i < w->nPax; i++) {
        Passenger *x = &w->pax[i];
        Flight *fl = flight_by_id(w, x->flight);
        fprintf(f, "%d,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d\n",
                x->id, x->pnr, x->name, fl ? fl->no : "----", x->seat,
                x->bags, x->checkedIn, x->security, x->boarded,
                x->loyalty, x->fastTrack, x->wheelchair, x->infant);
    }
    fclose(f);
    return result(1, p, w->nPax, "Passenger roster written");
}

StoreResult store_load_passengers(World *w)
{
    char p[300]; path_of("passengers.csv", p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return result(0, p, 0, "passengers.csv not found");

    char line[512];
    int n = 0, updated = 0;
    while (fgets(line, sizeof line, f)) {
        if (n++ == 0) continue;
        char *cur = line;
        csv_field(&cur);
        char *pnr = csv_field(&cur);
        if (!pnr) continue;
        Passenger *x = pax_by_pnr(w, pnr);
        if (!x) continue;
        csv_field(&cur); csv_field(&cur); csv_field(&cur);   /* name..seat */
        csv_field(&cur);                                     /* bags       */
        char *ci = csv_field(&cur);
        char *se = csv_field(&cur);
        char *bo = csv_field(&cur);
        if (ci) x->checkedIn = atoi(ci);
        if (se) x->security  = atoi(se);
        if (bo) x->boarded   = atoi(bo);
        updated++;
    }
    fclose(f);
    return result(1, p, updated, "Passenger roster restored");
}

/* ==========================================================================
 *  baggage
 * ========================================================================== */

StoreResult store_save_baggage(World *w)
{
    char p[300]; path_of("baggage.csv", p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return result(0, p, 0, "Could not open baggage.csv");

    fprintf(f, "id,tag,pax_id,flight,state,state_name,weight_kg,lane,"
               "threat_score,flagged,inspected,priority\n");
    for (int i = 0; i < w->nBags; i++) {
        Bag *b = &w->bag[i];
        Flight *fl = flight_by_id(w, b->flight);
        fprintf(f, "%d,%s,%d,%s,%d,%s,%.1f,%d,%.4f,%d,%d,%d\n",
                b->id, b->tag, b->pax, fl ? fl->no : "----",
                (int)b->state, bs_name(b->state), b->weight, b->lane,
                b->threatScore, b->threat, b->inspected, b->priority);
    }
    fclose(f);
    return result(1, p, w->nBags, "Baggage register written");
}

StoreResult store_load_baggage(World *w)
{
    char p[300]; path_of("baggage.csv", p, sizeof p);
    FILE *f = fopen(p, "rb");
    if (!f) return result(0, p, 0, "baggage.csv not found");

    char line[512];
    int n = 0, updated = 0;
    while (fgets(line, sizeof line, f)) {
        if (n++ == 0) continue;
        char *cur = line;
        csv_field(&cur);
        char *tag = csv_field(&cur);
        if (!tag) continue;
        Bag *b = bag_by_tag(w, tag);
        if (!b) continue;
        csv_field(&cur); csv_field(&cur);
        char *st = csv_field(&cur);
        if (st) b->state = (BagState)atoi(st);
        updated++;
    }
    fclose(f);
    return result(1, p, updated, "Baggage register restored");
}

/* ==========================================================================
 *  staff + report
 * ========================================================================== */

StoreResult store_save_staff(World *w)
{
    char p[300]; path_of("staff.csv", p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return result(0, p, 0, "Could not open staff.csv");
    fprintf(f, "name,role,shift,on_duty,station\n");
    for (int i = 0; i < w->nStaff; i++) {
        Staff *s = &w->staff[i];
        const char *sh = s->shift == 0 ? "Early" : (s->shift == 1 ? "Late" : "Night");
        fprintf(f, "%s,%s,%s,%d,%d\n", s->name, s->role, sh, s->onDuty, s->station);
    }
    fclose(f);
    return result(1, p, w->nStaff, "Duty roster written");
}

void store_journal(World *w, const char *what)
{
    char p[300]; path_of("journal.log", p, sizeof p);
    FILE *f = fopen(p, "ab");
    if (!f) return;
    char hhmm[8]; fmt_hhmm((int)w->clock, hhmm);
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] SIM %s  %s\r\n",
            lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
            lt->tm_hour, lt->tm_min, lt->tm_sec, hhmm, what);
    fclose(f);
}

StoreResult store_save_all(World *w)
{
    StoreResult a = store_save_flights(w);
    StoreResult b = store_save_passengers(w);
    StoreResult c = store_save_baggage(w);
    StoreResult d = store_save_staff(w);
    int total = a.records + b.records + c.records + d.records;
    store_journal(w, "Full operational snapshot written to disk");
    return result(a.ok && b.ok && c.ok && d.ok, store_root(), total,
                  "All operational records written");
}

StoreResult store_export_report(World *w)
{
    char p[300]; path_of("daily_report.txt", p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return result(0, p, 0, "Could not open daily_report.txt");

    int dep = 0, arr = 0, delayed = 0, cancelled = 0, paxTotal = 0;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *fl = &w->flight[i];
        if (fl->arrival) arr++; else dep++;
        if (fl->delayMin >= 15) delayed++;
        if (fl->state == FS_CANCELLED) cancelled++;
        paxTotal += fl->pax;
    }
    int flagged = 0, loaded = 0;
    for (int i = 0; i < w->nBags; i++) {
        if (w->bag[i].threat) flagged++;
        if (w->bag[i].state >= BG_LOADED) loaded++;
    }
    char hhmm[8]; fmt_hhmm((int)w->clock, hhmm);

    fprintf(f,
      "================================================================\r\n"
      "  AURA  --  DAILY OPERATIONS REPORT\r\n"
      "  Sir Seewoosagur Ramgoolam International Airport (MRU / FIMP)\r\n"
      "  Plaisance, Mauritius\r\n"
      "================================================================\r\n"
      "  Report generated at simulation time %s on %02d/%02d/%04d\r\n\r\n"
      "  MOVEMENTS\r\n"
      "    Departures scheduled .............. %d\r\n"
      "    Arrivals scheduled ............... %d\r\n"
      "    Movements delayed 15 min or more . %d\r\n"
      "    Cancelled ........................ %d\r\n"
      "    On-time performance .............. %.1f%%\r\n\r\n"
      "  PASSENGERS\r\n"
      "    Total seats sold ................. %d\r\n"
      "    Roster records held .............. %d\r\n\r\n"
      "  BAGGAGE\r\n"
      "    Bags in system ................... %d\r\n"
      "    Loaded to aircraft ............... %d\r\n"
      "    Flagged by screening ............. %d\r\n"
      "    Mishandled ....................... %d\r\n\r\n"
      "  AIRFIELD\r\n"
      "    Runway in use .................... %02d\r\n"
      "    Wind ............................. %03d/%02dkt\r\n"
      "    QNH .............................. %.0f hPa\r\n"
      "    Conditions ....................... %s\r\n"
      "    Stands in the register ........... %d\r\n\r\n"
      "================================================================\r\n"
      "  Produced by AURA -- Airport Unified Resource Administration\r\n"
      "  Trois Freres Systems Ltd.\r\n"
      "================================================================\r\n",
      hhmm, w->day, w->month, w->year,
      dep, arr, delayed, cancelled,
      w->nFlights ? 100.f * (float)(w->nFlights - delayed) / w->nFlights : 100.f,
      paxTotal, w->nPax,
      w->nBags, loaded, flagged, w->bagsMishandled,
      w->activeRunway, (int)w->windDir, (int)w->windKt, w->qnh,
      wx_name(w->wxKind), w->nStands);
    fclose(f);
    store_journal(w, "Daily operations report exported");
    return result(1, p, 1, "Daily report exported");
}
