/* ==========================================================================
 *  AURA :: ops.c  --  flight, gate and runway management
 * ========================================================================== */

#include "ops.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 *  flight CRUD
 * ========================================================================== */

int ops_flight_add(World *w, const FlightSpec *s)
{
    if (!s || w->nFlights >= MAX_FLIGHTS) return 0;
    if (!s->no[0]) return 0;
    if (s->schedMin < 0 || s->schedMin >= 1440) return 0;
    if (flight_by_no(w, s->no)) return 0;             /* already flying today */

    int id = 1;
    for (int i = 0; i < w->nFlights; i++)
        if (w->flight[i].id >= id) id = w->flight[i].id + 1;

    Flight *f = &w->flight[w->nFlights++];
    memset(f, 0, sizeof *f);
    f->id      = id;
    f->airline = (s->airline >= 0 && s->airline < w->nAirlines) ? s->airline : 0;
    f->acType  = (s->acType  >= 0 && s->acType  < w->nActypes)  ? s->acType  : 0;
    f->airport = (s->airport >= 0 && s->airport < w->nAirports) ? s->airport : 0;
    f->arrival = s->arrival ? 1 : 0;
    snprintf(f->no, sizeof f->no, "%s", s->no);
    f->schedMin = s->schedMin;
    f->estMin   = s->schedMin;
    f->state    = FS_SCHEDULED;
    f->stand    = -1;
    f->gate     = 0;
    f->phase    = AP_NONE;

    const AcType *t = &w->actype[f->acType];
    f->paxCap = t->seats;
    f->pax    = (int)(t->seats * 0.78f);
    f->bags   = (int)(f->pax * 0.9f);
    snprintf(f->reg, sizeof f->reg, "3B-N%c%c",
             'A' + (char)(id % 26), 'A' + (char)((id * 7) % 26));
    if (f->arrival)
        snprintf(f->belt, sizeof f->belt, "%s", t->widebody ? "04-05" : "03");
    else {
        f->nDesks = t->widebody ? 4 : 2;
        int base = 1 + (w->nFlights * 3) % 20;
        for (int i = 0; i < f->nDesks; i++) f->desks[i] = base + i;
    }

    /*  A new flight is put on a gate straight away if one is genuinely free.
     *  Leaving it unassigned and hoping somebody notices is how a gate clash
     *  gets discovered at the gate.                                        */
    int g = ops_gate_suggest(w, id);
    if (g > 0) ops_flight_gate(w, id, g);

    world_log(w, LG_OK, "%s created -- %s %s, scheduled %02d:%02d",
              f->no, f->arrival ? "arrival from" : "departure to",
              w->airport[f->airport].city, f->schedMin / 60, f->schedMin % 60);
    return id;
}

int ops_flight_cancel(World *w, int id)
{
    Flight *f = flight_by_id(w, id);
    if (!f) return 0;
    if (f->state == FS_CANCELLED) return 0;
    /* an aeroplane that has already gone cannot be cancelled */
    if (f->state == FS_DEPARTED || f->state == FS_ONBLOCK) return 0;

    f->state = FS_CANCELLED;
    f->phase = AP_NONE;
    /* the gate and stand go back into the pool immediately */
    f->stand = -1;
    int gate = f->gate;
    f->gate  = 0;
    world_log(w, LG_WARN, "%s cancelled -- gate %d released", f->no, gate);
    return 1;
}

int ops_flight_delay(World *w, int id, int minutes)
{
    Flight *f = flight_by_id(w, id);
    if (!f || f->state == FS_CANCELLED) return 0;
    f->delayMin += minutes;
    if (f->delayMin < 0) f->delayMin = 0;
    f->estMin = f->schedMin + f->delayMin;
    while (f->estMin >= 1440) f->estMin -= 1440;
    while (f->estMin < 0)     f->estMin += 1440;
    if (f->delayMin >= 15 && f->state == FS_SCHEDULED) f->state = FS_DELAYED;
    world_log(w, f->delayMin >= 30 ? LG_WARN : LG_INFO,
              "%s retimed to %02d:%02d (%+d min)", f->no,
              f->estMin / 60, f->estMin % 60, minutes);
    return 1;
}

int ops_flight_state(World *w, int id, FlightState st)
{
    Flight *f = flight_by_id(w, id);
    if (!f) return 0;
    if (st < 0 || st >= FS_COUNT) return 0;
    if (f->state == st) return 0;
    f->state = st;
    world_log(w, LG_INFO, "%s status set to %s", f->no, fs_name(st));
    return 1;
}

int ops_flight_gate(World *w, int id, int gate)
{
    Flight *f = flight_by_id(w, id);
    if (!f) return 0;
    if (gate < 1 || gate > OPS_MAX_GATE) return 0;
    f->gate = gate;
    /*  Move the aeroplane to a stand that serves this gate, so the airfield
     *  view and the board do not disagree about where it is.               */
    for (int s = 0; s < w->nStands; s++)
        if (w->stand[s].gate == gate) { f->stand = s; break; }
    return 1;
}

int ops_flight_stand(World *w, int id, int stand)
{
    Flight *f = flight_by_id(w, id);
    if (!f || stand < 0 || stand >= w->nStands) return 0;
    f->stand = stand;
    if (w->stand[stand].gate) f->gate = w->stand[stand].gate;
    return 1;
}

/* ------------------------------------------------------------------ search */

static int ci_find(const char *hay, const char *needle)
{
    if (!needle || !*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *a = h, *b = needle;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) break;
            a++; b++;
        }
        if (!*b) return 1;
    }
    return 0;
}

int ops_flight_find(World *w, const char *query, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < w->nFlights && n < max; i++) {
        Flight *f = &w->flight[i];
        if (!query || !*query) { out[n++] = i; continue; }
        if (ci_find(f->no, query) ||
            ci_find(f->reg, query) ||
            ci_find(w->airport[f->airport].city, query) ||
            ci_find(w->airport[f->airport].iata, query) ||
            ci_find(w->airport[f->airport].country, query) ||
            ci_find(w->airline[f->airline].name, query) ||
            ci_find(w->actype[f->acType].code, query) ||
            ci_find(fs_name(f->state), query))
            out[n++] = i;
    }
    /* soonest first */
    for (int i = 1; i < n; i++) {
        int k = out[i], j = i - 1;
        while (j >= 0 && w->flight[out[j]].estMin > w->flight[k].estMin) {
            out[j+1] = out[j]; j--;
        }
        out[j+1] = k;
    }
    return n;
}

/* ==========================================================================
 *  gates
 * ========================================================================== */

/*  A departure holds its gate from the start of boarding until pushback; an
 *  arrival from touchdown until the cabin is clear.  These are the windows
 *  that must not overlap, not the scheduled times -- two flights ten minutes
 *  apart on paper can sit on the same gate for forty minutes each.          */
void ops_gate_window(const World *w, const Flight *f, int *from, int *to)
{
    int t = f->estMin;
    if (f->arrival) {
        int stay = w->actype[f->acType].widebody ? 55 : 35;
        *from = t - 5;
        *to   = t + stay;
    } else {
        int board = w->actype[f->acType].widebody ? 55 : 40;
        *from = t - board;
        *to   = t + 10;
    }
}

static int overlap_of(int a0, int a1, int b0, int b1)
{
    int lo = a0 > b0 ? a0 : b0;
    int hi = a1 < b1 ? a1 : b1;
    return hi > lo ? hi - lo : 0;
}

static int gate_active(const Flight *f)
{
    return f->gate > 0 && f->state != FS_CANCELLED &&
           f->state != FS_DEPARTED;
}

int ops_gate_conflicts(const World *w, GateConflict *out, int max)
{
    int n = 0;
    for (int i = 0; i < w->nFlights && n < max; i++) {
        const Flight *a = &w->flight[i];
        if (!gate_active(a)) continue;
        int a0, a1;
        ops_gate_window(w, a, &a0, &a1);

        for (int j = i + 1; j < w->nFlights && n < max; j++) {
            const Flight *b = &w->flight[j];
            if (!gate_active(b) || b->gate != a->gate) continue;
            /*  The two halves of one turnaround are the same aeroplane on
             *  the same gate, which is not a clash -- it is the point.     */
            if (strcmp(a->reg, b->reg) == 0) continue;

            int b0, b1;
            ops_gate_window(w, b, &b0, &b1);
            int ov = overlap_of(a0, a1, b0, b1);
            if (ov <= 0) continue;

            out[n].a       = a->id;
            out[n].b       = b->id;
            out[n].gate    = a->gate;
            out[n].overlap = ov;
            n++;
        }
    }
    return n;
}

int ops_gate_free(const World *w, int gate, int from, int to, int except)
{
    if (gate < 1 || gate > OPS_MAX_GATE) return 0;
    for (int i = 0; i < w->nFlights; i++) {
        const Flight *f = &w->flight[i];
        if (f->id == except || !gate_active(f) || f->gate != gate) continue;
        int f0, f1;
        ops_gate_window(w, f, &f0, &f1);
        /* ten minutes of turnround margin between two aeroplanes */
        if (overlap_of(from - 10, to + 10, f0, f1) > 0) return 0;
    }
    return 1;
}

int ops_gate_load(const World *w, int gate)
{
    int n = 0;
    for (int i = 0; i < w->nFlights; i++)
        if (w->flight[i].gate == gate && w->flight[i].state != FS_CANCELLED) n++;
    return n;
}

int ops_gate_occupant(const World *w, int gate, int from, int to, int except)
{
    for (int i = 0; i < w->nFlights; i++) {
        const Flight *f = &w->flight[i];
        if (f->id == except || !gate_active(f) || f->gate != gate) continue;
        int f0, f1;
        ops_gate_window(w, f, &f0, &f1);
        if (overlap_of(from - 10, to + 10, f0, f1) > 0) return f->id;
    }
    return 0;
}

/*  Pick a replacement gate.  Free is the hard requirement; among the free
 *  ones prefer the least busy, so the day's load stays spread instead of
 *  everything piling onto whichever gate has the lowest number.            */
int ops_gate_suggest(const World *w, int flightId)
{
    const Flight *f = NULL;
    for (int i = 0; i < w->nFlights; i++)
        if (w->flight[i].id == flightId) { f = &w->flight[i]; break; }
    if (!f) return -1;

    int from, to;
    ops_gate_window(w, f, &from, &to);

    int best = -1, bestLoad = 1 << 30;
    for (int g = 1; g <= OPS_MAX_GATE; g++) {
        if (g == f->gate) continue;
        if (!ops_gate_free(w, g, from, to, f->id)) continue;
        /* a widebody needs a stand that can take it */
        int fits = 0;
        for (int s = 0; s < w->nStands; s++)
            if (w->stand[s].gate == g &&
                w->stand[s].maxSpan >= w->actype[f->acType].wingspan) {
                fits = 1; break;
            }
        if (!fits) continue;

        int load = ops_gate_load(w, g);
        if (load < bestLoad) { bestLoad = load; best = g; }
    }
    return best;
}

/* ==========================================================================
 *  delay ripple
 *
 *  A flight running late does not only affect itself.  It sits on its gate
 *  past the time the next aeroplane needs it, and that second flight is now
 *  in trouble through no fault of its own.  Catching that automatically, and
 *  naming a gate that would clear it, is the whole job.
 * ========================================================================== */

int ops_delay_impact(const World *w, DelayImpact *out, int max)
{
    GateConflict gc[64];
    int nc = ops_gate_conflicts(w, gc, 64);
    int n  = 0;

    for (int i = 0; i < nc && n < max; i++) {
        const Flight *a = flight_by_id((World *)w, gc[i].a);
        const Flight *b = flight_by_id((World *)w, gc[i].b);
        if (!a || !b) continue;

        /*  Attribute the clash to whichever of the two is actually late.
         *  If neither is, it is a planning error rather than a knock-on and
         *  the later flight is treated as the one to move.                 */
        const Flight *late, *victim;
        if (a->delayMin > b->delayMin)      { late = a; victim = b; }
        else if (b->delayMin > a->delayMin) { late = b; victim = a; }
        else if (a->estMin <= b->estMin)    { late = a; victim = b; }
        else                                { late = b; victim = a; }

        int sug = ops_gate_suggest(w, victim->id);

        out[n].lateId    = late->id;
        out[n].victimId  = victim->id;
        out[n].gate      = gc[i].gate;
        out[n].suggested = sug;
        out[n].overlap   = gc[i].overlap;

        if (late->delayMin > 0)
            snprintf(out[n].text, sizeof out[n].text,
                     "%s is %d min late and holds gate %d for a further %d min;"
                     " %s needs it.",
                     late->no, late->delayMin, gc[i].gate, gc[i].overlap,
                     victim->no);
        else
            snprintf(out[n].text, sizeof out[n].text,
                     "%s and %s are both planned on gate %d and overlap by"
                     " %d min.", late->no, victim->no, gc[i].gate,
                     gc[i].overlap);
        n++;
    }
    return n;
}

/* ==========================================================================
 *  passengers
 * ========================================================================== */

int ops_seat_assign(World *w, int flightId, int assist, char *out)
{
    Flight *f = flight_by_id(w, flightId);
    if (!f) return 0;

    const AcType *t = &w->actype[f->acType];
    int rows = t->widebody ? 58 : 32;
    const char *cols = t->widebody ? "ABCDEFGHJ" : "ABCDEF";
    int nc = (int)strlen(cols);

    /*  Passengers who have asked for assistance are seated at the front and
     *  on the aisle: it is a shorter walk, and a wheelchair cannot be taken
     *  down the row.  Everybody else is filled from the front backwards,
     *  which is also how the load sheet wants the weight distributed.      */
    for (int r = 1; r <= rows; r++) {
        for (int ci = 0; ci < nc; ci++) {
            int isAisle = (ci == 2 || ci == 3);
            if (assist && !isAisle && r <= 6) continue;

            char seat[5];
            snprintf(seat, sizeof seat, "%d%c", r, cols[ci]);

            int taken = 0;
            for (int i = 0; i < w->nPax; i++)
                if (w->pax[i].flight == flightId &&
                    strcmp(w->pax[i].seat, seat) == 0) { taken = 1; break; }
            if (taken) continue;

            snprintf(out, 5, "%s", seat);
            return 1;
        }
    }
    return 0;                                   /* the aircraft is full */
}

int ops_pax_register(World *w, const char *name, int flightId)
{
    if (w->nPax >= MAX_PASSENGERS) return 0;
    if (!name || !name[0]) return 0;
    Flight *f = flight_by_id(w, flightId);
    if (!f || f->state == FS_CANCELLED) return 0;

    int id = 1;
    for (int i = 0; i < w->nPax; i++)
        if (w->pax[i].id >= id) id = w->pax[i].id + 1;

    Passenger *p = &w->pax[w->nPax++];
    memset(p, 0, sizeof *p);
    p->id     = id;
    p->flight = flightId;
    snprintf(p->name, sizeof p->name, "%s", name);

    /* a booking reference nobody else on the day already has */
    for (int attempt = 0; attempt < 40; attempt++) {
        for (int c = 0; c < 6; c++) {
            w->rng = w->rng * 1664525u + 1013904223u;
            int v = (int)((w->rng >> 16) % 36u);
            p->pnr[c] = (char)(v < 26 ? 'A' + v : '0' + (v - 26));
        }
        p->pnr[6] = 0;
        int clash = 0;
        for (int i = 0; i < w->nPax - 1; i++)
            if (strcmp(w->pax[i].pnr, p->pnr) == 0) { clash = 1; break; }
        if (!clash) break;
    }

    if (!ops_seat_assign(w, flightId, 0, p->seat)) {
        w->nPax--;                              /* no seat, no booking */
        world_log(w, LG_WARN, "%s is full -- %s could not be booked",
                  f->no, name);
        return 0;
    }
    p->nationality = 0;

    world_log(w, LG_OK, "%s booked onto %s, seat %s, reference %s",
              p->name, f->no, p->seat, p->pnr);
    return id;
}

int ops_pax_checkin(World *w, int paxId, int bags)
{
    Passenger *p = NULL;
    for (int i = 0; i < w->nPax; i++)
        if (w->pax[i].id == paxId) { p = &w->pax[i]; break; }
    if (!p || p->checkedIn) return 0;

    Flight *f = flight_by_id(w, p->flight);
    if (!f || f->state == FS_CANCELLED) return 0;
    /* bag drop closes an hour out; after that it is a gate matter */
    if (f->state >= FS_CLOSED && f->state <= FS_DEPARTED) return 0;

    if (!p->seat[0]) ops_seat_assign(w, p->flight, p->assist, p->seat);
    p->checkedIn = 1;
    f->checkedIn++;

    if (bags < 0) bags = 0;
    if (bags > 3) bags = 3;
    p->bags = bags;

    for (int b = 0; b < bags && w->nBags < MAX_BAGS; b++) {
        Bag *g = &w->bag[w->nBags++];
        memset(g, 0, sizeof *g);
        g->id     = w->nBags;
        g->pax    = p->id;
        g->flight = f->id;
        g->state  = BG_CHECKIN;
        w->rng = w->rng * 1664525u + 1013904223u;
        int serial = 100000 + (int)((w->rng >> 8) % 900000u);
        /*  Both the tag and the airline code live inside World, so the
         *  compiler cannot prove they do not overlap.  Copying the two
         *  characters out first makes it obvious that they do not.       */
        char code[4];
        code[0] = w->airline[f->airline].iata[0];
        code[1] = w->airline[f->airline].iata[1];
        code[2] = 0;
        snprintf(g->tag, sizeof g->tag, "%.2s%06d", code, serial);
        g->weight     = 8.f + (float)((w->rng >> 5) % 220u) * 0.1f;
        g->lane       = (int)((w->rng >> 3) % 4u);
        g->colour     = (int)((w->rng >> 11) % 6u);
        g->priority   = p->loyalty >= 2;
        g->releaseMin = w->clock;
        f->bags++;
        w->totalBagsToday++;
    }

    world_log(w, LG_OK, "%s checked in for %s, seat %s, %d bag%s",
              p->name, f->no, p->seat, bags, bags == 1 ? "" : "s");
    return 1;
}

int ops_pax_find(World *w, const char *query, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < w->nPax && n < max; i++) {
        Passenger *p = &w->pax[i];
        if (!query || !*query) { out[n++] = i; continue; }
        Flight *f = flight_by_id(w, p->flight);
        if (ci_find(p->name, query) || ci_find(p->pnr, query) ||
            ci_find(p->seat, query) || (f && ci_find(f->no, query)))
            out[n++] = i;
    }
    return n;
}

/* ==========================================================================
 *  the runway
 *
 *  Plaisance has one: 14/32, 3,390 m.  Everything that lands and everything
 *  that departs goes through it, so it is the one resource that genuinely
 *  cannot be shared, and the separation between two movements is what limits
 *  how many the airport can handle in an hour.
 * ========================================================================== */

static int uses_runway(const Flight *f)
{
    if (f->state == FS_CANCELLED) return 0;
    return 1;
}

int ops_runway_slots(const World *w, RunwaySlot *out, int max)
{
    int n = 0;
    for (int i = 0; i < w->nFlights && n < max; i++) {
        const Flight *f = &w->flight[i];
        if (!uses_runway(f)) continue;
        out[n].flightId = f->id;
        out[n].arrival  = f->arrival;
        /*  An arrival occupies the runway from short final to vacating; a
         *  departure from lining up to airborne.  Both get the mandated
         *  separation added behind them.                                   */
        out[n].fromMin = f->estMin - OPS_RWY_OCC;
        out[n].toMin   = f->estMin + OPS_RWY_OCC + OPS_RWY_SEP;
        n++;
    }
    for (int i = 1; i < n; i++) {
        RunwaySlot t = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].fromMin > t.fromMin) { out[j+1] = out[j]; j--; }
        out[j+1] = t;
    }
    return n;
}

int ops_runway_occupied(const World *w, float atMin, int *byFlight)
{
    RunwaySlot s[MAX_FLIGHTS];
    int n = ops_runway_slots(w, s, MAX_FLIGHTS);
    for (int i = 0; i < n; i++) {
        if (atMin >= (float)s[i].fromMin && atMin <= (float)s[i].toMin) {
            if (byFlight) *byFlight = s[i].flightId;
            return 1;
        }
    }
    if (byFlight) *byFlight = 0;
    return 0;
}

int ops_runway_conflicts(const World *w, int *pairs, int maxPairs)
{
    RunwaySlot s[MAX_FLIGHTS];
    int n = ops_runway_slots(w, s, MAX_FLIGHTS);
    int k = 0;
    for (int i = 0; i + 1 < n && k < maxPairs; i++) {
        if (s[i+1].fromMin < s[i].toMin) {
            pairs[k*2]     = s[i].flightId;
            pairs[k*2 + 1] = s[i+1].flightId;
            k++;
        }
    }
    return k;
}

int ops_runway_next_free(const World *w, int fromMin)
{
    RunwaySlot s[MAX_FLIGHTS];
    int n = ops_runway_slots(w, s, MAX_FLIGHTS);
    int t = fromMin;
    for (int i = 0; i < n; i++) {
        if (t >= s[i].fromMin && t <= s[i].toMin) t = s[i].toMin + 1;
    }
    return t;
}
