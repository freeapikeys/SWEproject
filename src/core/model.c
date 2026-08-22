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
        { "KL","KLM","KLM",                 "Amsterdam",     0x00A1DE },
        { "BA","BAW","British Airways",     "London LGW",    0x1B4A8B },
        { "TK","THY","Turkish Airlines",    "Istanbul",      0xC70A0C },
        { "KQ","KQA","Kenya Airways",       "Nairobi",       0xB01C2E },
        { "4Z","LNK","Airlink",             "Johannesburg",  0x0F4C81 },
        { "FA","SFR","Safair",              "Johannesburg",  0x00954C },
        { "UU","REU","Air Austral",         "Saint-Denis",   0xE2001A },
        { "SS","CRL","Corsair International","Paris ORY",    0xE30613 },
        { "6E","IGO","IndiGo",              "Mumbai",        0x001B94 },
        { "AI","AIC","Air India",           "Mumbai",        0xD5122B },
        { "SV","SVA","Saudia",              "Jeddah",        0x006C35 },
        { "DE","CFG","Condor",              "Frankfurt",     0xF7C600 },
        { "4Y","OCN","Discover Airlines",   "Frankfurt",     0x0B3D7C },
        { "WK","EDW","Edelweiss Air",       "Zurich",        0xE2001A },
        { "OS","AUA","Austrian Airlines",   "Vienna",        0xCC0000 },
        { "AZ","ITY","ITA Airways",         "Rome",          0x004B87 },
        { "HM","SEY","Air Seychelles",      "Mahe",          0xE4002B },
        { "MD","MDG","Madagascar Airlines", "Antananarivo",  0x00843D },
        { "2W","WLD","World2Fly",           "Madrid",        0x00A9E0 },
        { "E9","EVE","Iberojet",            "Madrid",        0xE85A0C },
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
        { "RUN","Roland Garros", "Reunion",       -20.887f,  55.513f,  50 },
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
        { "A5", ST_CONTACT, 588, 470, 90, 65.f, 1,  5 },
        { "A6", ST_CONTACT, 660, 470, 90, 40.f, 1,  6 },
        { "A7", ST_CONTACT, 726, 470, 90, 40.f, 1,  7 },
        { "A8", ST_CONTACT, 786, 470, 90, 36.f, 1,  8 },
        { "R1", ST_REMOTE,  196, 496, 60, 65.f, 0, 21 },
        { "R2", ST_REMOTE,  196, 556, 60, 65.f, 0, 22 },
        { "R3", ST_REMOTE,  128, 496, 60, 65.f, 0, 23 },
        { "R4", ST_REMOTE,  128, 556, 60, 65.f, 0, 24 },
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
        /* Southern overflow row on the eastern apron.  Plaisance parks the
         * evening long-haul bank out here when the pier is full; without it
         * the last few movements of the night have nowhere to go. */
        { "R15",ST_REMOTE,  862, 616,120, 65.f, 0, 35 },
        { "R16",ST_REMOTE,  930, 616,120, 65.f, 0, 36 },
        { "R17",ST_REMOTE,  982, 616,120, 65.f, 0, 37 },
        { "R18",ST_REMOTE,  794, 616,120, 52.f, 0, 38 },
        { "C1", ST_CARGO,    62, 420,  0, 65.f, 0,  0 },
        { "C2", ST_CARGO,    62, 356,  0, 65.f, 0,  0 },
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

const char *as_name(SpecialAssist a)
{
    switch (a) {
    case AS_WHEELCHAIR: return "Wheelchair";
    case AS_MOBILITY:   return "Reduced mobility";
    case AS_VISUAL:     return "Visually impaired";
    case AS_HEARING:    return "Hearing impaired";
    case AS_MINOR:      return "Unaccompanied minor";
    case AS_MEDICAL:    return "Medical clearance";
    default:            return "None";
    }
}

const char *as_code(SpecialAssist a)
{
    switch (a) {
    case AS_WHEELCHAIR: return "WCHR";
    case AS_MOBILITY:   return "WCHS";
    case AS_VISUAL:     return "BLND";
    case AS_HEARING:    return "DEAF";
    case AS_MINOR:      return "UMNR";
    case AS_MEDICAL:    return "MEDA";
    default:            return "----";
    }
}

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
 *  the clock
 *
 *  Plaisance keeps Mauritius time, UTC+4, and Mauritius does not observe
 *  daylight saving, so airport local time is the UTC instant plus exactly
 *  four hours all year round.  Taking it that way rather than from the
 *  machine's own time zone means the board reads correctly whatever zone the
 *  laptop running it happens to be set to.
 *
 *  There is no simulated time anywhere in this application.
 * ========================================================================== */

/*  Civil-date conversion, exact across month ends, leap years and century
 *  boundaries.  The schedule is keyed on a day number, so this has to be
 *  right rather than approximately right.                                  */
long days_from_civil(int y, int m, int d)
{
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);                  /* 0..399    */
    unsigned doy = (unsigned)((153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1);
    unsigned doe = yoe*365 + yoe/4 - yoe/100 + doy;            /* 0..146096 */
    return era * 146097L + (long)doe - 719468L;
}

void civil_from_days(long z, int *y, int *m, int *d)
{
    z += 719468L;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long yr  = (long)yoe + era * 400;
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp  = (5*doy + 2)/153;
    unsigned dd  = doy - (153*mp + 2)/5 + 1;
    unsigned mm  = (mp < 10) ? mp + 3 : mp - 9;
    *y = (int)(yr + (mm <= 2 ? 1 : 0));
    *m = (int)mm;
    *d = (int)dd;
}

/* 1 January 1970 was a Thursday, so day zero is weekday 4 with Sunday = 0 */
static int weekday_of(long epochDay)
{
    return (int)(((epochDay % 7) + 7 + 4) % 7);
}

const char *month_name(int m)
{
    static const char *N[13] = { "", "January", "February", "March", "April",
        "May", "June", "July", "August", "September", "October", "November",
        "December" };
    return (m >= 1 && m <= 12) ? N[m] : "";
}

const char *weekday_name(int wd)
{
    static const char *N[7] = { "Sunday", "Monday", "Tuesday", "Wednesday",
                                "Thursday", "Friday", "Saturday" };
    return (wd >= 0 && wd < 7) ? N[wd] : "";
}

void world_sync_clock(World *w)
{
    /*  Shifting in the 100-nanosecond FILETIME domain rather than poking at
     *  the fields of a SYSTEMTIME means midnight, month ends, leap days and
     *  the new year all look after themselves.                             */
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart  = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    u.QuadPart += 4ULL * 3600ULL * 10000000ULL;              /* UTC+4       */
    ft.dwLowDateTime  = u.LowPart;
    ft.dwHighDateTime = u.HighPart;

    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&ft, &st)) return;

    w->clock = (float)st.wHour * 60.f
             + (float)st.wMinute
             + (float)st.wSecond / 60.f
             + (float)st.wMilliseconds / 60000.f;
    w->day   = (int)st.wDay;
    w->month = (int)st.wMonth;
    w->year  = (int)st.wYear;

    long ed = days_from_civil(w->year, w->month, w->day);
    w->weekday = weekday_of(ed);
    if (w->epochDay != 0 && ed != w->epochDay) w->dayRolled = 1;
    w->epochDay = ed;
}

/* ==========================================================================
 *  world generation
 * ========================================================================== */

void world_init(World *w)
{
    memset(w, 0, sizeof *w);
    w->rng   = 0xA17C0DEu;
    w->tempC = 24.6f; w->windKt = 12.f; w->windDir = 135.f;
    w->visKm = 10.f;  w->qnh = 1017.f;  w->humidity = 74.f;
    w->wxKind = 1;
    w->activeRunway = 14;
    world_seed_data(w);

    world_sync_clock(w);           /* the real date, before anything is made */
    w->baseDay   = w->epochDay;    /* the programme is published from today  */
    w->dayRolled = 0;
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
    } else if (strcmp(ia, "AI") == 0) {
        sprintf(out, "VT-%c%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "KL") == 0) {
        sprintf(out, "PH-B%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "FA") == 0) {
        sprintf(out, "ZS-%c%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "4Y") == 0) {
        sprintf(out, "D-A%c%c%c", 'I'+rnd_int(rng,0,8), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "WK") == 0) {
        sprintf(out, "HB-J%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "OS") == 0) {
        sprintf(out, "OE-L%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "AZ") == 0) {
        sprintf(out, "EI-%c%c%c", 'A'+rnd_int(rng,0,25), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
    } else if (strcmp(ia, "2W") == 0 || strcmp(ia, "E9") == 0) {
        sprintf(out, "EC-%c%c%c", 'M'+rnd_int(rng,0,12), 'A'+rnd_int(rng,0,25),
                'A'+rnd_int(rng,0,25));
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
    /* Reconstructed from the published Plaisance boards: the overnight
     * European bank lands between 04:30 and 07:00, the Indian Ocean and
     * regional rotations work through the day, and the long-haul departures
     * stack up from 20:00 to just before midnight.  Flight numbers, block
     * times and turnarounds follow the real timetable.                      */
    /* airline, arr flight, other end, type, arrive, depart, dep flight      */
    { "KQ","KQ274", "NBO","738",  275,   380, "KQ275"  },
    { "MK","MK015", "CDG","359",  340,  1310, "MK014"  },
    { "MK","MK053", "LGW","339",  410,  1290, "MK052"  },
    { "UU","UU102", "RUN","320",  450,   530, "UU103"  },
    { "MK","MK249", "RUN","AT7",  470,   555, "MK250"  },
    { "6E","6E1861","BLR","32N",  510,   600, "6E1862" },
    { "AI","AI2241","BOM","32N",  515,   610, "AI2242" },
    { "EK","EK701", "DXB","77W",  565,   700, "EK702"  },
    { "MK","MK047", "JNB","332",  520,   620, "MK048"  },
    { "MK","MK201", "RRG","AT7",  545,   630, "MK202"  },
    { "4Z","4Z252", "JNB","E90",  580,   665, "4Z253"  },
    { "TK","TK741", "IST","789",  610,  1315, "TK742"  },
    { "MK","MK203", "RRG","AT7",  640,   725, "MK204"  },
    { "SS","SS910", "ORY","332",  655,  1330, "SS911"  },
    { "MK","MK041", "BOM","339",  670,  1250, "MK042"  },
    { "HM","HM062", "SEZ","320",  700,   790, "HM063"  },
    { "MK","MK205", "RRG","AT7",  725,   810, "MK206"  },
    { "MD","MD731", "TNR","AT7",  745,   835, "MD732"  },
    { "UU","UU974", "RUN","320",  770,   855, "UU975"  },
    { "WK","WK340", "ZRH","333",  795,  1275, "WK341"  },
    { "MK","MK051", "TNR","AT7",  815,   900, "MK054"  },
    { "MK","MK045", "CPT","332",  840,   935, "MK046"  },
    { "4Y","4Y100", "FRA","333",  865,  1295, "4Y101"  },
    { "MK","MK207", "RRG","AT7",  890,   975, "MK208"  },
    { "FA","FA351", "JNB","738",  915,  1000, "FA352"  },
    { "MK","MK043", "DEL","339",  940,  1265, "MK044"  },
    { "UU","UU976", "RUN","320",  965,  1050, "UU977"  },
    { "DE","DE2314","MUC","333",  990,  1305, "DE2315" },
    { "MK","MK209", "RRG","AT7", 1015,  1100, "MK210"  },
    { "SV","SV831", "JED","789", 1040,  1320, "SV832"  },
    { "2W","2W121", "MAD","332", 1065,  1285, "2W122"  },
    { "MK","MK211", "RRG","AT7", 1090,  1175, "MK212"  },
    { "BA","BA2065","LGW","789", 1115,  1345, "BA2066" },
    { "OS","OS035", "VIE","789", 1140,  1300, "OS036"  },
    { "MK","MK017", "CDG","359", 1165,  1335, "MK016"  },
    { "AZ","AZ852", "FCO","333", 1190,  1325, "AZ853"  },
    { "MK","MK031", "KUL","339", 1215,  1385, "MK032"  },
    { "E9","E9312", "MAD","332", 1240,  1355, "E9313"  },
    { "MK","MK055", "PER","332", 1265,  1420, "MK056"  },
    { "MK","MK213", "RRG","AT7", 1290,  1375, "MK214"  },
};


/* --------------------------------------------------------------------------
 *  Slot coordination.
 *
 *  Plaisance has one runway, so two movements cannot be published four
 *  minutes apart and both happen.  A real timetable is slot-coordinated
 *  before it is published: where two movements are too close, the later one
 *  is pushed back until it fits.
 *
 *  Doing this to the *scheduled* times matters.  It means the published day
 *  is feasible, and it means that when a runway conflict does appear it is
 *  news -- caused by a delay somebody can act on, rather than by a timetable
 *  that never worked in the first place.
 * ------------------------------------------------------------------------- */

#define RWY_MIN_GAP 5            /* minutes between published movements     */

static void deconflict_runway(int *minutes, int n)
{
    if (n < 2) return;

    int order[MAX_FLIGHTS];
    if (n > MAX_FLIGHTS) n = MAX_FLIGHTS;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && minutes[order[j]] > minutes[k]) {
            order[j+1] = order[j]; j--;
        }
        order[j+1] = k;
    }

    for (int i = 1; i < n; i++) {
        int prev = minutes[order[i-1]];
        int cur  = minutes[order[i]];
        if (cur - prev < RWY_MIN_GAP) minutes[order[i]] = prev + RWY_MIN_GAP;
    }
}

/* --------------------------------------------------------------------------
 *  Which days of the week a rotation operates.
 *
 *  A published timetable is weekly, not daily.  The Rodrigues and Reunion
 *  shuttles run every day; the Air Mauritius long-haul rotations four to six
 *  times a week; the foreign carriers two to six.  Deriving the pattern from
 *  a hash of the flight number keeps it stable for the whole season -- the
 *  same route always operates on the same weekdays -- without a frequency
 *  column having to be maintained by hand for every row in the table.
 * ------------------------------------------------------------------------- */

static unsigned route_days(const RouteRow *r)
{
    uint32_t h = 2166136261u;
    for (const char *p = r->num; *p; p++) {
        h ^= (uint32_t)(unsigned char)*p;
        h *= 16777619u;
    }

    int freq;
    if (strcmp(r->ap, "RRG") == 0 || strcmp(r->ap, "RUN") == 0) freq = 7;
    else if (strcmp(r->ac, "AT7") == 0)                         freq = 7;
    else if (strcmp(r->al, "MK")  == 0)                         freq = 4 + (int)(h % 3u);
    else                                                        freq = 2 + (int)(h % 5u);
    if (freq > 7) freq = 7;

    unsigned mask = 0;
    uint32_t x = h | 1u;
    for (int placed = 0; placed < freq; ) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        unsigned d = x % 7u;
        if (!(mask & (1u << d))) { mask |= 1u << d; placed++; }
    }
    return mask;
}

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
    /* Southern summer and the European holidays fill the aeroplanes; May and
     * June are the shoulder.  The same route in a different month carries a
     * visibly different load, which is half the point of holding a schedule
     * three months long rather than a single day. */
    float season = (w->month == 12 || w->month == 1 ||
                    w->month == 7  || w->month == 8) ?  0.09f
                 : (w->month == 5  || w->month == 6) ? -0.07f : 0.f;
    float load = rnd_range(&w->rng, 0.62f, 0.97f) + season;
    if (load < 0.45f) load = 0.45f;
    if (load > 0.99f) load = 0.99f;
    f->pax    = (int)(t->seats * load);
    f->bags   = (int)(f->pax * rnd_range(&w->rng, 0.55f, 1.35f));

    /* a realistic scatter of delays */
    float roll = rnd_f(&w->rng);
    if (roll > 0.86f)      f->delayMin = rnd_int(&w->rng, 20, 95);
    else if (roll > 0.68f) f->delayMin = rnd_int(&w->rng, 5, 19);
    else                   f->delayMin = 0;
    f->estMin = f->schedMin + f->delayMin;

    if (arrival) {
        /* Plaisance quotes reclaim belts in pairs for the wide-bodies, which
         * is what the arrivals board actually displays. */
        const char *wide[]  = { "02-03", "04-05", "06-07" };
        const char *narrow[] = { "01", "03", "06", "08" };
        if (w->actype[f->acType].widebody)
            snprintf(f->belt, sizeof f->belt, "%s", wide[rnd_int(&w->rng, 0, 2)]);
        else
            snprintf(f->belt, sizeof f->belt, "%s", narrow[rnd_int(&w->rng, 0, 3)]);

        /* Codeshares: one aeroplane sold under several numbers.  The live
         * boards list every marketing carrier against the same movement. */
        struct { const char *op, *dest, *share; } CS[] = {
            { "MK","CDG","AF7964" }, { "MK","CDG","KL3848" },
            { "MK","RUN","AF7945" }, { "MK","RUN","AI6727" },
            { "KQ","NBO","MK963"  }, { "MK","JNB","4Z8043" },
            { "MK","BOM","AI7132" }, { "AF","CDG","MK9015" },
            { "MK","LGW","BA6742" }, { "UU","RUN","MK9102" },
        };
        for (int k = 0; k < (int)(sizeof CS/sizeof CS[0]) && f->nShares < 3; k++) {
            if (strcmp(w->airline[f->airline].iata, CS[k].op) != 0) continue;
            if (strcmp(w->airport[f->airport].iata, CS[k].dest) != 0) continue;
            snprintf(f->shares[f->nShares], 8, "%s", CS[k].share);
            f->nShares++;
        }
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
        /* Two passes: passenger stands first, and only if the whole apron is
         * committed does a cargo hardstand get used.  Testing cargo inside a
         * single pass makes its use depend on array order rather than on
         * there genuinely being nothing else free. */
        for (int pass = 0; pass < 2 && best < 0; pass++)
        for (int s = 0; s < w->nStands; s++) {
            Stand *st = &w->stand[s];
            if (pass == 0 && st->kind == ST_CARGO) continue;
            if (pass == 1 && st->kind != ST_CARGO) continue;
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
            /*  Draw a seat, but check nobody on this flight already has it.
             *  A random draw alone collides far more often than it feels
             *  like it should -- with fourteen passengers over a hundred
             *  and ninety seats it is about even money per flight -- and
             *  two people holding 12B is a real error, not a cosmetic one. */
            const char *cols = "ABCDEFGHJK";
            int rows = w->actype[f->acType].widebody ? 58 : 32;
            int nc   = w->actype[f->acType].widebody ?  9 :  6;
            int placed = 0;
            for (int attempt = 0; attempt < 60 && !placed; attempt++) {
                char cand[5];
                snprintf(cand, sizeof cand, "%d%c",
                         rnd_int(&w->rng, 1, rows),
                         cols[rnd_int(&w->rng, 0, nc-1)]);
                int taken = 0;
                for (int q = 0; q < w->nPax - 1; q++)
                    if (w->pax[q].flight == f->id &&
                        strcmp(w->pax[q].seat, cand) == 0) { taken = 1; break; }
                if (!taken) {
                    snprintf(p->seat, sizeof p->seat, "%s", cand);
                    placed = 1;
                }
            }
            if (!placed) {
                /* the random draw kept losing: walk the cabin instead */
                for (int r = 1; r <= rows && !placed; r++)
                    for (int ci = 0; ci < nc && !placed; ci++) {
                        char cand[5];
                        snprintf(cand, sizeof cand, "%d%c", r, cols[ci]);
                        int taken = 0;
                        for (int q = 0; q < w->nPax - 1; q++)
                            if (w->pax[q].flight == f->id &&
                                strcmp(w->pax[q].seat, cand) == 0) {
                                taken = 1; break;
                            }
                        if (!taken) {
                            snprintf(p->seat, sizeof p->seat, "%s", cand);
                            placed = 1;
                        }
                    }
            }
            p->bags       = rnd_int(&w->rng, 0, 2);
            p->nationality= rnd_int(&w->rng, 0, w->nAirports - 1);
            p->loyalty    = (rnd_f(&w->rng) > 0.80f)
                          ? rnd_int(&w->rng, 1, 3) : 0;
            p->fastTrack  = p->loyalty >= 2;
            p->wheelchair = rnd_f(&w->rng) > 0.955f;
            p->infant     = rnd_f(&w->rng) > 0.93f;

            /*  Special assistance.  Roughly one passenger in twenty asks for
             *  something, which is what a station this size actually files;
             *  the wheelchair flag and the assistance code are kept
             *  consistent so the two never disagree on screen.             */
            float sr = rnd_f(&w->rng);
            if (p->wheelchair)   p->assist = AS_WHEELCHAIR;
            else if (sr > 0.975f) p->assist = AS_MOBILITY;
            else if (sr > 0.968f) p->assist = AS_VISUAL;
            else if (sr > 0.961f) p->assist = AS_HEARING;
            else if (sr > 0.955f) p->assist = AS_MINOR;
            else if (sr > 0.950f) p->assist = AS_MEDICAL;
            else                  p->assist = AS_NONE;
            if (p->assist == AS_WHEELCHAIR) p->wheelchair = 1;
        }
    }
}

/*  Connecting passengers.
 *
 *  Plaisance is a hub for the Indian Ocean: a passenger off the Paris or
 *  Dubai widebody in the morning may be leaving again on the Rodrigues or
 *  Reunion turboprop the same afternoon.  Marking those passengers matters
 *  because their bag has to be found and re-tagged, and because if the
 *  inbound is late they are the ones who miss the onward flight.
 * ------------------------------------------------------------------------- */
static void gen_connections(World *w)
{
    for (int i = 0; i < w->nPax; i++) {
        Passenger *p = &w->pax[i];
        Flight *out = flight_by_id(w, p->flight);
        if (!out || out->arrival) continue;
        if (rnd_f(&w->rng) > 0.22f) continue;      /* about one in five      */

        /*  Find an arrival that lands early enough to make the connection
         *  and not so early that it would be a day trip.  The minimum
         *  connecting time at Plaisance is 60 minutes for international.  */
        int best = -1, bestGap = 0;
        for (int k = 0; k < w->nFlights; k++) {
            Flight *in = &w->flight[k];
            if (!in->arrival || in->state == FS_CANCELLED) continue;
            if (in->airport == out->airport) continue;   /* not straight back */
            int gap = out->estMin - in->estMin;
            if (gap < 60 || gap > 420) continue;
            if (best < 0 || gap < bestGap) { best = k; bestGap = gap; }
        }
        if (best < 0) continue;
        p->connFlight = w->flight[best].id;
        p->connMin    = bestGap;
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

/* --------------------------------------------------------------------------
 *  Optional local roster.
 *
 *  data/roster.csv, one name per line, overrides the generated names.  It is
 *  deliberately git-ignored: a demonstrator can put a real list of people in
 *  it for a presentation without those names ever entering the repository.
 *  If the file is absent the generated Mauritian roster is used unchanged.
 * ------------------------------------------------------------------------- */

int world_load_roster(World *w, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    char line[128];
    int applied = 0;
    while (applied < w->nPax && fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r' ||
                     line[n-1] == ' '  || line[n-1] == '\t')) line[--n] = 0;
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') continue;              /* blank or comment    */
        /* accept "Surname, First" as well as "First Surname" */
        char *comma = strchr(p, ',');
        if (comma) {
            /* sized so the pair always fits the 38-byte name field */
            char sur[18], first[18];
            size_t sl = (size_t)(comma - p);
            if (sl > sizeof sur - 1) sl = sizeof sur - 1;
            memcpy(sur, p, sl); sur[sl] = 0;
            char *q = comma + 1;
            while (*q == ' ') q++;
            snprintf(first, sizeof first, "%s", q);
            snprintf(w->pax[applied].name, sizeof w->pax[applied].name,
                     "%s %s", first, sur);
        } else {
            snprintf(w->pax[applied].name, sizeof w->pax[applied].name, "%s", p);
        }
        applied++;
    }
    fclose(f);
    if (applied)
        world_log(w, LG_INFO, "Passenger roster: %d names loaded from %s",
                  applied, path);
    return applied;
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

/* ==========================================================================
 *  a day of operations
 *
 *  The programme runs for ninety-two days and none of it is stored: a date
 *  seeds the generator, the generator produces that date's flights, and the
 *  same date always produces the same day again.  That is what makes it a
 *  schedule rather than a random shuffle -- a passenger can be told their
 *  flight leaves at 20:35 three weeks from now and it still will.
 * ========================================================================== */

void world_generate_day(World *w, long epochDay)
{
    int y, m, d;
    civil_from_days(epochDay, &y, &m, &d);
    int wd = weekday_of(epochDay);

    /* the date is the seed, so the day is reproducible from nothing else */
    w->epochDay = epochDay;
    w->day = d; w->month = m; w->year = y; w->weekday = wd;
    w->rng = ((uint32_t)epochDay * 2654435761u) ^ 0xA17C0DEu;
    for (int i = 0; i < 8; i++) (void)rnd_f(&w->rng);   /* let it settle */

    w->nFlights = 0;
    w->nPax     = 0;
    w->nBags    = 0;
    w->nStaff   = 0;
    w->nDesks   = 0;
    w->totalDepartures = w->totalArrivals = 0;
    w->bagsMishandled  = w->securityAlerts = 0;

    int idc = 1;
    int n = (int)(sizeof ROUTES / sizeof ROUTES[0]);
    for (int i = 0; i < n; i++) {
        if (!(route_days(&ROUTES[i]) & (1u << wd))) continue;   /* not today */

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

    /*  Space the published times before anything is allocated: the stand
     *  plan, the passenger timings and the baggage release all key off
     *  estMin, so the slots have to be settled first.                      */
    int slot[MAX_FLIGHTS];
    for (int i = 0; i < w->nFlights; i++) slot[i] = w->flight[i].schedMin;
    deconflict_runway(slot, w->nFlights);
    for (int i = 0; i < w->nFlights; i++) {
        w->flight[i].schedMin = slot[i];
        w->flight[i].estMin   = slot[i] + w->flight[i].delayMin;
        while (w->flight[i].estMin >= 1440) w->flight[i].estMin -= 1440;
    }

    assign_stands(w);
    gen_passengers(w);
    gen_connections(w);
    gen_bags(w);
    gen_staff(w);
    gen_desks(w);

    w->totalPaxToday  = 0;
    w->totalBagsToday = w->nBags;
    for (int i = 0; i < w->nFlights; i++) w->totalPaxToday += w->flight[i].pax;

    world_load_roster(w, "data/roster.csv");
}

void world_generate(World *w)
{
    world_generate_day(w, w->epochDay);

    world_log(w, LG_OK,
              "AURA operations core online -- %d movements for %s %d %s %d",
              w->nFlights, weekday_name(w->weekday), w->day,
              month_name(w->month), w->year);
    world_log(w, LG_INFO, "Runway %02d in use, wind 135/12kt, QNH 1017",
              w->activeRunway);
}

/* --------------------------------------------------------------------------
 *  Looking ahead.
 *
 *  These read the same weekly patterns as the generator but touch nothing,
 *  so any of the ninety-two days can be shown without disturbing the day the
 *  airport is actually running.
 * ------------------------------------------------------------------------- */

int schedule_movements(const World *w, long epochDay)
{
    int wd = weekday_of(epochDay), n = 0;
    int nr = (int)(sizeof ROUTES / sizeof ROUTES[0]);
    (void)w;
    for (int i = 0; i < nr; i++)
        if (route_days(&ROUTES[i]) & (1u << wd)) n += 2;
    return n;
}

int schedule_for_day(const World *w, long epochDay, SchedRow *out, int max)
{
    int wd = weekday_of(epochDay), n = 0;
    int nr = (int)(sizeof ROUTES / sizeof ROUTES[0]);

    for (int i = 0; i < nr && n < max; i++) {
        if (!(route_days(&ROUTES[i]) & (1u << wd))) continue;

        int ai = -1, al = 0;
        for (int k = 0; k < w->nAirports; k++)
            if (strcmp(w->airport[k].iata, ROUTES[i].ap) == 0) { ai = k; break; }
        for (int k = 0; k < w->nAirlines; k++)
            if (strcmp(w->airline[k].iata, ROUTES[i].al) == 0) { al = k; break; }

        for (int leg = 0; leg < 2 && n < max; leg++) {
            SchedRow *r = &out[n++];
            /* cleared, not just filled: the unused tail of every character
             * array would otherwise be whatever was on the stack, and two
             * identical days would not compare equal */
            memset(r, 0, sizeof *r);
            snprintf(r->no, sizeof r->no, "%s",
                     leg ? ROUTES[i].depNum : ROUTES[i].num);
            r->arrival  = (leg == 0);
            r->schedMin = leg ? ROUTES[i].depMin : ROUTES[i].arrMin;
            r->airline  = al;
            snprintf(r->ac, sizeof r->ac, "%s", ROUTES[i].ac);
            snprintf(r->destIata, sizeof r->destIata, "%s", ROUTES[i].ap);
            snprintf(r->dest, sizeof r->dest, "%s",
                     ai >= 0 ? w->airport[ai].city : ROUTES[i].ap);
        }
    }

    /* in time order, the way a published timetable reads */
    for (int i = 1; i < n; i++) {
        SchedRow t = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].schedMin > t.schedMin) { out[j+1] = out[j]; j--; }
        out[j+1] = t;
    }

    /*  Slot-coordinated with exactly the same rule the live day uses, so the
     *  forward timetable and the day it turns into agree to the minute.    */
    int slot[MAX_FLIGHTS];
    int m = n > MAX_FLIGHTS ? MAX_FLIGHTS : n;
    for (int i = 0; i < m; i++) slot[i] = out[i].schedMin;
    deconflict_runway(slot, m);
    for (int i = 0; i < m; i++) out[i].schedMin = slot[i];
    return n;
}
