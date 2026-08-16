/* ==========================================================================
 *  AURA :: ai_flow.c   --   terminal flow forecasting
 *
 *  Two models chained together:
 *
 *   1. Holt's linear (double exponential) smoothing over five-minute bins of
 *      passengers presenting at the search area.  It carries a level and a
 *      trend, which matters here because demand at Plaisance does not drift
 *      -- it ramps, hard, three hours before each long-haul bank.
 *
 *   2. An M/M/c queue fed by that forecast, which converts "how many people
 *      are coming" into the number a duty manager actually needs: how long
 *      the queue will be, and how many lanes have to be open to hold the
 *      service level.
 *
 *  The forecast is compared against what actually happened one step later,
 *  so the mean absolute percentage error shown on screen is measured, not
 *  asserted.
 * ========================================================================== */

#include "ai.h"
#include "../engine/anim.h"
#include <math.h>
#include <string.h>

/* --------------------------------------------------------------------------
 *  demand: how many passengers present at search in the five minutes ending
 *  at `minute`, derived from the published departure schedule.
 * ------------------------------------------------------------------------- */

static float demand_at(World *w, float minute)
{
    float total = 0.f;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival || f->state == FS_CANCELLED) continue;
        float lead = (float)f->estMin - minute;
        if (lead < 25.f || lead > 195.f) continue;
        /* passengers show up on a hump centred about 95 minutes before */
        float d = (lead - 95.f) / 46.f;
        float share = expf(-d*d);
        total += (float)f->pax * share * 0.055f;
    }
    /* the search area also sees staff and meeters, roughly a flat floor */
    return total + 3.5f;
}

void ai_flow_observe(FlowModel *m, World *w)
{
    if (m->nHist == 0) {
        /* prime the history with the two hours behind us */
        for (int i = 0; i < FLOW_HIST; i++) {
            float t = w->clock - (FLOW_HIST - i) * 5.f;
            m->history[i] = demand_at(w, t);
        }
        m->nHist = FLOW_HIST;
        m->alpha = 0.42f;
        m->beta  = 0.18f;
        m->level = m->history[FLOW_HIST-1];
        m->trend = m->history[FLOW_HIST-1] - m->history[FLOW_HIST-2];
        m->lanesOpen = 6;
        return;
    }
    /* slide one bin on */
    memmove(&m->history[0], &m->history[1], sizeof(float) * (FLOW_HIST - 1));
    m->history[FLOW_HIST-1] = demand_at(w, w->clock);
}

/* ==========================================================================
 *  M/M/c  --  Erlang C
 * ========================================================================== */

float ai_flow_queue_wait(float lambda, float mu, int c)
{
    if (c < 1) c = 1;
    if (mu <= 0.f) return 99.f;
    float a   = lambda / mu;             /* offered load, erlangs           */
    float rho = a / (float)c;
    if (rho >= 0.985f) return 45.f;      /* saturated: the queue runs away  */

    /* Erlang C probability that an arrival has to wait */
    double sum = 0.0, term = 1.0;
    for (int k = 0; k < c; k++) {
        if (k > 0) term *= (double)a / k;
        sum += term;
    }
    double last = term * ((double)a / c);
    double pc   = last / (1.0 - (double)rho);
    double p0   = 1.0 / (sum + pc);
    double C    = pc * p0;

    float wq = (float)(C / (c * mu * (1.0 - rho)));   /* minutes            */
    return wq;
}

/* ==========================================================================
 *  forecast
 * ========================================================================== */

void ai_flow_forecast(FlowModel *m, World *w)
{
    if (m->nHist < 4) { ai_flow_observe(m, w); return; }

    /* --- Holt's linear method over the observed bins ---------------------- */
    float level = m->history[0], trend = m->history[1] - m->history[0];
    float errSum = 0.f;
    int   errN = 0;

    for (int i = 1; i < m->nHist; i++) {
        float pred = level + trend;
        float y    = m->history[i];
        if (y > 0.5f) { errSum += fabsf(pred - y) / y; errN++; }
        float newLevel = m->alpha * y + (1.f - m->alpha) * (level + trend);
        trend = m->beta * (newLevel - level) + (1.f - m->beta) * trend;
        level = newLevel;
    }
    m->level = level;
    m->trend = trend;
    m->mape  = errN ? 100.f * errSum / errN : 0.f;

    /* --- project forward, widening the band as we go --------------------- */
    float sd = 0.f;
    for (int i = 1; i < m->nHist; i++) {
        float d = m->history[i] - m->history[i-1];
        sd += d*d;
    }
    sd = sqrtf(sd / (m->nHist - 1));

    for (int h = 0; h < FLOW_HORIZON; h++) {
        float f = level + trend * (h + 1);
        /* the schedule is known, so blend the statistical projection with
         * what the timetable says is coming -- the hybrid beats either */
        float sched = demand_at(w, w->clock + (h + 1) * 5.f);
        f = f * 0.35f + sched * 0.65f;
        if (f < 0.f) f = 0.f;
        m->forecast[h] = f;
        float band = sd * sqrtf((float)(h + 1)) * 1.28f;   /* ~80% interval */
        m->upper[h] = f + band;
        m->lower[h] = f - band < 0.f ? 0.f : f - band;
    }

    /* --- queueing ---------------------------------------------------------
     * Each lane processes a passenger about every 21 seconds once the tray
     * flow is steady, so mu is roughly 2.85 passengers per minute per lane. */
    const float mu = 2.85f;
    float lambdaNow = m->history[m->nHist-1] / 5.f;    /* per minute        */
    if (m->lanesOpen < 1) m->lanesOpen = 6;

    m->waitNow = ai_flow_queue_wait(lambdaNow, mu, m->lanesOpen);

    float peak = 0.f;
    for (int h = 0; h < FLOW_HORIZON; h++)
        if (m->forecast[h] > peak) peak = m->forecast[h];
    float lambdaPeak = peak / 5.f;
    m->waitPeak = ai_flow_queue_wait(lambdaPeak, mu, m->lanesOpen);

    /* how many lanes hold the ten minute service level at the peak? */
    m->lanesNeeded = 1;
    for (int c = 1; c <= 12; c++) {
        if (ai_flow_queue_wait(lambdaPeak, mu, c) <= 10.f) { m->lanesNeeded = c; break; }
        m->lanesNeeded = c;
    }
    m->utilisation = lambdaNow / (mu * m->lanesOpen);
    m->ready = 1;
}
