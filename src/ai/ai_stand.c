/* ==========================================================================
 *  AURA :: ai_stand.c   --   stand and gate allocation
 *
 *  Assigning aircraft to stands is a constrained assignment problem, and at
 *  Plaisance it is a tight one: eight contact stands, a mixed wide-body and
 *  regional fleet, and an evening bank where half the long-haul operation
 *  wants to be on a pier at the same time.
 *
 *  Exhaustive search is out (36 movements over 18 stands), so this uses
 *  simulated annealing: start from the current plan, perturb one assignment
 *  at a time, always accept an improvement and accept a worsening move with
 *  probability exp(-delta / T) while the temperature is still high.  That is
 *  what lets it climb out of the local optimum a greedy allocator falls into.
 * ========================================================================== */

#include "ai.h"
#include "../engine/anim.h"
#include <math.h>
#include <string.h>

/* the stand is blocked from this many minutes before to after the movement */
static void occupancy(World *w, Flight *f, float *from, float *to)
{
    /* Realistic stand occupancy at Plaisance: an arrival holds the stand for
     * the disembark, clean and offload; a departure from the start of loading
     * to pushback.  These were originally far longer than any turnaround
     * actually takes, which made the apron look full when it was not. */
    if (f->arrival) { *from = (float)f->estMin -  5.f; *to = (float)f->estMin + 75.f; }
    else            { *from = (float)f->estMin - 80.f; *to = (float)f->estMin + 12.f; }
}

static int compatible(World *w, Flight *f, int standIdx)
{
    if (standIdx < 0 || standIdx >= w->nStands) return 0;
    Stand *s = &w->stand[standIdx];
    if (s->kind == ST_CARGO) return 0;
    return w->actype[f->acType].wingspan <= s->maxSpan + 0.01f;
}

/* Hard violations are counted, not priced.  The search treats them
 * lexicographically: a plan with fewer impossible assignments always beats
 * one with more, whatever the soft cost says.  Pricing them instead lets a
 * large enough pile of soft savings buy a double-booked stand, which is
 * exactly the trade an annealer will find if you let it. */
int ai_stand_hard(World *w, const int *assign)
{
    int hard = 0;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        int si = assign[i];
        if (si < 0) { hard++; continue; }
        if (w->actype[f->acType].wingspan > w->stand[si].maxSpan + 0.01f) hard++;

        float a0, a1;
        occupancy(w, f, &a0, &a1);
        for (int j = i + 1; j < w->nFlights; j++) {
            if (assign[j] != si) continue;
            if (strcmp(w->flight[j].reg, f->reg) == 0) continue;
            float b0, b1;
            occupancy(w, &w->flight[j], &b0, &b1);
            if (a0 < b1 && b0 < a1) hard++;
        }
    }
    return hard;
}

float ai_stand_cost(World *w, const int *assign, int *conflictsOut,
                    int *contactOut, float *walkOut)
{
    float cost = 0.f;
    int conflicts = 0, contact = 0;
    float walkSum = 0.f;
    int   walkN = 0;
    const float TERMINAL_X = 520.f;      /* centre of the terminal frontage */

    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        int si = assign[i];
        if (si < 0) { cost += 500.f; continue; }
        Stand *s = &w->stand[si];

        /* Hard constraints are priced so that no combination of soft savings
         * can ever pay for one.  A double-booked stand is not a slightly worse
         * plan, it is an impossible one. */
        if (w->actype[f->acType].wingspan > s->maxSpan + 0.01f) cost += 6000.f;


        /* passengers should not be bussed unless there is no alternative */
        if (s->kind != ST_CONTACT) cost += 12.f + (float)f->pax * 0.06f;
        else                       contact++;

        /* walking distance from the stand to the centre of the terminal */
        float walk = fabsf(s->x - TERMINAL_X) * 0.45f;
        walkSum += walk; walkN++;
        cost += walk * 0.05f;

        /* wide-body on a stand sized for one is wasteful if smaller fits */
        if (!w->actype[f->acType].widebody && s->maxSpan >= 60.f) cost += 9.f;

        /* Two aircraft cannot share a stand at the same time -- but an
         * inbound and its outbound are one aeroplane, and their windows are
         * meant to overlap.  Counting that as a clash makes the optimiser
         * believe a perfectly good plan is broken, and it then trades real
         * quality away trying to "fix" it. */
        float a0, a1;
        occupancy(w, f, &a0, &a1);
        for (int j = i + 1; j < w->nFlights; j++) {
            if (assign[j] != si) continue;
            if (strcmp(w->flight[j].reg, f->reg) == 0) {
                int gr = f->arrival ? (w->flight[j].estMin - f->estMin)
                                    : (f->estMin - w->flight[j].estMin);
                if (gr > 0 && gr <= 170) continue;     /* one turnaround    */
            }
            float b0, b1;
            occupancy(w, &w->flight[j], &b0, &b1);
            if (a0 < b1 && b0 < a1) { cost += 5000.f; conflicts++; }
        }

        /* keep an inbound and its outbound on the same stand -- a tow costs
         * a tug, a crew and twenty minutes */
        for (int j = 0; j < w->nFlights; j++) {
            if (j == i) continue;
            Flight *g = &w->flight[j];
            if (g->arrival == f->arrival) continue;
            if (strcmp(g->reg, f->reg) != 0) continue;
            int ground = f->arrival ? (g->estMin - f->estMin)
                                    : (f->estMin - g->estMin);
            if (ground > 0 && ground <= 170 &&
                assign[j] >= 0 && assign[j] != si) cost += 130.f;
            break;
        }
    }

    if (conflictsOut) *conflictsOut = conflicts;
    if (contactOut)   *contactOut   = contact;
    if (walkOut)      *walkOut      = walkN ? walkSum / walkN : 0.f;
    return cost;
}

void ai_stand_optimise(StandPlan *p, World *w, int iterations)
{
    if (iterations < 200) iterations = 200;
    memset(p, 0, sizeof *p);

    if (w->nFlights <= 0) { p->ready = 0; return; }

    int cur[MAX_FLIGHTS], best[MAX_FLIGHTS], incoming[MAX_FLIGHTS];
    for (int i = 0; i < MAX_FLIGHTS; i++) cur[i] = -1;
    for (int i = 0; i < w->nFlights; i++) cur[i] = w->flight[i].stand;
    memcpy(incoming, cur, sizeof(int) * (size_t)w->nFlights);

    /* Record the incoming plan's cost before anything is touched, so the
     * improvement reported to the user is measured against what they had. */
    p->startCost = ai_stand_cost(w, cur, NULL, NULL, NULL);

    /* Anything unassigned or impossible has to start somewhere legal.  Taking
     * the *first* stand that fits looks harmless but seeds the search with
     * double bookings, and since the search will not accept a move that makes
     * feasibility worse, it can never dig itself back out.  Choose the stand
     * that adds the fewest clashes instead. */
    uint32_t rng = 0x5A11D0Cu ^ (uint32_t)w->nFlights;
    for (int i = 0; i < w->nFlights; i++) {
        if (compatible(w, &w->flight[i], cur[i])) continue;

        int   bestS = -1, bestClash = 1 << 30;
        float a0, a1;
        occupancy(w, &w->flight[i], &a0, &a1);

        for (int s = 0; s < w->nStands; s++) {
            if (!compatible(w, &w->flight[i], s)) continue;
            int clash = 0;
            for (int j = 0; j < w->nFlights; j++) {
                if (j == i || cur[j] != s) continue;
                if (strcmp(w->flight[j].reg, w->flight[i].reg) == 0) {
                    int gr = w->flight[i].arrival
                           ? (w->flight[j].estMin - w->flight[i].estMin)
                           : (w->flight[i].estMin - w->flight[j].estMin);
                    if (gr > 0 && gr <= 170) continue;
                }
                float b0, b1;
                occupancy(w, &w->flight[j], &b0, &b1);
                if (a0 < b1 && b0 < a1) clash++;
            }
            if (clash < bestClash) { bestClash = clash; bestS = s; }
            if (clash == 0) break;
        }
        if (bestS >= 0) cur[i] = bestS;
    }

    int cf, ct; float wk;
    float curCost = ai_stand_cost(w, cur, &cf, &ct, &wk);
    float bestCost = curCost;
    int   curHard  = ai_stand_hard(w, cur);
    int   bestHard = curHard;
    memcpy(best, cur, sizeof(int) * (size_t)w->nFlights);

    float T0 = 240.f, T1 = 0.6f;

    for (int it = 0; it < iterations; it++) {
        float frac = (float)it / (float)iterations;
        float T = T0 * powf(T1 / T0, frac);          /* geometric cooling   */

        /* Two move types.  A relocation moves one aircraft to a free stand;
         * a swap exchanges two aircraft.  Relocation alone cannot escape the
         * case where two movements each want the stand the other is on, which
         * is the most common way a greedy plan gets stuck. */
        int fi = rnd_int(&rng, 0, w->nFlights - 1);
        int fj = -1, si, oldI, oldJ = -1;
        int isSwap = (rnd_f(&rng) < 0.45f);

        if (isSwap) {
            fj = rnd_int(&rng, 0, w->nFlights - 1);
            if (fj == fi) continue;
            oldI = cur[fi]; oldJ = cur[fj];
            if (oldI == oldJ) continue;
            if (!compatible(w, &w->flight[fi], oldJ)) continue;
            if (!compatible(w, &w->flight[fj], oldI)) continue;
            cur[fi] = oldJ;
            cur[fj] = oldI;
            si = oldJ;
        } else {
            si = rnd_int(&rng, 0, w->nStands - 1);
            if (!compatible(w, &w->flight[fi], si)) continue;
            oldI = cur[fi];
            if (oldI == si) continue;
            cur[fi] = si;
        }
        int old = oldI;
        (void)old; (void)si;

        int   nh = ai_stand_hard(w, cur);
        float nc = ai_stand_cost(w, cur, NULL, NULL, NULL);
        float d  = nc - curCost;

        int accept;
        if (nh != curHard) accept = (nh < curHard);   /* feasibility first  */
        else               accept = (d <= 0.f || rnd_f(&rng) < expf(-d / T));

        if (accept) {
            curCost = nc;
            curHard = nh;
            if (nh < bestHard || (nh == bestHard && nc < bestCost)) {
                bestHard = nh;
                bestCost = nc;
                memcpy(best, cur, sizeof(int) * (size_t)w->nFlights);
            }
        } else {
            cur[fi] = oldI;                           /* reject             */
            if (isSwap) cur[fj] = oldJ;
        }

        if (p->nCurve < 80 && (it % (iterations / 80 + 1)) == 0)
            p->bestCurve[p->nCurve++] = bestCost;
    }

    /* Safety net: if the search never beat the plan it started from, hand
     * back the original.  An optimiser that can return something worse than
     * its input is not an optimiser. */
    int   inHard = ai_stand_hard(w, incoming);
    float inCost = ai_stand_cost(w, incoming, NULL, NULL, NULL);
    if (bestHard > inHard || (bestHard == inHard && bestCost >= inCost))
        memcpy(best, incoming, sizeof(int) * (size_t)w->nFlights);

    memcpy(p->assign, best, sizeof(int) * (size_t)w->nFlights);
    p->cost = ai_stand_cost(w, best, &p->conflicts, &p->contactUsed, &p->walkAvg);
    p->iterations = iterations;

    p->towMoves = 0;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        for (int j = 0; j < w->nFlights; j++) {
            if (j == i) continue;
            Flight *g = &w->flight[j];
            if (g->arrival == f->arrival || strcmp(g->reg, f->reg) != 0) continue;
            if (p->assign[i] != p->assign[j]) p->towMoves++;
            break;
        }
    }
    p->towMoves /= 2;
    p->ready = 1;
}

/* Scatter the allocation onto random (but physically compatible) stands.
 * Used to demonstrate the optimiser: a scrambled plan is full of clashes and
 * bussed passengers, and the annealing run visibly recovers it. */
void ai_stand_scramble(World *w)
{
    uint32_t rng = 0xD15EA5Eu ^ (uint32_t)(w->clock * 97.f);
    for (int i = 0; i < w->nFlights; i++) {
        int tries = 0, s;
        do {
            s = rnd_int(&rng, 0, w->nStands - 1);
        } while (!compatible(w, &w->flight[i], s) && ++tries < 40);
        if (compatible(w, &w->flight[i], s)) {
            w->flight[i].stand = s;
            w->flight[i].gate  = w->stand[s].gate;
        }
    }
    world_log(w, LG_WARN, "Stand plan scrambled for optimiser demonstration");
}

void ai_stand_apply(StandPlan *p, World *w)
{
    if (!p->ready) return;
    int moved = 0;
    for (int i = 0; i < w->nFlights; i++) {
        if (p->assign[i] < 0) continue;
        if (w->flight[i].stand != p->assign[i]) moved++;
        w->flight[i].stand = p->assign[i];
        w->flight[i].gate  = w->stand[p->assign[i]].gate;
    }
    world_log(w, LG_AI, "Stand allocator applied: %d movements re-assigned, "
                        "%d conflicts remaining", moved, p->conflicts);
}
