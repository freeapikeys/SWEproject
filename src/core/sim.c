/* ==========================================================================
 *  AURA :: sim.c
 * ========================================================================== */

#include "sim.h"
#include "../engine/anim.h"
#include "../ai/ai.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 *  path sampling
 * ========================================================================== */

static float seg_len(const float *p, int i)
{
    float dx = p[(i+1)*2] - p[i*2];
    float dy = p[(i+1)*2+1] - p[i*2+1];
    return sqrtf(dx*dx + dy*dy);
}

float path_frac_at(const float *pts, int n, int index)
{
    if (n < 2) return 0.f;
    if (index <= 0) return 0.f;
    if (index >= n - 1) return 1.f;
    float total = 0.f, upto = 0.f;
    for (int i = 0; i < n - 1; i++) {
        float L = seg_len(pts, i);
        if (i < index) upto += L;
        total += L;
    }
    return total > 0.f ? upto / total : 0.f;
}

void path_sample(const float *pts, int n, float t,
                 float *x, float *y, float *heading)
{
    if (n < 2) {
        if (n == 1) { *x = pts[0]; *y = pts[1]; }
        if (heading) *heading = 0.f;
        return;
    }
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;

    float total = 0.f;
    for (int i = 0; i < n - 1; i++) total += seg_len(pts, i);
    if (total <= 0.f) { *x = pts[0]; *y = pts[1]; if (heading) *heading = 0.f; return; }

    float want = t * total, acc = 0.f;
    for (int i = 0; i < n - 1; i++) {
        float L = seg_len(pts, i);
        if (acc + L >= want || i == n - 2) {
            float u = L > 0.f ? (want - acc) / L : 0.f;
            if (u > 1.f) u = 1.f;
            float ax = pts[i*2],   ay = pts[i*2+1];
            float bx = pts[i*2+2], by = pts[i*2+3];
            *x = ax + (bx - ax) * u;
            *y = ay + (by - ay) * u;
            if (heading) *heading = atan2f(by - ay, bx - ax);
            return;
        }
        acc += L;
    }
}

/* ==========================================================================
 *  flight movement
 * ========================================================================== */

/* Departures: pushback at the estimated time, eight minutes of taxi, then
 * the roll.  Arrivals: six minutes of final approach onto the threshold, then
 * rollout and taxi in.  Both are anchored so the touchdown / rotation lands
 * exactly on the published time rather than drifting. */

float sim_flight_path(World *w, Flight *f, float *outXY, int maxPts, int *nOut)
{
    *nOut = 0;
    if (f->stand < 0) return -1.f;
    int n = field_taxi_path(w, f->stand, !f->arrival, outXY, maxPts);
    *nOut = n;
    if (n < 2) return -1.f;

    float now = w->clock;
    float est = (float)f->estMin;

    if (!f->arrival) {
        float t0 = est, t1 = est + 13.f;
        if (now < t0 || now > t1 + 1.2f) return -1.f;
        float u = (now - t0) / (t1 - t0);
        if (u < 0.f) u = 0.f;
        if (u > 1.f) u = 1.f;
        /* slow taxi then a quick roll: bias the parameter late */
        float holdFrac = path_frac_at(outXY, n, n - 3);
        if (u < 0.72f) return holdFrac * (u / 0.72f);
        float v = (u - 0.72f) / 0.28f;
        return holdFrac + (1.f - holdFrac) * (v*v);
    } else {
        float t0 = est - 6.f, t1 = est + 8.f;
        if (now < t0 || now > t1 + 1.2f) return -1.f;
        float touchFrac = path_frac_at(outXY, n, 1);
        float u = (now - t0) / (t1 - t0);
        if (u < 0.f) u = 0.f;
        if (u > 1.f) u = 1.f;
        float touchU = 6.f / 14.f;
        if (u < touchU) {
            float v = u / touchU;
            return touchFrac * v;
        }
        float v = (u - touchU) / (1.f - touchU);
        v = 1.f - (1.f - v)*(1.f - v);        /* decelerating rollout       */
        return touchFrac + (1.f - touchFrac) * v;
    }
}

static void update_flight_state(World *w, Flight *f)
{
    float now = w->clock;
    float est = (float)f->estMin;
    FlightState prev = f->state;

    if (f->state == FS_CANCELLED) return;

    if (!f->arrival) {
        if      (now >= est + 13.f)  f->state = FS_DEPARTED;
        else if (now >= est + 9.f)   f->state = FS_LINEUP;
        else if (now >= est + 2.f)   f->state = FS_TAXI_OUT;
        else if (now >= est)         f->state = FS_PUSHBACK;
        else if (now >= est - 10.f)  f->state = FS_CLOSED;
        else if (now >= est - 25.f)  f->state = FS_FINAL;
        else if (now >= est - 50.f)  f->state = FS_BOARDING;
        else if (now >= est - 180.f) f->state = FS_CHECKIN;
        else                         f->state = f->delayMin >= 15 ? FS_DELAYED
                                                                  : FS_SCHEDULED;
        if (f->delayMin >= 15 && now < est - 180.f) f->state = FS_DELAYED;
    } else {
        if      (now >= est + 8.f)   f->state = FS_ONBLOCK;
        else if (now >= est + 1.5f)  f->state = FS_TAXI_IN;
        else if (now >= est)         f->state = FS_LANDED;
        else if (now >= est - 6.f)   f->state = FS_APPROACH;
        else if (now >= est - 90.f)  f->state = FS_ENROUTE;
        else                         f->state = f->delayMin >= 15 ? FS_DELAYED
                                                                  : FS_SCHEDULED;
    }

    if (prev != f->state) {
        switch (f->state) {
        case FS_BOARDING:
            world_log(w, LG_INFO, "%s now boarding at gate %d",
                      f->no, f->gate);
            break;
        case FS_FINAL:
            world_log(w, LG_WARN, "%s final call, gate %d", f->no, f->gate);
            break;
        case FS_DEPARTED:
            world_log(w, LG_OK, "%s airborne for %s", f->no,
                      w->airport[f->airport].city);
            w->totalDepartures++;
            break;
        case FS_LANDED:
            world_log(w, LG_OK, "%s landed from %s, runway %02d", f->no,
                      w->airport[f->airport].city, w->activeRunway);
            w->totalArrivals++;
            break;
        case FS_ONBLOCK:
            world_log(w, LG_INFO, "%s on blocks, stand %s", f->no,
                      f->stand >= 0 ? w->stand[f->stand].name : "--");
            break;
        default: break;
        }
    }
}

static void update_flight_counts(World *w, Flight *f, float dtMin)
{
    float now = w->clock, est = (float)f->estMin;
    if (f->arrival) return;

    /* check-in ramps up over the three hours before departure */
    if (now > est - 180.f && now < est - 45.f) {
        float span = 135.f;
        float target = (now - (est - 180.f)) / span;
        int want = (int)(f->pax * target * 0.98f);
        if (want > f->pax) want = f->pax;
        if (want > f->checkedIn) f->checkedIn = want;
    } else if (now >= est - 45.f) {
        f->checkedIn = f->pax;
    }

    /* boarding fills the cabin over the boarding window */
    if (f->state == FS_BOARDING || f->state == FS_FINAL || f->state == FS_CLOSED) {
        float t0 = est - 50.f, t1 = est - 8.f;
        float u = (now - t0) / (t1 - t0);
        if (u < 0.f) u = 0.f;
        if (u > 1.f) u = 1.f;
        u = 1.f - (1.f - u)*(1.f - u);
        int want = (int)(f->pax * u);
        if (want > f->boarded) f->boarded = want;
    } else if (f->state >= FS_PUSHBACK && f->state <= FS_DEPARTED) {
        f->boarded = f->pax;
    }
    (void)dtMin;
}

/* ==========================================================================
 *  baggage flow
 * ========================================================================== */

static const float LANE_SPEED[BG_COUNT] = {
    0.052f,  /* check-in belt  */
    0.030f,  /* screening      */
    0.045f,  /* sortation      */
    0.038f,  /* make-up        */
    0.f, 0.030f, 0.f, 0.f, 0.f
};

static void update_bags(World *w, float dtMin)
{
    for (int i = 0; i < w->nBags; i++) {
        Bag *b = &w->bag[i];
        Flight *f = flight_by_id(w, b->flight);
        if (!f) continue;

        if (b->releaseMin <= 0.f) {
            b->releaseMin = (float)f->estMin - 195.f + (float)(b->id % 175);
            /* A deterministic phase offset.  Without it, every bag released
             * before the session started begins at t=0 on the same tick and
             * they ride the belt as one solid block. */
            uint32_t h = (uint32_t)b->id * 2654435761u;
            b->t = (float)(h % 977u) / 977.f;
        }

        if (w->clock < b->releaseMin) { b->t = 0.f; continue; }
        if (b->state >= BG_LOADED) continue;

        b->t += LANE_SPEED[b->state] * dtMin;
        b->wobble += dtMin * 3.f;

        if (b->t >= 1.f) {
            /* Enter the next belt at a small per-bag offset.  Resetting every
             * bag to exactly zero re-synchronises them at each transition, and
             * they re-form into a block just past the merge. */
            b->t = (float)(((uint32_t)b->id * 2246822519u) % 101u) / 101.f * 0.12f;
            switch (b->state) {
            case BG_CHECKIN:
                b->state = BG_SCREEN;
                break;
            case BG_SCREEN: {
                /* the neural classifier decides what happens next */
                b->threatScore = ai_bag_score(w, b);
                if (b->threatScore > BAG_THRESHOLD) {
                    b->threat = 1;
                    b->state  = BG_HELD;
                    w->securityAlerts++;
                    world_log(w, LG_WARN,
                              "Bag %s held for manual search (score %.2f)",
                              b->tag, b->threatScore);
                } else {
                    b->state = BG_SORT;
                }
            } break;
            case BG_SORT:
                b->state = BG_MAKEUP;
                break;
            case BG_MAKEUP:
                b->state = BG_LOADED;
                f->bagsLoaded++;
                break;
            default: break;
            }
        }
    }
}

/* ==========================================================================
 *  hall, weather, incidents
 * ========================================================================== */

static void update_desks(World *w, float dtMin)
{
    for (int i = 0; i < w->nDesks; i++) {
        Desk *d = &w->desk[i];
        if (!d->open) { d->queue = 0; continue; }
        d->served += dtMin * 1.8f;
        while (d->served >= 1.f) {
            d->served -= 1.f;
            if (d->queue > 0) d->queue--;
        }
        if (rnd_f(&w->rng) < dtMin * 0.55f) d->queue++;
        if (d->queue > 24) d->queue = 24;
    }
}

static void update_weather(World *w, float dtMin)
{
    float t = anim_time();
    w->tempC   = 24.2f + sinf(t * 0.05f) * 2.4f + noise1(t * 0.09f) * 1.1f;
    w->windKt  = 9.f + noise1(t * 0.13f + 4.f) * 9.f;
    w->windDir = 118.f + noise1(t * 0.07f + 9.f) * 44.f;
    w->humidity= 68.f + noise1(t * 0.06f + 2.f) * 16.f;
    w->qnh     = 1015.f + noise1(t * 0.03f) * 4.f;

    float wxn = noise1(t * 0.021f + 17.f);
    int   k   = wxn < 0.34f ? 0 : (wxn < 0.62f ? 1 : (wxn < 0.82f ? 2 :
                (wxn < 0.94f ? 3 : 4)));
    if (k != w->wxKind) {
        w->wxKind = k;
        if (k >= 3)
            world_log(w, LG_WARN, "Plaisance weather: %s reported over the field",
                      wx_name(k));
    }
    w->visKm = (k >= 3) ? 5.5f + noise1(t*0.2f)*3.f : 9.f + noise1(t*0.2f)*1.f;

    /* the runway in use follows the wind */
    int want = (w->windDir > 60.f && w->windDir < 240.f) ? 14 : 32;
    if (want != w->activeRunway) {
        w->activeRunway = want;
        world_log(w, LG_INFO, "Runway change: %02d now in use", want);
    }
    (void)dtMin;
}

static void update_incidents(World *w, float dtMin)
{
    if (rnd_f(&w->rng) > dtMin * 0.004f) return;
    int which = rnd_int(&w->rng, 0, 5);
    switch (which) {
    case 0: {
        int i = rnd_int(&w->rng, 0, w->nFlights - 1);
        Flight *f = &w->flight[i];
        if (f->state <= FS_CHECKIN && f->delayMin < 90) {
            int add = rnd_int(&w->rng, 10, 35);
            f->delayMin += add;
            f->estMin   += add;
            world_log(w, LG_WARN, "%s delayed a further %d min (late inbound)",
                      f->no, add);
        }
    } break;
    case 1:
        world_log(w, LG_INFO, "Bird hazard reported near threshold %02d, "
                              "dispersal team dispatched", w->activeRunway);
        break;
    case 2:
        world_log(w, LG_INFO, "Apron sweep completed on taxiway Alpha");
        break;
    case 3: {
        int i = rnd_int(&w->rng, 0, w->nBags - 1);
        if (w->bag[i].state < BG_LOADED) {
            w->bag[i].state = BG_MISHANDLED;
            w->bagsMishandled++;
            world_log(w, LG_ERR, "Bag %s fell out of the sortation loop",
                      w->bag[i].tag);
        }
    } break;
    case 4:
        world_log(w, LG_OK, "Ground power restored to stand %s",
                  w->stand[rnd_int(&w->rng, 0, w->nStands-1)].name);
        break;
    default:
        world_log(w, LG_INFO, "Fuel bowser 3 released for stand duties");
        break;
    }
}

/* ==========================================================================
 *  tick
 * ========================================================================== */

void sim_set_speed(World *w, float s)   { w->speed = s; }
void sim_toggle_pause(World *w)         { w->paused = !w->paused; }

void sim_jump_to(World *w, float minutes)
{
    w->clock = minutes;
    while (w->clock >= 1440.f) w->clock -= 1440.f;
    while (w->clock < 0.f)     w->clock += 1440.f;
}

void sim_tick(World *w, float dtSeconds)
{
    if (w->paused) dtSeconds = 0.f;
    float dtMin = dtSeconds * w->speed / 60.f * 60.f;   /* speed = min/sec  */
    dtMin = dtSeconds * w->speed;

    w->clock += dtMin;
    if (w->clock >= 1440.f) {
        w->clock -= 1440.f;
        w->day++;
        world_log(w, LG_INFO, "Date rollover -- new operating day begins");
    }

    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        update_flight_state(w, f);
        update_flight_counts(w, f, dtMin);
        f->strobe = ((int)(anim_time() * 1.5f) & 1);
    }

    update_bags(w, dtMin);
    update_desks(w, dtMin);
    update_weather(w, dtMin);
    update_incidents(w, dtMin);
}

/* Advance only the baggage system, without moving the clock.  Called once at
 * start-up so the belts open in the state they would really be in: bags spread
 * from check-in through screening, sortation and make-up, rather than every
 * bag in the airport standing at the first conveyor. */
void sim_warm_baggage(World *w, float minutes)
{
    int steps = (int)minutes;
    if (steps < 1) steps = 1;
    if (steps > 600) steps = 600;
    for (int i = 0; i < steps; i++) update_bags(w, 1.0f);
}

/* ==========================================================================
 *  read-only statistics
 * ========================================================================== */

int sim_count_state(World *w, FlightState s)
{
    int n = 0;
    for (int i = 0; i < w->nFlights; i++) if (w->flight[i].state == s) n++;
    return n;
}

int sim_bags_in_state(World *w, BagState s)
{
    int n = 0;
    for (int i = 0; i < w->nBags; i++) if (w->bag[i].state == s) n++;
    return n;
}

/* A bag counts as "in the system" while it is anywhere on the belts or in
 * the search bay.  BG_HELD sits after BG_LOADED in the enumeration, so a
 * plain "state < BG_LOADED" test silently loses every held bag. */
int bag_in_system(BagState s)
{
    return s == BG_CHECKIN || s == BG_SCREEN || s == BG_SORT ||
           s == BG_MAKEUP  || s == BG_HELD;
}

int sim_active_bags(World *w)
{
    int n = 0;
    for (int i = 0; i < w->nBags; i++) {
        Bag *b = &w->bag[i];
        if (bag_in_system(b->state) && b->releaseMin > 0.f &&
            w->clock >= b->releaseMin)
            n++;
    }
    return n;
}

int sim_checkin_queue(World *w)
{
    int n = 0;
    for (int i = 0; i < w->nDesks; i++) n += w->desk[i].queue;
    return n;
}

int sim_security_queue(World *w)
{
    /* everybody checked in but not yet through the search area */
    int n = 0;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival) continue;
        if (f->state == FS_CHECKIN || f->state == FS_BOARDING)
            n += (f->checkedIn - f->boarded) / 8;
    }
    return n < 0 ? 0 : n;
}

float sim_security_wait(World *w)
{
    int q = sim_security_queue(w);
    int lanes = 6;
    float perLane = (float)q / lanes;
    return perLane * 0.42f + 2.5f;      /* minutes */
}

int sim_next_departure(World *w)
{
    int best = -1;
    float bestT = 1e9f;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival || f->state >= FS_DEPARTED) continue;
        float d = (float)f->estMin - w->clock;
        if (d >= -5.f && d < bestT) { bestT = d; best = i; }
    }
    return best;
}

int sim_next_arrival(World *w)
{
    int best = -1;
    float bestT = 1e9f;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (!f->arrival || f->state >= FS_ONBLOCK) continue;
        float d = (float)f->estMin - w->clock;
        if (d >= -8.f && d < bestT) { bestT = d; best = i; }
    }
    return best;
}
