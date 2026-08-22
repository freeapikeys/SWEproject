/* ==========================================================================
 *  AURA :: queue.c  --  the queue abstract data type
 * ========================================================================== */

#include "queue.h"
#include <string.h>
#include <stdio.h>

/* ==========================================================================
 *  FIFO
 * ========================================================================== */

void fifo_init(Fifo *q)
{
    memset(q, 0, sizeof *q);
}

int fifo_full(const Fifo *q) { return q->n >= QCAP; }
int fifo_size(const Fifo *q) { return q->n; }

int fifo_push(Fifo *q, int pax, float nowMin)
{
    if (q->n >= QCAP) { q->turnedAway++; return 0; }
    q->pax[q->tail]    = pax;
    q->joined[q->tail] = nowMin;
    q->tail = (q->tail + 1) % QCAP;
    q->n++;
    if (q->n > q->maxSeen) q->maxSeen = q->n;
    return 1;
}

int fifo_pop(Fifo *q, float nowMin, float *waitOut)
{
    if (q->n <= 0) return -1;
    int   pax  = q->pax[q->head];
    float wait = nowMin - q->joined[q->head];
    /* the clock can cross midnight between joining and being called */
    if (wait < 0.f) wait += 1440.f;
    q->head = (q->head + 1) % QCAP;
    q->n--;
    q->served++;
    q->waitTotal += wait;
    q->waitCount++;
    if (waitOut) *waitOut = wait;
    return pax;
}

int fifo_peek(const Fifo *q)
{
    return q->n > 0 ? q->pax[q->head] : -1;
}

int fifo_at(const Fifo *q, int i)
{
    if (i < 0 || i >= q->n) return -1;
    return q->pax[(q->head + i) % QCAP];
}

float fifo_avg_wait(const Fifo *q)
{
    return q->waitCount ? q->waitTotal / (float)q->waitCount : 0.f;
}

/* ==========================================================================
 *  the service point
 * ========================================================================== */

void pq_init(PaxQueue *q, const char *name, int desks)
{
    memset(q, 0, sizeof *q);
    snprintf(q->name, sizeof q->name, "%s", name ? name : "");
    fifo_init(&q->priority);
    fifo_init(&q->standard);
    q->desks = desks > 0 ? desks : 1;
}

int pq_join(PaxQueue *q, int pax, int priority, float nowMin)
{
    return priority ? fifo_push(&q->priority, pax, nowMin)
                    : fifo_push(&q->standard, pax, nowMin);
}

/*  Who goes next.
 *
 *  Serving the priority lane until it empties is what a naive implementation
 *  does, and it is also how a standard queue ends up waiting for ever behind
 *  a steady trickle of fast-track passengers.  Real lanes interleave: after
 *  three from the priority lane, one standard passenger is called regardless.
 *  That bounds the worst case for everybody without giving up the point of
 *  having a priority lane at all.                                          */
int pq_next(PaxQueue *q, float nowMin, float *waitOut, int *wasPriority)
{
    int takePriority;
    if (fifo_size(&q->priority) == 0)      takePriority = 0;
    else if (fifo_size(&q->standard) == 0) takePriority = 1;
    else                                   takePriority = (q->priorityStreak < 3);

    int pax = takePriority ? fifo_pop(&q->priority, nowMin, waitOut)
                           : fifo_pop(&q->standard, nowMin, waitOut);
    if (pax < 0) return -1;

    if (takePriority) q->priorityStreak++;
    else              q->priorityStreak = 0;
    if (wasPriority) *wasPriority = takePriority;
    return pax;
}

int pq_size(const PaxQueue *q)
{
    return fifo_size(&q->priority) + fifo_size(&q->standard);
}

int pq_served(const PaxQueue *q)
{
    return q->priority.served + q->standard.served;
}

float pq_avg_wait(const PaxQueue *q)
{
    int n = q->priority.waitCount + q->standard.waitCount;
    if (!n) return 0.f;
    return (q->priority.waitTotal + q->standard.waitTotal) / (float)n;
}

/*  How long somebody joining right now should expect to wait: everyone ahead
 *  of them, divided by the number of desks open, times the service time.  */
float pq_wait_estimate(const PaxQueue *q, float perPaxMin)
{
    int ahead = pq_size(q);
    int desks = q->desks > 0 ? q->desks : 1;
    return (float)ahead * perPaxMin / (float)desks;
}
