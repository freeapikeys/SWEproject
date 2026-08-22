/* ==========================================================================
 *  AURA :: analytics.c  --  what the day actually looked like
 * ========================================================================== */

#include "analytics.h"
#include "sim.h"
#include <stdio.h>
#include <string.h>

/*  Sort a tally into a league table, biggest first, and keep the top few.
 *  Selection rather than a full sort: with at most 34 destinations and six
 *  places to fill, a comparison sort would be more code for less clarity.  */
static int top_of(const int *count, int n, const char *const *label,
                  AnRow *out, int want, int total)
{
    int used[64];
    memset(used, 0, sizeof used);
    if (n > 64) n = 64;

    int k = 0;
    for (int slot = 0; slot < want; slot++) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (used[i] || count[i] <= 0) continue;
            if (best < 0 || count[i] > count[best]) best = i;
        }
        if (best < 0) break;
        used[best] = 1;
        snprintf(out[k].label, sizeof out[k].label, "%s", label[best]);
        out[k].value = count[best];
        out[k].pct   = total ? (float)count[best] * 100.f / (float)total : 0.f;
        k++;
    }
    return k;
}

void an_build(Report *r, World *w)
{
    memset(r, 0, sizeof *r);

    r->standsTotal = w->nStands;
    r->securityAlerts = w->securityAlerts;

    int destCount[MAX_AIRPORTS];
    int alCount  [MAX_AIRLINES];
    int gateCount[OPS_MAX_GATE + 1];
    int standUsed[MAX_STANDS];
    memset(destCount, 0, sizeof destCount);
    memset(alCount,   0, sizeof alCount);
    memset(gateCount, 0, sizeof gateCount);
    memset(standUsed, 0, sizeof standUsed);

    int   seatsOffered = 0, seatsSold = 0;
    int   delaySum = 0, lateSum = 0;

    /* ---------------------------------------------------------- movements */
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        r->flights++;
        if (f->arrival) r->arrivals++; else r->departures++;

        if (f->state == FS_CANCELLED) { r->cancelled++; continue; }

        if (f->delayMin >= 15) {
            r->delayed++;
            lateSum += f->delayMin;
        } else {
            r->onTime++;
        }
        delaySum += f->delayMin;

        if (f->delayMin > r->worstDelay) {
            r->worstDelay = f->delayMin;
            snprintf(r->worstFlight, sizeof r->worstFlight, "%s", f->no);
        }

        if (f->state >= FS_PUSHBACK && f->state <= FS_LANDED) r->airborne++;
        if (f->state == FS_DEPARTED || f->state == FS_ONBLOCK) r->completed++;

        seatsOffered += f->paxCap;
        seatsSold    += f->pax;
        r->paxTotal  += f->pax;

        if (f->airport >= 0 && f->airport < w->nAirports) destCount[f->airport]++;
        if (f->airline >= 0 && f->airline < w->nAirlines) alCount[f->airline]++;
        if (f->gate > 0 && f->gate <= OPS_MAX_GATE)       gateCount[f->gate]++;
        if (f->stand >= 0 && f->stand < w->nStands)       standUsed[f->stand] = 1;

        int h = f->estMin / 60;
        if (h >= 0 && h < 24) {
            r->byHour[h].value++;
            snprintf(r->byHour[h].label, sizeof r->byHour[h].label, "%02d", h);
        }
    }

    int counted = r->flights - r->cancelled;
    r->avgDelayAll    = counted   ? (float)delaySum / (float)counted   : 0.f;
    r->avgDelayOfLate = r->delayed? (float)lateSum  / (float)r->delayed: 0.f;
    r->onTimePct      = counted   ? (float)r->onTime * 100.f / (float)counted : 0.f;
    r->loadFactor     = seatsOffered
                      ? (float)seatsSold * 100.f / (float)seatsOffered : 0.f;

    for (int i = 0; i < w->nStands; i++) if (standUsed[i]) r->standsUsed++;

    for (int h = 0; h < 24; h++) {
        if (!r->byHour[h].label[0])
            snprintf(r->byHour[h].label, sizeof r->byHour[h].label, "%02d", h);
        if (r->byHour[h].value > r->movementsPeakHour) {
            r->movementsPeakHour = r->byHour[h].value;
            r->peakHour = h;
        }
    }

    /* --------------------------------------------------------- passengers */
    r->paxSampled = w->nPax;
    for (int i = 0; i < w->nPax; i++) {
        Passenger *p = &w->pax[i];
        if (p->checkedIn) r->checkedIn++;
        if (p->boarded)   r->boarded++;
        if (p->wheelchair) r->assistance++;
        if (p->infant)     r->infants++;
        if (p->fastTrack)  r->priority++;
    }

    /* ------------------------------------------------------------ baggage */
    float wsum = 0.f;
    for (int i = 0; i < w->nBags; i++) {
        Bag *b = &w->bag[i];
        r->bags++;
        wsum += b->weight;
        if (b->weight > 23.f)          r->bagsOverweight++;
        if (b->state == BG_LOADED)     r->bagsLoaded++;
        if (b->state == BG_HELD)       r->bagsHeld++;
        if (b->state == BG_MISHANDLED) r->bagsMishandled++;
        if (bag_in_system(b->state))   r->bagsInSystem++;
    }
    r->avgBagWeight = r->bags ? wsum / (float)r->bags : 0.f;
    r->mishandledPer1000 = r->bags
        ? (float)r->bagsMishandled * 1000.f / (float)r->bags : 0.f;

    /* ----------------------------------------------------- infrastructure */
    GateConflict gc[64];
    r->gateConflicts = ops_gate_conflicts(w, gc, 64);
    int pairs[64];
    r->runwayConflicts = ops_runway_conflicts(w, pairs, 32);

    for (int g = 1; g <= OPS_MAX_GATE; g++)
        if (gateCount[g] > r->busiestGateCount) {
            r->busiestGateCount = gateCount[g];
            r->busiestGate = g;
        }

    /*  Runway utilisation: the share of the operating day the single runway
     *  is actually occupied.  Plaisance runs one runway, so this is the hard
     *  ceiling on how much the airport can grow without building another. */
    RunwaySlot slots[MAX_FLIGHTS];
    int ns = ops_runway_slots(w, slots, MAX_FLIGHTS);
    int occupied = 0;
    for (int i = 0; i < ns; i++) {
        int len = slots[i].toMin - slots[i].fromMin;
        if (len > 0) occupied += len;
    }
    r->runwayUtilPct = (float)occupied * 100.f / (19.f * 60.f);  /* 05:00-24:00 */
    if (r->runwayUtilPct > 100.f) r->runwayUtilPct = 100.f;

    /* -------------------------------------------------------- the tables */
    const char *destLabel[MAX_AIRPORTS];
    for (int i = 0; i < w->nAirports; i++) destLabel[i] = w->airport[i].city;
    r->nTopDest = top_of(destCount, w->nAirports, destLabel,
                         r->topDest, AN_TOP, counted);

    const char *alLabel[MAX_AIRLINES];
    for (int i = 0; i < w->nAirlines; i++) alLabel[i] = w->airline[i].name;
    r->nTopAirline = top_of(alCount, w->nAirlines, alLabel,
                            r->topAirline, AN_TOP, counted);

    static char gateName[OPS_MAX_GATE + 1][8];
    const char *gateLabel[OPS_MAX_GATE + 1];
    int gc2[OPS_MAX_GATE + 1];
    for (int g = 0; g <= OPS_MAX_GATE; g++) {
        snprintf(gateName[g], sizeof gateName[g], "Gate %d", g);
        gateLabel[g] = gateName[g];
        gc2[g] = (g == 0) ? 0 : gateCount[g];
    }
    r->nTopGate = top_of(gc2, OPS_MAX_GATE + 1, gateLabel,
                         r->topGate, AN_TOP, counted);
}

void an_headline(const Report *r, char *out, int cap)
{
    if (r->cancelled > 0 && r->onTimePct < 70.f)
        snprintf(out, (size_t)cap,
                 "A difficult day: %.0f%% on time, %d delayed and %d cancelled.",
                 r->onTimePct, r->delayed, r->cancelled);
    else if (r->gateConflicts > 0)
        snprintf(out, (size_t)cap,
                 "%.0f%% on time, but %d gate clash%s need resolving.",
                 r->onTimePct, r->gateConflicts,
                 r->gateConflicts == 1 ? "" : "es");
    else if (r->onTimePct >= 90.f)
        snprintf(out, (size_t)cap,
                 "A good day: %.0f%% on time across %d movements, no gate"
                 " clashes.", r->onTimePct, r->flights);
    else
        snprintf(out, (size_t)cap,
                 "%.0f%% on time across %d movements, average delay %.0f min.",
                 r->onTimePct, r->flights, r->avgDelayAll);
}
