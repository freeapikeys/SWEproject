/* ==========================================================================
 *  AURA :: model.h  --  the domain of Sir Seewoosagur Ramgoolam
 *                        International Airport (MRU / FIMP), Plaisance
 *
 *  Everything the operations floor cares about is described here: the
 *  airlines that serve Mauritius, the aircraft they fly, the stands they
 *  park on, the passengers who travel and the bags that follow them.
 *
 *  Geometry note: the airfield is modelled in "field units" on a 1000 x 640
 *  grid laid over the real Plaisance layout -- a single 3,390 m runway
 *  oriented 14/32, a parallel taxiway to the north-west, and the 2013
 *  terminal with its contact stands facing the apron.
 * ========================================================================== */
#ifndef AURA_MODEL_H
#define AURA_MODEL_H

#include "../engine/canvas.h"

/* ------------------------------------------------------------- constants -- */

#define MAX_AIRLINES     14
#define MAX_ACTYPES      14
#define MAX_AIRPORTS     34
#define MAX_FLIGHTS     120
#define MAX_STANDS       26
#define MAX_PASSENGERS  520
#define MAX_BAGS        620
#define MAX_STAFF        30
#define MAX_DESKS        24
#define MAX_LOG        1200

/* ---------------------------------------------------------------- lookup -- */

typedef struct {
    char  iata[4];
    char  icao[5];
    char  name[36];
    Color col;
    char  hub[28];
} Airline;

typedef struct {
    char  code[6];       /* 359, 333, 77W, AT7 ... */
    char  name[30];
    int   seats;
    int   rangeKm;
    float wingspan;      /* metres -- drives the stand compatibility rules  */
    int   uld;           /* container positions                             */
    int   widebody;
} AcType;

typedef struct {
    char  iata[4];
    char  city[26];
    char  country[26];
    float lat, lon;
    int   flightMin;     /* block time from MRU, minutes                    */
} Airport;

/* ---------------------------------------------------------------- stands -- */

typedef enum { ST_CONTACT = 0, ST_REMOTE, ST_CARGO } StandKind;

typedef struct {
    char      name[6];      /* A1 .. A7, R1 .. R8, C1 ...                   */
    StandKind kind;
    float     x, y;         /* parked centre, field units                   */
    float     heading;      /* parked heading, radians                      */
    float     maxSpan;      /* largest wingspan accepted, metres            */
    int       jetbridge;
    int       gate;         /* boarding gate number, 0 if none              */
    float     entryX, entryY;   /* taxiway node the stand hangs off         */
} Stand;

/* --------------------------------------------------------------- flights -- */

typedef enum {
    FS_SCHEDULED = 0, FS_CHECKIN, FS_BOARDING, FS_FINAL, FS_CLOSED,
    FS_PUSHBACK, FS_TAXI_OUT, FS_LINEUP, FS_DEPARTED,
    FS_ENROUTE, FS_APPROACH, FS_LANDED, FS_TAXI_IN, FS_ONBLOCK,
    FS_DELAYED, FS_CANCELLED,
    FS_COUNT
} FlightState;

/* where an aircraft physically is, for the airfield view */
typedef enum {
    AP_NONE = 0, AP_STAND, AP_TAXI_OUT, AP_RUNWAY, AP_CLIMB,
    AP_APPROACH, AP_ROLLOUT, AP_TAXI_IN
} AcPhase;

typedef struct {
    int   id;
    char  no[8];             /* MK046                                       */
    int   airline;
    int   acType;
    char  reg[10];           /* 3B-NBQ                                      */
    int   airport;           /* the other end of the leg                    */
    int   arrival;           /* 1 = inbound to MRU                          */

    int   schedMin;          /* scheduled, minutes past local midnight      */
    int   estMin;            /* current estimate                            */
    int   delayMin;
    FlightState state;

    int   stand;             /* index into Stand[], -1 when unassigned      */
    int   gate;
    char  belt[4];           /* arrivals reclaim belt                       */
    int   desks[4];          /* check-in desk numbers                       */
    int   nDesks;

    int   paxCap, pax, checkedIn, boarded;
    int   bags, bagsLoaded, bagsScreened;

    /* AI outputs */
    float delayRisk;         /* 0..1 from the logistic model                */
    float riskFeat[6];       /* per-feature contribution, for explanation   */
    int   aiStandSuggest;

    /* live movement */
    AcPhase phase;
    float   px, py, heading, speed;
    float   pathT;           /* progress along the current path            */
    int     pathId;
    float   wheelSmoke;      /* touchdown puff timer                        */
    int     strobe;
} Flight;

/* ------------------------------------------------------------ passengers -- */

typedef struct {
    int   id;
    char  name[38];
    char  pnr[8];
    int   flight;
    char  seat[5];
    int   bags;
    int   checkedIn, boarded, security;
    int   fastTrack, wheelchair, infant;
    int   nationality;       /* airport index of residence                  */
    int   loyalty;           /* 0 none, 1 silver, 2 gold, 3 platinum        */
} Passenger;

/* ------------------------------------------------------------------ bags -- */

typedef enum {
    BG_CHECKIN = 0, BG_SCREEN, BG_SORT, BG_MAKEUP, BG_LOADED,
    BG_RECLAIM, BG_DELIVERED, BG_HELD, BG_MISHANDLED,
    BG_COUNT
} BagState;

typedef struct {
    int      id;
    char     tag[12];        /* MK123456                                    */
    int      pax;
    int      flight;
    BagState state;
    float    weight;
    int      lane;           /* which conveyor lane it rides                */
    float    t;              /* 0..1 along the lane                         */
    float    releaseMin;     /* clock time this bag enters the system       */
    float    x, y;           /* live position for the animation             */
    float    wobble;
    int      threat;         /* 1 = flagged by the classifier               */
    float    threatScore;
    int      inspected;
    int      priority;
    int      colour;
} Bag;

/* ----------------------------------------------------------------- staff -- */

typedef struct {
    char name[30];
    char role[26];
    int  shift;              /* 0 = early, 1 = late, 2 = night              */
    int  onDuty;
    int  station;
} Staff;

/* --------------------------------------------------------------- desks ---- */

typedef struct {
    int  number;
    int  open;
    int  flight;
    int  queue;
    float served;
} Desk;

/* ------------------------------------------------------------------- log -- */

typedef enum { LG_INFO = 0, LG_OK, LG_WARN, LG_ERR, LG_AI } LogKind;

typedef struct {
    int  minute;
    int  kind;
    char text[110];
} LogEntry;

/* ============================================================== the world = */

typedef struct {
    /* reference data */
    Airline  airline[MAX_AIRLINES];   int nAirlines;
    AcType   actype [MAX_ACTYPES];    int nActypes;
    Airport  airport[MAX_AIRPORTS];   int nAirports;
    Stand    stand  [MAX_STANDS];     int nStands;

    /* live data */
    Flight    flight[MAX_FLIGHTS];    int nFlights;
    Passenger pax   [MAX_PASSENGERS]; int nPax;
    Bag       bag   [MAX_BAGS];       int nBags;
    Staff     staff [MAX_STAFF];      int nStaff;
    Desk      desk  [MAX_DESKS];      int nDesks;

    LogEntry  log[MAX_LOG];           int nLog;

    /* clock */
    float    clock;          /* minutes past local midnight, fractional     */
    float    speed;          /* simulation multiplier                       */
    int      paused;
    int      day, month, year;

    /* weather at Plaisance */
    float    tempC, windKt, windDir, visKm, qnh, humidity;
    int      wxKind;         /* 0 clear 1 partly 2 cloud 3 shower 4 storm   */
    int      activeRunway;   /* 14 or 32                                    */

    /* rolling counters */
    int      totalDepartures, totalArrivals, totalPaxToday, totalBagsToday;
    int      bagsMishandled, securityAlerts;
    uint32_t rng;
} World;

/* ------------------------------------------------------------------- api -- */

void  world_init      (World *w);
void  world_seed_data (World *w);           /* reference tables            */
void  world_generate  (World *w);           /* a full day of operations    */

const char *fs_name   (FlightState s);
Color       fs_color  (FlightState s);
const char *bs_name   (BagState s);
int         mins_now  (const World *w);
void        fmt_hhmm  (int minutes, char *out);
void        fmt_hhmmss(float minutes, char *out);
const char *wx_name   (int kind);

Flight    *flight_by_no(World *w, const char *no);
Flight    *flight_by_id(World *w, int id);
Passenger *pax_by_pnr  (World *w, const char *pnr);
Bag       *bag_by_tag  (World *w, const char *tag);
Stand     *stand_by_name(World *w, const char *name);

void  world_log(World *w, int kind, const char *fmt, ...);

/* airfield geometry ------------------------------------------------------- */
#define FIELD_W 1000.f
#define FIELD_H  640.f

void  field_runway_ends(const World *w, float *x0,float *y0,float *x1,float *y1);
int   field_taxi_path(const World *w, int standIdx, int departing,
                      float *outXY, int maxPts);

#endif /* AURA_MODEL_H */
