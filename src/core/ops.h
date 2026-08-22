/* ==========================================================================
 *  AURA :: ops.h  --  flight, gate and runway management
 *
 *  This is the operational core: the things a duty controller actually does.
 *  Add a flight, retime it, change its status, cancel it.  Put it on a gate,
 *  and be told when that gate is already taken by somebody else at the same
 *  moment.  Watch a delay ripple into the flight parked behind it.  Keep two
 *  aeroplanes off the runway at once.
 *
 *  Everything here works on the World -- there is no separate copy of the
 *  schedule to fall out of step with it.
 * ========================================================================== */
#ifndef AURA_OPS_H
#define AURA_OPS_H

#include "model.h"

/*  Plaisance has eight contact gates, A1 to A8, and those are the scarce
 *  resource this module manages.  The remote stands carry their own bussing
 *  numbers in the twenties and thirties -- a bus can serve any of them, so
 *  they do not contend the way a pier gate does.  Twelve was wrong in both
 *  directions: it invented four gates that do not exist and ignored the
 *  remote stands that do.                                                  */
#define OPS_MAX_GATE 8
/*  Runway occupancy is about a minute either side of the moment a movement
 *  is logged -- the landing roll, or line-up to airborne.  On top of that
 *  sits the wake-turbulence separation the next aircraft must wait out.    */
#define OPS_RWY_OCC  1           /* minutes either side of the movement     */
#define OPS_RWY_SEP  2           /* minutes of separation behind it         */

/* ------------------------------------------------------------- flight CRUD */

typedef struct {
    char no[8];
    int  airline;                /* index into World.airline                */
    int  acType;
    int  airport;                /* the other end of the leg                */
    int  arrival;
    int  schedMin;
} FlightSpec;

/* Returns the new flight's id, or 0 if it could not be created. */
int  ops_flight_add   (World *w, const FlightSpec *s);
int  ops_flight_cancel(World *w, int id);
int  ops_flight_delay (World *w, int id, int minutes);
int  ops_flight_state (World *w, int id, FlightState st);
int  ops_flight_gate  (World *w, int id, int gate);
int  ops_flight_stand (World *w, int id, int stand);

/* Free-text search over number, city, airline, registration and status. */
int  ops_flight_find  (World *w, const char *query, int *out, int max);

/* ------------------------------------------------------------------- gates */

/*  How long a flight occupies its gate.  A departure holds it from the start
 *  of boarding; an arrival from touchdown until the last passenger is off.  */
void ops_gate_window(const World *w, const Flight *f, int *from, int *to);

typedef struct {
    int a, b;                    /* flight ids                              */
    int gate;
    int overlap;                 /* minutes the two windows share           */
} GateConflict;

int  ops_gate_conflicts(const World *w, GateConflict *out, int max);
int  ops_gate_free     (const World *w, int gate, int from, int to,
                        int exceptFlightId);
int  ops_gate_suggest  (const World *w, int flightId);   /* -1 if none free */
int  ops_gate_load     (const World *w, int gate);       /* flights today   */

/* ---------------------------------------------------------- delay ripple -- */

typedef struct {
    int  lateId;                 /* the flight running late                 */
    int  victimId;               /* the one it now clashes with             */
    int  gate;
    int  suggested;              /* a gate that would clear it, -1 if none  */
    int  overlap;
    char text[150];
} DelayImpact;

int  ops_delay_impact(const World *w, DelayImpact *out, int max);

/* ------------------------------------------------------------ passengers -- */

/*  Register a passenger onto a flight.  Returns the new passenger id, or 0.
 *  A seat is allocated automatically -- see ops_seat_assign.               */
int  ops_pax_register(World *w, const char *name, int flightId);

/*  Check a passenger in: marks them checked in, allocates a seat if they do
 *  not have one, and creates their hold baggage.  Returns 0 if they are
 *  already checked in or the flight has closed.                            */
int  ops_pax_checkin (World *w, int paxId, int bags);

/*  The first free seat on the flight, written into `out` (>= 5 bytes).
 *  Window seats go to passengers needing assistance, aisles to everybody
 *  else, which is what a real allocator does.  0 if the aircraft is full.  */
int  ops_seat_assign (World *w, int flightId, int assist, char *out);

int  ops_pax_find    (World *w, const char *query, int *out, int max);

/* ----------------------------------------------------------------- runway -- */

typedef struct {
    int flightId;
    int fromMin, toMin;          /* occupancy, including separation         */
    int arrival;
} RunwaySlot;

int  ops_runway_slots    (const World *w, RunwaySlot *out, int max);
int  ops_runway_occupied (const World *w, float atMin, int *byFlight);
int  ops_runway_conflicts(const World *w, int *pairs, int maxPairs);
int  ops_runway_next_free(const World *w, int fromMin);

#endif /* AURA_OPS_H */
