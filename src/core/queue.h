/* ==========================================================================
 *  AURA :: queue.h  --  the queue abstract data type
 *
 *  A security lane and a boarding gate are both queues in the textbook sense:
 *  first in, first out, with a fixed amount of floor space.  This is that
 *  structure, written out properly rather than faked with a counter -- a
 *  circular buffer over a fixed array, so joining and being called forward
 *  are both O(1) and nothing is ever shuffled along.
 *
 *  Real airports do not run one queue, they run two: a priority lane for fast
 *  track, business class and passengers needing assistance, and a standard
 *  lane for everybody else.  PaxQueue is that pair, and the rule for who is
 *  called next lives in one place, pq_next().
 * ========================================================================== */
#ifndef AURA_QUEUE_H
#define AURA_QUEUE_H

#define QCAP 192                 /* people one lane can hold                */

/* --------------------------------------------------------------------------
 *  the plain FIFO
 *
 *  head is the next to be served, tail is where the next arrival is written.
 *  n is carried explicitly: with a circular buffer, head == tail is ambiguous
 *  between empty and full, and keeping the count is clearer than sacrificing
 *  a slot to disambiguate it.
 * ------------------------------------------------------------------------- */

typedef struct {
    int   pax[QCAP];
    float joined[QCAP];          /* clock minutes, for the wait statistics  */
    int   head, tail, n;

    int   served;                /* lifetime counters                       */
    int   maxSeen;
    int   turnedAway;
    float waitTotal;
    int   waitCount;
} Fifo;

void  fifo_init(Fifo *q);
int   fifo_push(Fifo *q, int pax, float nowMin);   /* 0 if full             */
int   fifo_pop (Fifo *q, float nowMin, float *waitOut);  /* -1 if empty     */
int   fifo_peek(const Fifo *q);                    /* -1 if empty           */
int   fifo_at  (const Fifo *q, int i);             /* i-th from the head    */
int   fifo_size(const Fifo *q);
int   fifo_full(const Fifo *q);
float fifo_avg_wait(const Fifo *q);

/* --------------------------------------------------------------------------
 *  a service point: two lanes and some desks
 * ------------------------------------------------------------------------- */

typedef struct {
    char  name[26];
    Fifo  priority;
    Fifo  standard;
    int   desks;                 /* how many are being served at once       */
    float servedFrac;            /* part-served passenger carried per tick  */
    int   priorityStreak;        /* how many in a row from the fast lane    */
} PaxQueue;

void  pq_init   (PaxQueue *q, const char *name, int desks);
int   pq_join   (PaxQueue *q, int pax, int priority, float nowMin);
int   pq_next   (PaxQueue *q, float nowMin, float *waitOut, int *wasPriority);
int   pq_size   (const PaxQueue *q);
int   pq_served (const PaxQueue *q);
float pq_avg_wait(const PaxQueue *q);
float pq_wait_estimate(const PaxQueue *q, float perPaxMin);

#endif /* AURA_QUEUE_H */
