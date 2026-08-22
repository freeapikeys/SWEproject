/* ==========================================================================
 *  AURA :: incident.c  --  emergencies and passenger notifications
 * ========================================================================== */

#include "incident.h"
#include <stdio.h>
#include <string.h>

/* ==========================================================================
 *  emergencies
 * ========================================================================== */

const char *emg_kind_name(EmgKind k)
{
    switch (k) {
    case EMG_FIRE:     return "Fire";
    case EMG_MEDICAL:  return "Medical";
    case EMG_SECURITY: return "Security";
    case EMG_BAGGAGE:  return "Suspicious baggage";
    case EMG_AIRCRAFT: return "Aircraft";
    default:           return "Weather";
    }
}

const char *emg_level_name(EmgLevel l)
{
    switch (l) {
    case EMG_CRITICAL: return "CRITICAL";
    case EMG_MAJOR:    return "MAJOR";
    case EMG_MINOR:    return "MINOR";
    default:           return "ADVISORY";
    }
}

const char *emg_state_name(EmgState s)
{
    switch (s) {
    case EMS_RAISED:        return "Raised";
    case EMS_ACKNOWLEDGED:  return "Acknowledged";
    case EMS_RESPONDING:    return "Responding";
    default:                return "Resolved";
    }
}

/*  What the airport is supposed to do.  Having the protocol next to the
 *  incident is the difference between a log and a system somebody can work
 *  from at three in the morning.                                           */
const char *emg_protocol(EmgKind k)
{
    switch (k) {
    case EMG_FIRE:
        return "Sound the alarm for the affected zone. Airport fire service "
               "to attend. Evacuate to the assembly point. Hold all boarding "
               "at gates in that pier.";
    case EMG_MEDICAL:
        return "Dispatch the terminal medical team with a defibrillator. "
               "Clear a route for the ambulance to the kerbside. Notify the "
               "flight if the patient is a passenger.";
    case EMG_SECURITY:
        return "Airport police to attend. Seal the affected zone. Stop "
               "screening at the affected lane and divert the queue. Preserve "
               "the CCTV record.";
    case EMG_BAGGAGE:
        return "Do not move the item. Clear 100 m and cordon. Call the bomb "
               "disposal unit. Divert the baggage line around the affected "
               "sortation loop.";
    case EMG_AIRCRAFT:
        return "Declare a local standby. Fire service to the stand. Stop all "
               "runway movements. Prepare the stairs and buses for a rapid "
               "disembarkation.";
    default:
        return "Suspend apron operations if lightning is within 5 NM. Secure "
               "loose equipment. Advise crews of the expected duration.";
    }
}

EmgLevel emg_default_level(EmgKind k)
{
    switch (k) {
    case EMG_FIRE:     return EMG_CRITICAL;
    case EMG_AIRCRAFT: return EMG_CRITICAL;
    case EMG_MEDICAL:  return EMG_MAJOR;
    case EMG_SECURITY: return EMG_MAJOR;
    case EMG_BAGGAGE:  return EMG_MAJOR;
    default:           return EMG_MINOR;
    }
}

/* how quickly it must be acknowledged, in minutes */
int emg_target_minutes(EmgLevel l)
{
    switch (l) {
    case EMG_CRITICAL: return 1;
    case EMG_MAJOR:    return 3;
    case EMG_MINOR:    return 15;
    default:           return 60;
    }
}

void emg_init(EmergencyLog *L)
{
    memset(L, 0, sizeof *L);
    L->nextId = 1;
}

int emg_raise(EmergencyLog *L, World *w, EmgKind k, EmgLevel lv,
              const char *where, const char *detail, int flight)
{
    if (L->n >= MAX_EMERGENCY) {
        /*  Drop the oldest resolved entry rather than refusing to record a
         *  live emergency.  A full log must never be the reason an incident
         *  goes unrecorded.                                                */
        int drop = -1;
        for (int i = 0; i < L->n; i++)
            if (L->e[i].state == EMS_RESOLVED) { drop = i; break; }
        if (drop < 0) drop = 0;
        memmove(&L->e[drop], &L->e[drop+1],
                sizeof(Emergency) * (size_t)(L->n - drop - 1));
        L->n--;
    }

    Emergency *e = &L->e[L->n++];
    memset(e, 0, sizeof *e);
    e->id        = L->nextId++;
    e->kind      = k;
    e->level     = lv;
    e->state     = EMS_RAISED;
    e->flight    = flight;
    e->raisedMin = w->clock;
    e->ackMin    = -1.f;
    e->clearedMin= -1.f;
    snprintf(e->where,  sizeof e->where,  "%s", where  ? where  : "Terminal");
    snprintf(e->detail, sizeof e->detail, "%s", detail ? detail : "");
    L->totalRaised++;

    w->securityAlerts += (k == EMG_SECURITY || k == EMG_BAGGAGE) ? 1 : 0;
    world_log(w, lv <= EMG_MAJOR ? LG_ERR : LG_WARN,
              "%s %s at %s -- %s", emg_level_name(lv), emg_kind_name(k),
              e->where, e->detail);
    return e->id;
}

static Emergency *emg_by_id(EmergencyLog *L, int id)
{
    for (int i = 0; i < L->n; i++) if (L->e[i].id == id) return &L->e[i];
    return NULL;
}

int emg_ack(EmergencyLog *L, World *w, int id)
{
    Emergency *e = emg_by_id(L, id);
    if (!e || e->state != EMS_RAISED) return 0;
    e->state  = EMS_ACKNOWLEDGED;
    e->ackMin = w->clock;
    float dt = e->ackMin - e->raisedMin;
    if (dt < 0.f) dt += 1440.f;
    L->responseTotal += dt;
    L->responseCount++;
    static const char *TEAM[EMG_KIND_COUNT] = {
        "Airport Fire Service", "Terminal Medical Team", "Airport Police",
        "Bomb Disposal Unit", "Airport Fire Service", "Duty Operations Manager"
    };
    snprintf(e->responder, sizeof e->responder, "%s", TEAM[e->kind]);
    world_log(w, LG_INFO, "Incident %d acknowledged by %s", id, e->responder);
    return 1;
}

int emg_respond(EmergencyLog *L, World *w, int id)
{
    Emergency *e = emg_by_id(L, id);
    if (!e || (e->state != EMS_ACKNOWLEDGED && e->state != EMS_RAISED)) return 0;
    if (e->state == EMS_RAISED) emg_ack(L, w, id);
    e->state = EMS_RESPONDING;
    if (e->level == EMG_CRITICAL) e->evacuate = 1;
    world_log(w, LG_WARN, "Incident %d -- %s on scene at %s", id,
              e->responder, e->where);
    return 1;
}

int emg_resolve(EmergencyLog *L, World *w, int id)
{
    Emergency *e = emg_by_id(L, id);
    if (!e || e->state == EMS_RESOLVED) return 0;
    if (e->ackMin < 0.f) emg_ack(L, w, id);
    e->state      = EMS_RESOLVED;
    e->clearedMin = w->clock;
    L->totalResolved++;
    world_log(w, LG_OK, "Incident %d stood down -- %s at %s cleared", id,
              emg_kind_name(e->kind), e->where);
    return 1;
}

int emg_active(const EmergencyLog *L)
{
    int n = 0;
    for (int i = 0; i < L->n; i++) if (L->e[i].state != EMS_RESOLVED) n++;
    return n;
}

int emg_worst(const EmergencyLog *L)
{
    int worst = -1;
    for (int i = 0; i < L->n; i++) {
        if (L->e[i].state == EMS_RESOLVED) continue;
        if (worst < 0 || (int)L->e[i].level < worst) worst = (int)L->e[i].level;
    }
    return worst;
}

float emg_avg_response(const EmergencyLog *L)
{
    return L->responseCount ? L->responseTotal / (float)L->responseCount : 0.f;
}

int emg_raise_random(EmergencyLog *L, World *w)
{
    static const struct { EmgKind k; const char *where, *detail; } S[] = {
        { EMG_MEDICAL,  "Departures, Gate 6",
          "Passenger collapsed at the gate, conscious and breathing" },
        { EMG_MEDICAL,  "Immigration hall",
          "Elderly passenger unwell in the queue, wheelchair requested" },
        { EMG_BAGGAGE,  "Check-in island B",
          "Unattended suitcase, owner not located after three calls" },
        { EMG_BAGGAGE,  "Baggage make-up area",
          "Bag flagged by the screening classifier and diverted" },
        { EMG_SECURITY, "Security lane 3",
          "Prohibited item detected, passenger detained pending search" },
        { EMG_SECURITY, "Airside access door A12",
          "Door forced open, no pass presented" },
        { EMG_FIRE,     "Terminal kitchen, Level 1",
          "Smoke detected in the extraction duct" },
        { EMG_FIRE,     "Stand A3",
          "Auxiliary power unit overheat reported by the crew" },
        { EMG_AIRCRAFT, "Runway 14 threshold",
          "Bird strike reported on departure, crew returning" },
        { EMG_WEATHER,  "Apron",
          "Lightning within five nautical miles, ramp work suspended" },
    };
    int n = (int)(sizeof S / sizeof S[0]);
    int i = (int)(w->rng % (uint32_t)n);
    w->rng = w->rng * 1664525u + 1013904223u;
    return emg_raise(L, w, S[i].k, emg_default_level(S[i].k),
                     S[i].where, S[i].detail, 0);
}

/* ==========================================================================
 *  notifications
 * ========================================================================== */

const char *nt_kind_name(NotifyKind k)
{
    switch (k) {
    case NT_GATE:      return "Gate change";
    case NT_DELAY:     return "Delay";
    case NT_BOARDING:  return "Boarding";
    case NT_FINAL:     return "Final call";
    case NT_CANCELLED: return "Cancelled";
    case NT_BAGGAGE:   return "Baggage";
    case NT_EMERGENCY: return "Emergency";
    default:           return "Information";
    }
}

void nt_init(NotifyCentre *N)
{
    memset(N, 0, sizeof *N);
    N->nextId = 1;
}

int nt_push(NotifyCentre *N, World *w, NotifyKind k, int pax, int flight,
            const char *text)
{
    if (N->count >= MAX_NOTIFY) {
        /* a ring: the oldest message falls off the end */
        memmove(&N->n[0], &N->n[1], sizeof(Notification) * (MAX_NOTIFY - 1));
        N->count = MAX_NOTIFY - 1;
    }
    Notification *m = &N->n[N->count++];
    memset(m, 0, sizeof *m);
    m->id     = N->nextId++;
    m->kind   = k;
    m->pax    = pax;
    m->flight = flight;
    m->atMin  = w->clock;
    snprintf(m->text, sizeof m->text, "%s", text ? text : "");
    N->sent++;
    return m->id;
}

/*  Send one message to every passenger booked on a flight.  Passengers are a
 *  representative sample rather than the full manifest, so this is a handful
 *  of messages per flight, not three hundred.                              */
static void notify_flight(NotifyCentre *N, World *w, NotifyKind k,
                          const Flight *f, const char *text)
{
    int sent = 0;
    for (int i = 0; i < w->nPax; i++) {
        if (w->pax[i].flight != f->id) continue;
        nt_push(N, w, k, w->pax[i].id, f->id, text);
        sent++;
    }
    /* nobody sampled on this flight: still record it as a public announcement */
    if (!sent) nt_push(N, w, k, 0, f->id, text);
}

void nt_scan(NotifyCentre *N, World *w)
{
    char msg[150];

    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (i >= MAX_FLIGHTS) break;

        int gate  = f->gate;
        int delay = f->delayMin;
        int state = (int)f->state;

        if (!N->primed) {
            /*  First pass after a schedule is loaded records the baseline
             *  without announcing it.  Otherwise starting the application
             *  would fire a notification for every flight of the day.      */
            N->lastGate[i]  = gate;
            N->lastDelay[i] = delay;
            N->lastState[i] = state;
            continue;
        }

        if (gate != N->lastGate[i] && gate > 0 && N->lastGate[i] > 0) {
            snprintf(msg, sizeof msg,
                     "%s: gate changed from %d to %d. Please make your way"
                     " to the new gate.", f->no, N->lastGate[i], gate);
            notify_flight(N, w, NT_GATE, f, msg);
        }
        if (delay != N->lastDelay[i] && delay >= 15) {
            char hm[8]; fmt_hhmm(f->estMin, hm);
            snprintf(msg, sizeof msg,
                     "%s: delayed by %d minutes. New estimated time %s.",
                     f->no, delay, hm);
            notify_flight(N, w, NT_DELAY, f, msg);
        }
        if (state != N->lastState[i]) {
            if (f->state == FS_BOARDING) {
                snprintf(msg, sizeof msg,
                         "%s: boarding now at gate %d.", f->no, f->gate);
                notify_flight(N, w, NT_BOARDING, f, msg);
            } else if (f->state == FS_FINAL) {
                snprintf(msg, sizeof msg,
                         "%s: final call. Gate %d closes shortly.",
                         f->no, f->gate);
                notify_flight(N, w, NT_FINAL, f, msg);
            } else if (f->state == FS_CANCELLED) {
                snprintf(msg, sizeof msg,
                         "%s to %s is cancelled. Please go to the airline"
                         " desk in the departures hall.",
                         f->no, w->airport[f->airport].city);
                notify_flight(N, w, NT_CANCELLED, f, msg);
            }
        }

        N->lastGate[i]  = gate;
        N->lastDelay[i] = delay;
        N->lastState[i] = state;
    }
    N->primed = 1;
}

int nt_for_pax(const NotifyCentre *N, int pax, int *out, int max)
{
    int n = 0;
    /* newest first: that is the order anybody reads a notification list */
    for (int i = N->count - 1; i >= 0 && n < max; i--)
        if (N->n[i].pax == pax || N->n[i].pax == 0) out[n++] = i;
    return n;
}

int nt_unread(const NotifyCentre *N, int pax)
{
    int n = 0;
    for (int i = 0; i < N->count; i++)
        if ((N->n[i].pax == pax || N->n[i].pax == 0) && !N->n[i].read) n++;
    return n;
}

void nt_mark_read(NotifyCentre *N, int pax)
{
    for (int i = 0; i < N->count; i++)
        if (N->n[i].pax == pax || N->n[i].pax == 0) N->n[i].read = 1;
}
