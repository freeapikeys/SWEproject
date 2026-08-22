/* ==========================================================================
 *  AURA :: incident.h  --  emergencies and passenger notifications
 *
 *  Two things that look unrelated and are not.  When something goes wrong the
 *  airport has to do two jobs at once: get the right people to the right
 *  place, and tell the passengers what is happening.  Both are queues of
 *  messages with a priority on them, so they live together.
 *
 *  Emergencies carry a severity that decides the order they are worked, the
 *  response the system recommends and how long it is acceptable to leave them
 *  unacknowledged.  Notifications are generated automatically from changes in
 *  flight state -- a gate move, a delay, boarding, a cancellation -- so no
 *  operator has to remember to send them.
 * ========================================================================== */
#ifndef AURA_INCIDENT_H
#define AURA_INCIDENT_H

#include "model.h"

#define MAX_EMERGENCY   64
#define MAX_NOTIFY     320

/* ------------------------------------------------------------ emergencies -- */

typedef enum {
    EMG_FIRE = 0,        /* terminal or apron fire                          */
    EMG_MEDICAL,         /* passenger or staff medical                      */
    EMG_SECURITY,        /* breach, disorder, unauthorised airside access   */
    EMG_BAGGAGE,         /* suspicious or unclaimed item                    */
    EMG_AIRCRAFT,        /* aircraft on ground emergency                    */
    EMG_WEATHER,         /* severe weather affecting operations             */
    EMG_KIND_COUNT
} EmgKind;

/*  Severity, and what it means operationally.
 *      CRITICAL  life at immediate risk; everything else stops
 *      MAJOR     serious, needs a response now, operation continues
 *      MINOR     needs attention within the hour
 *      ADVISORY  logged and watched                                        */
typedef enum { EMG_CRITICAL = 0, EMG_MAJOR, EMG_MINOR, EMG_ADVISORY,
               EMG_LEVEL_COUNT } EmgLevel;

typedef enum { EMS_RAISED = 0, EMS_ACKNOWLEDGED, EMS_RESPONDING,
               EMS_RESOLVED } EmgState;

typedef struct {
    int      id;
    EmgKind  kind;
    EmgLevel level;
    EmgState state;
    char     where[42];
    char     detail[130];
    char     responder[44];
    int      flight;             /* related flight id, 0 if none            */
    float    raisedMin;          /* clock minutes                           */
    float    ackMin;
    float    clearedMin;
    int      evacuate;           /* the area was evacuated                  */
} Emergency;

typedef struct {
    Emergency e[MAX_EMERGENCY];
    int       n;
    int       nextId;
    int       totalRaised;
    int       totalResolved;
    float     responseTotal;     /* minutes from raised to acknowledged     */
    int       responseCount;
} EmergencyLog;

void         emg_init   (EmergencyLog *L);
int          emg_raise  (EmergencyLog *L, World *w, EmgKind k, EmgLevel lv,
                         const char *where, const char *detail, int flight);
int          emg_ack    (EmergencyLog *L, World *w, int id);
int          emg_respond(EmergencyLog *L, World *w, int id);
int          emg_resolve(EmergencyLog *L, World *w, int id);
int          emg_active (const EmergencyLog *L);
int          emg_worst  (const EmergencyLog *L);   /* level, or -1 if clear */
float        emg_avg_response(const EmergencyLog *L);

const char  *emg_kind_name (EmgKind k);
const char  *emg_level_name(EmgLevel l);
const char  *emg_state_name(EmgState s);
const char  *emg_protocol  (EmgKind k);    /* the recommended response      */
EmgLevel     emg_default_level(EmgKind k);
int          emg_target_minutes(EmgLevel l);   /* acknowledgement target    */

/* Raises a plausible incident, for demonstration and for the drill button. */
int          emg_raise_random(EmergencyLog *L, World *w);

/* --------------------------------------------------------- notifications -- */

typedef enum {
    NT_GATE = 0, NT_DELAY, NT_BOARDING, NT_FINAL, NT_CANCELLED,
    NT_BAGGAGE, NT_EMERGENCY, NT_GENERAL, NT_KIND_COUNT
} NotifyKind;

typedef struct {
    int        id;
    NotifyKind kind;
    int        pax;              /* passenger id, 0 = everyone              */
    int        flight;
    float      atMin;
    int        read;
    char       text[150];
} Notification;

typedef struct {
    Notification n[MAX_NOTIFY];
    int          count;          /* live entries, oldest dropped first      */
    int          nextId;
    int          sent;
    /*  What the notifier has already told each flight's passengers, so a
     *  gate change is announced once rather than sixty times a second.     */
    int          lastGate  [MAX_FLIGHTS];
    int          lastDelay [MAX_FLIGHTS];
    int          lastState [MAX_FLIGHTS];
    int          primed;
} NotifyCentre;

void  nt_init  (NotifyCentre *N);
int   nt_push  (NotifyCentre *N, World *w, NotifyKind k, int pax, int flight,
                const char *text);
/* Watches every flight and raises notifications when something changes. */
void  nt_scan  (NotifyCentre *N, World *w);
int   nt_for_pax(const NotifyCentre *N, int pax, int *out, int max);
int   nt_unread(const NotifyCentre *N, int pax);
void  nt_mark_read(NotifyCentre *N, int pax);
const char *nt_kind_name(NotifyKind k);

#endif /* AURA_INCIDENT_H */
