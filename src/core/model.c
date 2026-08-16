/* ==========================================================================
 *  AURA :: model.c  --  reference tables and day generation
 * ========================================================================== */

#include "model.h"
#include "../engine/anim.h"
#include "../../include/theme.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 *  reference data
 * ========================================================================== */

static void seed_airlines(World *w)
{
    struct { const char *ia, *ic, *nm, *hub; uint32_t col; } A[] = {
        { "MK","MAU","Air Mauritius",       "Plaisance",     0xC8102E },
        { "EK","UAE","Emirates",            "Dubai",         0xD71921 },
        { "AF","AFR","Air France",          "Paris CDG",     0x002157 },
        { "BA","BAW","British Airways",     "London LHR",    0x1B4A8B },
        { "TK","THY","Turkish Airlines",    "Istanbul",      0xC70A0C },
        { "KQ","KQA","Kenya Airways",       "Nairobi",       0xB01C2E },
        { "4Z","LNK","Airlink",             "Johannesburg",  0x0F4C81 },
        { "UU","REU","Air Austral",         "Saint-Denis",   0xE2001A },
        { "SS","CRL","Corsair International","Paris ORY",    0xE30613 },
        { "6E","IGO","IndiGo",              "Mumbai",        0x001B94 },
        { "SV","SVA","Saudia",              "Jeddah",        0x006C35 },
        { "DE","CFG","Condor",              "Frankfurt",     0xF7C600 },
        { "HM","SEY","Air Seychelles",      "Mahe",          0xE4002B },
        { "MD","MDG","Madagascar Airlines", "Antananarivo",  0x00843D },
    };
    w->nAirlines = (int)(sizeof A / sizeof A[0]);
    for (int i = 0; i < w->nAirlines; i++) {
        snprintf(w->airline[i].iata, 4, "%s", A[i].ia);
        snprintf(w->airline[i].icao, 5, "%s", A[i].ic);
        snprintf(w->airline[i].name, 36, "%s", A[i].nm);
        snprintf(w->airline[i].hub,  28, "%s", A[i].hub);
        w->airline[i].col = HEX(A[i].col);
    }
}

static void seed_actypes(World *w)
{
    struct { const char *cd, *nm; int seats, range; float span; int uld, wide; } T[] = {
        { "359","Airbus A350-900",       326, 15000, 64.75f, 36, 1 },
        { "339","Airbus A330-900neo",    288, 13300, 64.00f, 33, 1 },
        { "333","Airbus A330-300",       298, 11750, 60.30f, 33, 1 },
        { "332","Airbus A330-200",       247, 13450, 60.30f, 26, 1 },
        { "77W","Boeing 777-300ER",      354, 13650, 64.80f, 44, 1 },
        { "789","Boeing 787-9",          290, 14140, 60.12f, 36, 1 },
        { "788","Boeing 787-8",          256, 13530, 60.12f, 28, 1 },
        { "35K","Airbus A350-1000",      369, 16100, 64.75f, 40, 1 },
        { "738","Boeing 737-800",        189,  5765, 35.80f,  0, 0 },
        { "32N","Airbus A320neo",        186,  6300, 35.80f,  0, 0 },
        { "321","Airbus A321neo",        220,  7400, 35.80f,  0, 0 },
        { "320","Airbus A320",           180,  6100, 35.80f,  0, 0 },
        { "AT7","ATR 72-500",             72,  1500, 27.05f,  0, 0 },
        { "E90","Embraer 190",           104,  4500, 28.72f,  0, 0 },
    };
    w->nActypes = (int)(sizeof T / sizeof T[0]);
    for (int i = 0; i < w->nActypes; i++) {
        snprintf(w->actype[i].code, 6, "%s", T[i].cd);
        snprintf(w->actype[i].name, 30, "%s", T[i].nm);
        w->actype[i].seats    = T[i].seats;
        w->actype[i].rangeKm  = T[i].range;
        w->actype[i].wingspan = T[i].span;
        w->actype[i].uld      = T[i].uld;
        w->actype[i].widebody = T[i].wide;
    }
}

static void seed_airports(World *w)
{
    struct { const char *ia, *ct, *co; float la, lo; int mn; } P[] = {
        { "RRG","Rodrigues",     "Mauritius",     -19.757f,  63.361f,  95 },
        { "RUN","Saint-Denis",   "Reunion",       -20.887f,  55.513f,  50 },
        { "TNR","Antananarivo",  "Madagascar",    -18.797f,  47.479f, 110 },
        { "SEZ","Mahe",          "Seychelles",     -4.674f,  55.522f, 180 },
        { "JNB","Johannesburg",  "South Africa",  -26.139f,  28.246f, 245 },
        { "CPT","Cape Town",     "South Africa",  -33.965f,  18.602f, 305 },
        { "DUR","Durban",        "South Africa",  -29.615f,  31.120f, 260 },
        { "NBO","Nairobi",       "Kenya",          -1.319f,  36.928f, 300 },
        { "DXB","Dubai",         "UAE",            25.253f,  55.365f, 395 },
        { "CDG","Paris",         "France",         49.010f,   2.548f, 725 },
        { "ORY","Paris Orly",    "France",         48.723f,   2.379f, 730 },
        { "LHR","London",        "United Kingdom", 51.470f,  -0.454f, 750 },
        { "LGW","London Gatwick","United Kingdom", 51.148f,  -0.190f, 755 },
        { "IST","Istanbul",      "Turkiye",        41.262f,  28.742f, 635 },
        { "BOM","Mumbai",        "India",          19.089f,  72.868f, 360 },
        { "DEL","Delhi",         "India",          28.556f,  77.100f, 425 },
        { "MAA","Chennai",       "India",          12.994f,  80.171f, 350 },
        { "BLR","Bengaluru",     "India",          13.199f,  77.710f, 345 },
        { "KUL","Kuala Lumpur",  "Malaysia",        2.746f, 101.710f, 420 },
        { "SIN","Singapore",     "Singapore",       1.364f, 103.991f, 425 },
        { "HKG","Hong Kong",     "Hong Kong",      22.308f, 113.918f, 540 },
        { "PVG","Shanghai",      "China",          31.143f, 121.805f, 690 },
        { "PER","Perth",         "Australia",     -31.940f, 115.967f, 425 },
        { "MEL","Melbourne",     "Australia",     -37.669f, 144.841f, 575 },
        { "JED","Jeddah",        "Saudi Arabia",   21.680f,  39.157f, 480 },
        { "FRA","Frankfurt",     "Germany",        50.033f,   8.570f, 695 },
        { "MUC","Munich",        "Germany",        48.354f,  11.786f, 690 },
        { "GVA","Geneva",        "Switzerland",    46.238f,   6.109f, 690 },
        { "ZRH","Zurich",        "Switzerland",    47.464f,   8.549f, 685 },
        { "VIE","Vienna",        "Austria",        48.110f,  16.570f, 665 },
        { "AMS","Amsterdam",     "Netherlands",    52.310f,   4.768f, 705 },
        { "FCO","Rome",          "Italy",          41.800f,  12.239f, 665 },
        { "MXP","Milan",         "Italy",          45.630f,   8.723f, 670 },
        { "MRU","Plaisance",     "Mauritius",     -20.430f,  57.683f,   0 },
    };
    w->nAirports = (int)(sizeof P / sizeof P[0]);
    for (int i = 0; i < w->nAirports; i++) {
        snprintf(w->airport[i].iata,    4, "%s", P[i].ia);
        snprintf(w->airport[i].city,   26, "%s", P[i].ct);
        snprintf(w->airport[i].country,26, "%s", P[i].co);
        w->airport[i].lat = P[i].la;
        w->airport[i].lon = P[i].lo;
        w->airport[i].flightMin = P[i].mn;
    }
}

/* --------------------------------------------------------------- stands --- */
/*  Field units: runway axis runs (120,200) -> (900,340); the parallel taxiway
 *  sits 100 units to the apron side; the terminal frontage is at y ~ 545.    */

static void seed_stands(World *w)
{
    struct { const char *nm; int kind; float x,y,hd,span; int jb,gate; } S[] = {
        { "A1", ST_CONTACT, 300, 470, 90, 65.f, 1,  1 },
        { "A2", ST_CONTACT, 372, 470, 90, 65.f, 1,  2 },
        { "A3", ST_CONTACT, 444, 470, 90, 65.f, 1,  3 },
        { "A4", ST_CONTACT, 516, 470, 90, 65.f, 1,  4 },
        { "A5", ST_CONTACT, 588, 470, 90, 52.f, 1,  5 },
        { "A6", ST_CONTACT, 660, 470, 90, 40.f, 1,  6 },
        { "A7", ST_CONTACT, 726, 470, 90, 40.f, 1,  7 },
        { "A8", ST_CONTACT, 786, 470, 90, 36.f, 1,  8 },
        { "R1", ST_REMOTE,  196, 496, 60, 65.f, 0, 21 },
        { "R2", ST_REMOTE,  196, 556, 60, 65.f, 0, 22 },
        { "R3", ST_REMOTE,  128, 496, 60, 40.f, 0, 23 },
        { "R4", ST_REMOTE,  128, 556, 60, 40.f, 0, 24 },
        { "R5", ST_REMOTE,  862, 500,120, 40.f, 0, 25 },
        { "R6", ST_REMOTE,  862, 560,120, 40.f, 0, 26 },
        { "R7", ST_REMOTE,  930, 500,120, 36.f, 0, 27 },
        { "R8", ST_REMOTE,  930, 560,120, 30.f, 0, 28 },
        { "R9", ST_REMOTE,   64, 470, 60, 65.f, 0, 29 },
        { "R10",ST_REMOTE,   64, 540, 60, 65.f, 0, 30 },
        { "R11",ST_REMOTE,  982, 500,120, 45.f, 0, 31 },
        { "R12",ST_REMOTE,  982, 560,120, 45.f, 0, 32 },
        { "R13",ST_REMOTE,   64, 610, 60, 65.f, 0, 33 },
        { "R14",ST_REMOTE,  132, 610, 60, 65.f, 0, 34 },
        { "C1", ST_CARGO,    62, 420,  0, 60.f, 0,  0 },
        { "C2", ST_CARGO,    62, 356,  0, 60.f, 0,  0 },
    };
    w->nStands = (int)(sizeof S / sizeof S[0]);
    for (int i = 0; i < w->nStands; i++) {
        Stand *s = &w->stand[i];
        snprintf(s->name, 6, "%s", S[i].nm);
        s->kind      = (StandKind)S[i].kind;
        s->x         = S[i].x;
        s->y         = S[i].y;
        s->heading   = (float)(S[i].hd * M_PI / 180.0);
        s->maxSpan   = S[i].span;
        s->jetbridge = S[i].jb;
        s->gate      = S[i].gate;
        /* Link taxiway node: the perpendicular foot of the stand on the
         * parallel taxiway.  A right-angled link is both how it is built on
         * the ground and what stops the map reading as a row of bars. */
        const float px0 = 120.f, py0 = 300.f;      /* taxiway origin       */
        const float ux  = 0.98413f, uy = 0.17734f; /* unit direction       */
        float rx = S[i].x - px0, ry = S[i].y - py0;
        float t  = rx*ux + ry*uy;
        s->entryX = px0 + ux*t;
        s->entryY = py0 + uy*t;
    }
}

/* ==========================================================================
 *  names
 * ========================================================================== */

static const char *FIRST[] = {
    "Aditya","Ashvin","Bhavna","Chandra","Devina","Emilie","Fabrice","Gopal",
    "Hansley","Ishwar","Jean-Marc","Kavi","Laetitia","Marie-Claire","Nadia",
    "Olivier","Priya","Quentin","Rajesh","Sandrine","Thierry","Uma","Vikram",
    "Wesley","Yashna","Zaheer","Aisha","Brenda","Clency","Dinesh","Estelle",
    "Farhad","Ghislaine","Hemant","Ismael","Jyoti","Krishen","Lindsay",
    "Manisha","Nitin","Odile","Pravin","Reshma","Sanjay","Tania","Vinod",
    "Anjali","Bruno","Cedric","Doorgesh","Eshan","Fabiola","Geeta","Harold",
    "Ivan","Jessica","Kiran","Loic","Melissa","Nishal","Patrice","Rachel",
    "Sameer","Trisha","Veronique","Yannick","Zubair","Amelie","Damien","Kavita"
};

static const char *LAST[] = {
    "Ramgoolam","Beeharry","Jugnauth","Appadoo","Seetohul","Bhundoo","Narain",
    "Li Ying Pin","Ah Kong","Rughoonundun","Bissessur","Chinien","Nundlall",
    "Gungapersad","Moutou","Perrine","Lafleur","Adrien","Volbert","Sooprayen",
    "Vydelingum","Ramdhany","Curpen","Hurloll","Jhurry","Mungra","Boodhoo",
    "Dabee","Ellayah","Fakim","Gopaul","Hossen","Imrith","Joomun",
    "Kissoondoyal","Luchmun","Maunthrooa","Nagalingum","Oree","Pillay",
    "Ramsamy","Sewnath","Toolsee","Uteem","Veerapen","Woodun","Bhugaloo",
    "Cangy","Desvaux de Marigny","Espitalier-Noel","Foolchand","Gujadhur",
    "Hurdoyal","Itoola","Jeetun","Koonjul","Laval","Mudhoo","Neerunjun",
    "Purmessur","Quirin","Rasoanaivo","Sunassee","Tirvengadum","Valaydon"
};

static const char *ROLES[] = {
    "Duty Terminal Manager","Apron Controller","Ramp Supervisor",
    "Check-in Agent","Baggage Handler","Security Screener","Load Controller",
    "Gate Agent","Refuelling Operative","Marshaller","Dispatcher",
    "Baggage Reconciliation","Customer Service","Operations Officer"
};

/* ==========================================================================
 *  helpers
 * ========================================================================== */

static const char *FS_NAMES[FS_COUNT] = {
    "Scheduled","Check-in Open","Boarding","Final Call","Gate Closed",
    "Pushback","Taxiing","Line Up","Departed",
    "En Route","On Approach","Landed","Taxiing In","On Blocks",
    "Delayed","Cancelled"
};

const char *fs_name(FlightState s)
{
    if (s < 0 || s >= FS_COUNT) return "Unknown";
    return FS_NAMES[s];
}

Color fs_color(FlightState s)
{
    switch (s) {
    case FS_BOARDING: case FS_CHECKIN:                  return C_OK;
    case FS_FINAL:                                       return C_WARN;
    case FS_CLOSED: case FS_CANCELLED:                   return C_DANGER;
    case FS_DELAYED:                                     return C_DANGER;
    case FS_DEPARTED: case FS_ENROUTE: case FS_TAXI_OUT:
    case FS_PUSHBACK: case FS_LINEUP:                    return C_INFO;
    case FS_LANDED: case FS_ONBLOCK: case FS_TAXI_IN:    return C_TEAL;
    case FS_APPROACH:                                    return C_MAGENTA;
    default:                                             return C_INK_3;
    }
}

static const char *BS_NAMES[BG_COUNT] = {
    "At Check-in","In Screening","Sorting","Make-up","Loaded",
    "On Reclaim","Delivered","Held","Mishandled"
};

const char *bs_name(BagState s)
{
    if (s < 0 || s >= BG_COUNT) return "Unknown";
    return BS_NAMES[s];
}

void fmt_hhmm(int m, char *out)
{
    while (m < 0)     m += 1440;
    while (m >= 1440) m -= 1440;
    sprintf(out, "%02d:%02d", m / 60, m % 60);
}

void fmt_hhmmss(float fm, char *out)
{
    int total = (int)(fm * 60.f);
    while (total < 0)       total += 86400;
    while (total >= 86400)  total -= 86400;
    sprintf(out, "%02d:%02d:%02d", total/3600, (total/60)%60, total%60);
}

int mins_now(const World *w) { return (int)w->clock; }

const char *wx_name(int k)
{
    switch (k) {
    case 0: return "Clear";
    case 1: return "Partly cloudy";
    case 2: return "Cloudy";
    case 3: return "Passing shower";
    default:return "Thunderstorm";
    }
}

Flight *flight_by_no(World *w, const char *no)
{
    for (int i = 0; i < w->nFlights; i++)
        if (_stricmp(w->flight[i].no, no) == 0) return &w->flight[i];
    return NULL;
}

Flight *flight_by_id(World *w, int id)
{
    for (int i = 0; i < w->nFlights; i++)
        if (w->flight[i].id == id) return &w->flight[i];
    return NULL;
}

Passenger *pax_by_pnr(World *w, const char *pnr)
{
    for (int i = 0; i < w->nPax; i++)
        if (_stricmp(w->pax[i].pnr, pnr) == 0) return &w->pax[i];
    return NULL;
}

Bag *bag_by_tag(World *w, const char *tag)
{
    for (int i = 0; i < w->nBags; i++)
        if (_stricmp(w->bag[i].tag, tag) == 0) return &w->bag[i];
    return NULL;
}

Stand *stand_by_name(World *w, const char *nm)
{
    for (int i = 0; i < w->nStands; i++)
        if (_stricmp(w->stand[i].name, nm) == 0) return &w->stand[i];
    return NULL;
}

void world_log(World *w, int kind, const char *fmt, ...)
{
    if (w->nLog >= MAX_LOG) {
        memmove(&w->log[0], &w->log[1], sizeof(LogEntry) * (MAX_LOG - 1));
        w->nLog = MAX_LOG - 1;
    }
    LogEntry *e = &w->log[w->nLog++];
    e->minute = (int)w->clock;
    e->kind   = kind;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text, sizeof e->text, fmt, ap);
    va_end(ap);
}

/* ==========================================================================
 *  airfield geometry
 * ========================================================================== */

#define RWY_X0 120.f
#define RWY_Y0 200.f
#define RWY_X1 900.f
#define RWY_Y1 340.f
#define TWY_OFF 100.f

void field_runway_ends(const World *w, float *x0,float *y0,float *x1,float *y1)
{
    (void)w;
    *x0 = RWY_X0; *y0 = RWY_Y0; *x1 = RWY_X1; *y1 = RWY_Y1;
}

static void twy_point(float t, float *x, float *y)
{
    *x = RWY_X0 + (RWY_X1 - RWY_X0) * t;
    *y = RWY_Y0 + (RWY_Y1 - RWY_Y0) * t + TWY_OFF;
}

/* Build the polyline an aircraft follows.  Departing runs stand -> link ->
 * parallel taxiway -> holding point -> runway roll -> climb.  Arriving is the
 * mirror image, entering at a rapid exit two thirds along the runway. */
int field_taxi_path(const World *w, int standIdx, int departing,
                    float *out, int maxPts)
{
    if (standIdx < 0 || standIdx >= w->nStands) return 0;
    const Stand *s = &w->stand[standIdx];
    float p[40][2];
    int n = 0;

    float standT = (s->x - RWY_X0) / (RWY_X1 - RWY_X0);
    if (standT < 0.06f) standT = 0.06f;
    if (standT > 0.94f) standT = 0.94f;

    if (departing) {
        p[n][0] = s->x;      p[n][1] = s->y;                       n++;
        p[n][0] = s->x;      p[n][1] = s->y - 34.f;                n++;
        p[n][0] = s->entryX; p[n][1] = s->entryY;                  n++;
        /* run along the parallel taxiway to the threshold in use */
        float endT = (w->activeRunway == 14) ? 0.045f : 0.955f;
        int steps = 5;
        for (int i = 1; i <= steps; i++) {
            float t = standT + (endT - standT) * ((float)i / steps);
            twy_point(t, &p[n][0], &p[n][1]); n++;
        }
        /* holding point, then line up on the centreline */
        float rx, ry;
        rx = RWY_X0 + (RWY_X1 - RWY_X0)*endT;
        ry = RWY_Y0 + (RWY_Y1 - RWY_Y0)*endT;
        p[n][0] = rx; p[n][1] = ry; n++;
        /* takeoff roll to the far end and rotate */
        float offT = (w->activeRunway == 14) ? 0.92f : 0.08f;
        p[n][0] = RWY_X0 + (RWY_X1-RWY_X0)*offT;
        p[n][1] = RWY_Y0 + (RWY_Y1-RWY_Y0)*offT; n++;
        /* climb out beyond the field */
        float dx = (w->activeRunway == 14) ? 1.f : -1.f;
        p[n][0] = p[n-1][0] + dx*420.f;
        p[n][1] = p[n-1][1] + dx*76.f - 150.f; n++;
    } else {
        /* long final, aligned with the runway centreline */
        float thrT = (w->activeRunway == 14) ? 0.02f : 0.98f;
        float dx   = (w->activeRunway == 14) ? -1.f : 1.f;
        p[n][0] = RWY_X0 + (RWY_X1-RWY_X0)*thrT + dx*520.f;
        p[n][1] = RWY_Y0 + (RWY_Y1-RWY_Y0)*thrT + dx*94.f - 190.f; n++;
        p[n][0] = RWY_X0 + (RWY_X1-RWY_X0)*thrT;
        p[n][1] = RWY_Y0 + (RWY_Y1-RWY_Y0)*thrT; n++;
        /* rollout, then vacate */
        float exitT = (w->activeRunway == 14) ? 0.62f : 0.38f;
        p[n][0] = RWY_X0 + (RWY_X1-RWY_X0)*exitT;
        p[n][1] = RWY_Y0 + (RWY_Y1-RWY_Y0)*exitT; n++;
        twy_point(exitT, &p[n][0], &p[n][1]); n++;
        int steps = 4;
        for (int i = 1; i <= steps; i++) {
            float t = exitT + (standT - exitT) * ((float)i / steps);
            twy_point(t, &p[n][0], &p[n][1]); n++;
        }
        p[n][0] = s->entryX; p[n][1] = s->entryY;   n++;
        p[n][0] = s->x;      p[n][1] = s->y - 34.f; n++;
        p[n][0] = s->x;      p[n][1] = s->y;        n++;
    }

    if (n > maxPts) n = maxPts;
    for (int i = 0; i < n; i++) { out[i*2] = p[i][0]; out[i*2+1] = p[i][1]; }
    return n;
}

/* ==========================================================================
 *  world generation
 * ========================================================================== */

void world_init(World *w)
{
    memset(w, 0, sizeof *w);
    w->rng   = 0xA17C0DEu;
    w->speed = 8.f;
    w->clock = 10.f * 60.f + 10.f;
    w->day = 14; w->month = 8; w->year = 2026;
    w->tempC = 24.6f; w->windKt = 12.f; w->windDir = 135.f;
    w->visKm = 10.f;  w->qnh = 1017.f;  w->humidity = 74.f;
    w->wxKind = 1;
    w->activeRunway = 14;
    world_seed_data(w);
    world_generate(w);
}

void world_seed_data(World *w)
{
    seed_airlines(w);
    seed_actypes(w);
    seed_airports(w);
    seed_stands(w);
}

static int ap_index(World *w, const char *iata)
{
    for (int i = 0; i < w->nAirports; i++)
        if (strcmp(w->airport[i].iata, iata) == 0) return i;
    return 0;
}

static int al_index(World *w, const char *iata)
{
    for (int i = 0; i < w->nAirlines; i++)
        if (strcmp(w->airline[i].iata, iata) == 0) return i;
    return 0;
}

static int ac_index(World *w, const char *code)
{
    for (int i = 0; i < w->nActypes; i++)
        if (strcmp(w->actype[i].code, code) == 0) return i;
    return 0;
}

/* a plausible registration for the operator */
static void make_reg(World *w, int airline, char *out, uint32_t *rng)
{
    const char *ia = w->airline[airline].iata;
    if (strcmp(ia, "MK") == 0) {
        /* 3B-Nxx.  This has to be drawn from a pool large enough that the
         * ~19 Air Mauritius turnarounds each get their own tail: if two
         * rotations share a registration, every rule that pairs an inbound
         * with its outbound by tail pairs the wrong two aeroplanes. */
        sprintf(out, "3B-N%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "EK") == 0) {
        sprintf(out, "A6-E%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "AF") == 0 || strcmp(ia, "SS") == 0) {
        sprintf(out, "F-G%c%c%c", 'H'+rnd_int(rng,0,8), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "BA") == 0) {
        sprintf(out, "G-%c%c%c%c", 'V'+rnd_int(rng,0,4), 'I'+rnd_int(rng,0,8),
                'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "TK") == 0) {
        sprintf(out, "TC-L%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "4Z") == 0) {
        sprintf(out, "ZS-%c%c%c", 'S'+rnd_int(rng,0,5), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "UU") == 0) {
        sprintf(out, "F-O%c%c%c", 'J'+rnd_int(rng,0,9), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "6E") == 0) {
        sprintf(out, "VT-I%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "KQ") == 0) {
        sprintf(out, "5Y-K%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "SV") == 0) {
        sprintf(out, "HZ-A%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "DE") == 0) {
        sprintf(out, "D-A%c%c%c", 'I'+rnd_int(rng,0,8), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "HM") == 0) {
        sprintf(out, "S7-A%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else {
        sprintf(out, "5R-%c%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    }
}

/* --------------------------------------------------------------------------
 *  The published Plaisance pattern: European long-haul lands at first light,
 *  the regional wave works through the middle of the day, and the long-haul
 *  departures stack up between 20:00 and midnight.
 * ------------------------------------------------------------------------- */

typedef struct {
    const char *al; const char *num; const char *ap; const char *ac;
    int arrMin;     /* arrival time at MRU  */
    int depMin;     /* departure time from MRU (paired turnaround) */
    const char *depNum;
} RouteRow;

static const RouteRow ROUTES[] = {
    /* airline, arr flight, destination, type, arrive, depart, dep flight  */
    { "MK","MK015","LHR","359",  345,  1305, "MK014" },
    { "AF","AF464","CDG","77W",  375,  1355, "AF463" },
    { "EK","EK701","DXB","77W",  405,   700, "EK702" },
    { "TK","TK741","IST","789",  430,  1310, "TK742" },
    { "MK","MK047","JNB","332",  455,   580, "MK048" },
    { "MK","MK201","RRG","AT7",  470,   555, "MK202" },
    { "UU","UU974","RUN","320",  495,   580, "UU975" },
    { "MK","MK041","BOM","339",  510,  1250, "MK042" },
    { "4Z","4Z252","JNB","E90",  540,   625, "4Z253" },
    { "MK","MK203","RRG","AT7",  585,   670, "MK204" },
    { "SS","SS910","ORY","332",  600,  1330, "SS911" },
    { "KQ","KQ276","NBO","738",  625,   715, "KQ277" },
    { "MK","MK051","TNR","AT7",  640,   735, "MK052" },
    { "6E","6E065","BOM","32N",  660,   750, "6E066" },
    { "MK","MK205","RRG","AT7",  700,   785, "MK206" },
    { "MD","MD731","TNR","AT7",  720,   810, "MD732" },
    { "UU","UU976","RUN","320",  745,   830, "UU977" },
    { "HM","HM062","SEZ","320",  770,   860, "HM063" },
    { "MK","MK049","DUR","332",  790,   885, "MK050" },
    { "MK","MK207","RRG","AT7",  815,   900, "MK208" },
    { "4Z","4Z254","JNB","E90",  845,   930, "4Z255" },
    { "MK","MK043","DEL","339",  870,  1265, "MK044" },
    { "UU","UU978","RUN","320",  900,   985, "UU979" },
    { "DE","DE2314","FRA","333", 930,  1290, "DE2315" },
    { "MK","MK209","RRG","AT7",  950,  1035, "MK210" },
    { "SV","SV831","JED","789",  985,  1320, "SV832" },
    { "MK","MK045","CPT","332", 1010,  1105, "MK046" },
    { "BA","BA2065","LGW","789",1040,  1345, "BA2066" },
    { "MK","MK211","RRG","AT7", 1065,  1150, "MK212" },
    { "MK","MK017","CDG","359", 1090,  1300, "MK016" },
    { "EK","EK703","DXB","77W", 1120,  1400, "EK704" },
    { "MK","MK053","SEZ","AT7", 1145,  1235, "MK054" },
    { "4Z","4Z256","JNB","E90", 1170,  1255, "4Z257" },
    { "MK","MK031","KUL","339", 1195,  1385, "MK032" },
    { "MK","MK213","RRG","AT7", 1220,  1310, "MK214" },
    { "MK","MK055","PER","332", 1250,  1420, "MK056" },
};

static void gen_flight(World *w, const RouteRow *r, int arrival, int *idc,
                       const char *reg)
{
    if (w->nFlights >= MAX_FLIGHTS) return;
    Flight *f = &w->flight[w->nFlights++];
    memset(f, 0, sizeof *f);
    f->id      = (*idc)++;
    f->airline = al_index(w, r->al);
    f->acType  = ac_index(w, r->ac);
    f->airport = ap_index(w, r->ap);
    f->arrival = arrival;
    snprintf(f->no, 8, "%s", arrival ? r->num : r->depNum);
    /* The inbound and the outbound are the same aeroplane, so they carry the
     * same tail.  Generating a registration per leg breaks every downstream
     * rule that reasons about a turnaround: stand pairing, tow counting and
     * the "inbound running late" feature of the delay model. */
    snprintf(f->reg, sizeof f->reg, "%s", reg);
    f->schedMin = arrival ? r->arrMin : r->depMin;
    f->estMin   = f->schedMin;
    f->state    = FS_SCHEDULED;
    f->stand    = -1;
    f->phase    = AP_NONE;

    const AcType *t = &w->actype[f->acType];
    f->paxCap = t->seats;
    float load = rnd_range(&w->rng, 0.62f, 0.97f);
    f->pax    = (int)(t->seats * load);
    f->bags   = (int)(f->pax * rnd_range(&w->rng, 0.55f, 1.35f));

    /* a realistic scatter of delays */
    float roll = rnd_f(&w->rng);
    if (roll > 0.86f)      f->delayMin = rnd_int(&w->rng, 20, 95);
    else if (roll > 0.68f) f->delayMin = rnd_int(&w->rng, 5, 19);
    else                   f->delayMin = 0;
    f->estMin = f->schedMin + f->delayMin;

    if (arrival) {
        const char *belts[] = { "1","2","3","4","5" };
        snprintf(f->belt, 4, "%s", belts[rnd_int(&w->rng, 0, 4)]);
    } else {
        f->nDesks = t->widebody ? 4 : 2;
        int base = 1 + (w->nFlights * 3) % 20;
        for (int i = 0; i < f->nDesks; i++) f->desks[i] = base + i;
    }
}

/* An arrival and the departure it turns into are the same physical aeroplane,
 * so they occupy one stand for one continuous period.  Allocating them
 * separately doubles apparent demand and leaves half the programme without a
 * stand, which is what a naive first-fit does here. */
static void assign_stands(World *w)
{
    float busyUntil[MAX_STANDS];
    for (int i = 0; i < w->nStands; i++) busyUntil[i] = -1e9f;
    for (int i = 0; i < w->nFlights; i++) w->flight[i].stand = -1;

    /* work through the day in time order so the greedy pass behaves */
    int order[MAX_FLIGHTS], n = 0;
    for (int i = 0; i < w->nFlights; i++) if (w->flight[i].arrival) order[n++] = i;
    for (int i = 0; i < w->nFlights; i++) if (!w->flight[i].arrival) order[n++] = i;
    for (int i = 1; i < n; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && w->flight[order[j]].estMin > w->flight[k].estMin) {
            order[j+1] = order[j]; j--;
        }
        order[j+1] = k;
    }

    for (int oi = 0; oi < n; oi++) {
        int i = order[oi];
        Flight *f = &w->flight[i];
        if (f->stand >= 0) continue;

        /* find the other half of the turnaround, if there is one */
        Flight *pair = NULL;
        for (int j = 0; j < w->nFlights; j++) {
            Flight *g = &w->flight[j];
            if (g == f || g->arrival == f->arrival) continue;
            if (strcmp(g->reg, f->reg) != 0) continue;
            if (g->stand >= 0) continue;
            if (f->arrival ? (g->estMin > f->estMin) : (g->estMin < f->estMin)) {
                /* Only a genuine turnaround shares one stand.  A long-haul
                 * rotation sits on the ground for five hours or more; the
                 * aeroplane is towed to a remote parking stand in between
                 * rather than blocking a pier all morning, so the two legs
                 * are allocated independently. */
                int ground = f->arrival ? (g->estMin - f->estMin)
                                        : (f->estMin - g->estMin);
                if (ground <= 170) pair = g;
                break;
            }
        }

        float from = (float)(f->arrival ? f->estMin : f->estMin - 80) - 5.f;
        float to   = (float)(f->arrival ? f->estMin + 75 : f->estMin + 12);
        if (pair) {
            float pf = (float)(pair->arrival ? pair->estMin : pair->estMin - 80) - 5.f;
            float pt = (float)(pair->arrival ? pair->estMin + 75 : pair->estMin + 12);
            if (pf < from) from = pf;
            if (pt > to)   to   = pt;
        }

        float span = w->actype[f->acType].wingspan;
        int best = -1;
        for (int s = 0; s < w->nStands; s++) {
            Stand *st = &w->stand[s];
            if (st->kind == ST_CARGO) continue;
            if (st->maxSpan < span)   continue;
            if (busyUntil[s] > from)  continue;
            if (best < 0) { best = s; continue; }
            /* prefer a contact stand, then the tightest stand that still fits */
            if (st->kind < w->stand[best].kind) best = s;
            else if (st->kind == w->stand[best].kind &&
                     st->maxSpan < w->stand[best].maxSpan) best = s;
        }
        if (best < 0) continue;

        f->stand = best;
        f->gate  = w->stand[best].gate;
        if (pair && pair->stand < 0) {
            pair->stand = best;
            pair->gate  = w->stand[best].gate;
        }
        busyUntil[best] = to;
    }
}

static void gen_passengers(World *w)
{
    int pid = 1;
    for (int i = 0; i < w->nFlights && w->nPax < MAX_PASSENGERS; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival) continue;                  /* roster departures only */
        int want = f->pax / 14;                    /* a representative sample */
        if (want < 4)  want = 4;
        if (want > 16) want = 16;
        for (int k = 0; k < want && w->nPax < MAX_PASSENGERS; k++) {
            Passenger *p = &w->pax[w->nPax++];
            memset(p, 0, sizeof *p);
            p->id = pid++;
            snprintf(p->name, 38, "%s %s",
                     FIRST[rnd_int(&w->rng, 0, (int)(sizeof FIRST/sizeof*FIRST)-1)],
                     LAST [rnd_int(&w->rng, 0, (int)(sizeof LAST /sizeof*LAST )-1)]);
            for (int c = 0; c < 6; c++) {
                int v = rnd_int(&w->rng, 0, 35);
                p->pnr[c] = (char)(v < 26 ? 'A' + v : '0' + (v - 26));
            }
            p->pnr[6] = 0;
            p->flight = f->id;
            int row = rnd_int(&w->rng, 1, w->actype[f->acType].widebody ? 58 : 32);
            const char *cols = "ABCDEFGHJK";
            int nc = w->actype[f->acType].widebody ? 9 : 6;
            snprintf(p->seat, 5, "%d%c", row, cols[rnd_int(&w->rng, 0, nc-1)]);
            p->bags       = rnd_int(&w->rng, 0, 2);
            p->nationality= rnd_int(&w->rng, 0, w->nAirports - 1);
            p->loyalty    = (rnd_f(&w->rng) > 0.80f)
                          ? rnd_int(&w->rng, 1, 3) : 0;
            p->fastTrack  = p->loyalty >= 2;
            p->wheelchair = rnd_f(&w->rng) > 0.955f;
            p->infant     = rnd_f(&w->rng) > 0.93f;
        }
    }
}

static void gen_bags(World *w)
{
    for (int i = 0; i < w->nPax && w->nBags < MAX_BAGS; i++) {
        Passenger *p = &w->pax[i];
        Flight *f = flight_by_id(w, p->flight);
        if (!f) continue;
        for (int b = 0; b < p->bags && w->nBags < MAX_BAGS; b++) {
            Bag *g = &w->bag[w->nBags++];
            memset(g, 0, sizeof *g);
            g->id  = w->nBags;
            char code[4];
            snprintf(code, sizeof code, "%s", w->airline[f->airline].iata);
            /* bounded on both sides so the tag is provably eight characters */
            int serial = 100000 + rnd_int(&w->rng, 0, 899999);
            if (serial < 100000) serial = 100000;
            if (serial > 999999) serial = 999999;
            snprintf(g->tag, sizeof g->tag, "%.2s%06d", code, serial);
            g->pax    = p->id;
            g->flight = f->id;
            g->state  = BG_CHECKIN;
            g->weight = rnd_range(&w->rng, 6.4f, 31.8f);
            g->lane   = rnd_int(&w->rng, 0, 3);
            g->t      = rnd_f(&w->rng);
            g->colour = rnd_int(&w->rng, 0, 5);
            g->priority = p->loyalty >= 2;
            /* the screening score is produced later by the neural classifier */
            g->threatScore = 0.f;
        }
    }
}

static void gen_staff(World *w)
{
    for (int i = 0; i < MAX_STAFF; i++) {
        Staff *s = &w->staff[w->nStaff++];
        snprintf(s->name, 30, "%s %s",
                 FIRST[rnd_int(&w->rng, 0, (int)(sizeof FIRST/sizeof*FIRST)-1)],
                 LAST [rnd_int(&w->rng, 0, (int)(sizeof LAST /sizeof*LAST )-1)]);
        snprintf(s->role, 26, "%s",
                 ROLES[rnd_int(&w->rng, 0, (int)(sizeof ROLES/sizeof*ROLES)-1)]);
        s->shift  = rnd_int(&w->rng, 0, 2);
        s->onDuty = s->shift != 2;
        s->station= rnd_int(&w->rng, 1, 12);
    }
}

static void gen_desks(World *w)
{
    for (int i = 0; i < MAX_DESKS; i++) {
        Desk *d = &w->desk[w->nDesks++];
        d->number = i + 1;
        d->open   = (i < 14);
        d->flight = -1;
        d->queue  = rnd_int(&w->rng, 0, 9);
        d->served = 0.f;
    }
}

void world_generate(World *w)
{
    int idc = 1;
    int n = (int)(sizeof ROUTES / sizeof ROUTES[0]);
    for (int i = 0; i < n; i++) {
        char reg[10];
        int airline = al_index(w, ROUTES[i].al);
        /* keep drawing until this tail is not already flying today */
        for (int attempt = 0; attempt < 40; attempt++) {
            make_reg(w, airline, reg, &w->rng);
            int clash = 0;
            for (int k = 0; k < w->nFlights; k++)
                if (strcmp(w->flight[k].reg, reg) == 0) { clash = 1; break; }
            if (!clash) break;
        }
        gen_flight(w, &ROUTES[i], 1, &idc, reg);
        gen_flight(w, &ROUTES[i], 0, &idc, reg);
    }
    assign_stands(w);
    gen_passengers(w);
    gen_bags(w);
    gen_staff(w);
    gen_desks(w);

    w->totalPaxToday  = 0;
    w->totalBagsToday = w->nBags;
    for (int i = 0; i < w->nFlights; i++) w->totalPaxToday += w->flight[i].pax;

    world_log(w, LG_OK, "AURA operations core online -- %d movements loaded",
              w->nFlights);
    world_log(w, LG_INFO, "Runway 14 in use, wind 135/12kt, QNH 1017");
}
