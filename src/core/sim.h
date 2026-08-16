/* ==========================================================================
 *  AURA :: sim.h  --  the live airport
 *
 *  One call, sim_tick(), advances the whole operation: the clock, every
 *  flight's state machine, aircraft rolling along real taxi routings, the
 *  baggage system, the check-in hall and the weather over Plaisance.
 *  Nothing on screen is scripted -- the screens read this state and draw it.
 * ========================================================================== */
#ifndef AURA_SIM_H
#define AURA_SIM_H

#include "model.h"

void  sim_tick(World *w, float dtSeconds);
void  sim_set_speed(World *w, float speed);
void  sim_toggle_pause(World *w);
void  sim_jump_to(World *w, float minutes);
void  sim_warm_baggage(World *w, float minutes);

/* sample a taxi routing: position plus heading at arc-length fraction t */
void  path_sample(const float *pts, int n, float t,
                  float *x, float *y, float *heading);
float path_frac_at(const float *pts, int n, int index);

/* how far through its movement a flight is, 0..1, or -1 if not moving */
float sim_flight_path(World *w, Flight *f, float *outXY, int maxPts, int *nOut);

/* queue / hall statistics the screens read */
int   sim_security_queue(World *w);
int   sim_checkin_queue (World *w);
float sim_security_wait (World *w);
int   sim_active_bags   (World *w);
int   bag_in_system     (BagState s);
int   sim_bags_in_state (World *w, BagState s);
int   sim_count_state   (World *w, FlightState s);
int   sim_next_departure(World *w);
int   sim_next_arrival  (World *w);

#endif /* AURA_SIM_H */
