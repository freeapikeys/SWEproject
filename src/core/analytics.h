/* ==========================================================================
 *  AURA :: analytics.h  --  what the day actually looked like
 *
 *  Everything else in the application stores or moves data.  This reads it
 *  back and answers the questions an airport director asks: how many flights,
 *  how many of them were late and by how much, which gate is working hardest,
 *  where are the passengers going, and is the baggage system keeping up.
 *
 *  Nothing is cached.  The report is computed from the live World when it is
 *  asked for, so it can never drift out of step with what the screens show.
 * ========================================================================== */
#ifndef AURA_ANALYTICS_H
#define AURA_ANALYTICS_H

#include "model.h"
#include "ops.h"

#define AN_TOP 6

typedef struct { char label[30]; int value; float pct; } AnRow;

typedef struct {
    /* movements */
    int   flights, arrivals, departures;
    int   cancelled, delayed, onTime, airborne, completed;
    float onTimePct;

    /* punctuality */
    float avgDelayAll;           /* minutes, across every movement          */
    float avgDelayOfLate;        /* minutes, counting only the late ones    */
    int   worstDelay;
    char  worstFlight[8];

    /* passengers */
    int   paxTotal, paxSampled, checkedIn, boarded;
    float loadFactor;            /* seats sold / seats offered              */
    int   assistance, infants, priority;

    /* baggage */
    int   bags, bagsLoaded, bagsInSystem, bagsHeld, bagsMishandled;
    int   bagsOverweight;
    float avgBagWeight;
    float mishandledPer1000;

    /* infrastructure */
    int   gateConflicts, runwayConflicts;
    int   standsUsed, standsTotal;
    int   busiestGate, busiestGateCount;
    float runwayUtilPct;         /* share of the operating day occupied     */
    int   movementsPeakHour, peakHour;

    /* security and queues */
    int   securityAlerts, emergencies;

    /* league tables */
    AnRow topDest[AN_TOP];   int nTopDest;
    AnRow topAirline[AN_TOP];int nTopAirline;
    AnRow topGate[AN_TOP];   int nTopGate;
    AnRow byHour[24];
} Report;

void  an_build(Report *r, World *w);

/* A one-line headline for the top of the screen, phrased for a human. */
void  an_headline(const Report *r, char *out, int cap);

#endif /* AURA_ANALYTICS_H */
