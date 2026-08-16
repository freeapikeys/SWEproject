/* ==========================================================================
 *  AURA :: ai_chat.c   --   the operations assistant
 *
 *  A working natural language front end to the airport, written in C.
 *
 *  Pipeline:
 *    normalise -> tokenise -> intent classification -> entity extraction
 *              -> dialogue context resolution -> response synthesis
 *
 *  Classification is a bag-of-words scorer over a hand-built intent corpus.
 *  Each keyword carries a weight, and every keyword is further scaled by its
 *  inverse document frequency across the corpus, computed once at start-up:
 *  a word like "flight" appears under nearly every intent and so tells you
 *  almost nothing, while "carousel" or "crosswind" is close to decisive.
 *  The score is length-normalised so a long question does not automatically
 *  beat a short one.
 *
 *  Entity extraction runs independently of the intent, which is what lets
 *  "MK046" on its own do the sensible thing, and lets "what about its gate?"
 *  work -- the flight is recovered from the dialogue context stack.
 * ========================================================================== */

#include "ai.h"
#include "../core/sim.h"
#include "../engine/anim.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ==========================================================================
 *  intents
 * ========================================================================== */

enum {
    IN_GREET = 0, IN_HELP, IN_STATUS, IN_GATE, IN_TIME, IN_DEPARTURES,
    IN_ARRIVALS, IN_BAGTRACK, IN_BAGSTATS, IN_PAX, IN_CHECKIN, IN_SECURITY,
    IN_WEATHER, IN_RUNWAY, IN_STAND, IN_PREDICT, IN_AIRPORT, IN_AIRLINE,
    IN_DEST, IN_STAFF, IN_SUMMARY, IN_THANKS, IN_BUSIEST, IN_SAVE,
    IN_ABOUTAI, IN_DELAYED, IN_UNKNOWN, IN_COUNT
};

static const char *INTENT_NAME[IN_COUNT] = {
    "greeting","help","flight.status","flight.gate","flight.time",
    "departures.next","arrivals.next","baggage.track","baggage.stats",
    "passenger.lookup","checkin.info","security.wait","weather.now",
    "runway.info","stand.info","delay.predict","airport.info","airline.info",
    "destination.info","staff.roster","operations.summary","thanks",
    "traffic.busiest","data.save","ai.about","flights.delayed","unknown"
};

const char *ai_intent_name(int i)
{
    if (i < 0 || i >= IN_COUNT) return "unknown";
    return INTENT_NAME[i];
}

typedef struct { int intent; const char *kw; float w; } KW;

static const KW CORPUS[] = {
 { IN_GREET,"hello",3},{ IN_GREET,"hi",3},{ IN_GREET,"hey",3},
 { IN_GREET,"bonjour",3},{ IN_GREET,"morning",2},{ IN_GREET,"evening",2},
 { IN_GREET,"salut",3},{ IN_GREET,"greetings",3},{ IN_GREET,"there",1},

 { IN_HELP,"help",3},{ IN_HELP,"what",1},{ IN_HELP,"can",1.5f},
 { IN_HELP,"do",1},{ IN_HELP,"commands",3},{ IN_HELP,"options",2.5f},
 { IN_HELP,"able",2},{ IN_HELP,"capabilities",3},{ IN_HELP,"ask",2},

 { IN_STATUS,"status",3},{ IN_STATUS,"flight",2},{ IN_STATUS,"where",2},
 { IN_STATUS,"how",1},{ IN_STATUS,"doing",1.5f},{ IN_STATUS,"about",1},
 { IN_STATUS,"info",1.5f},{ IN_STATUS,"tell",1.5f},{ IN_STATUS,"details",2},
 { IN_STATUS,"is",0.6f},{ IN_STATUS,"on",0.6f},

 { IN_GATE,"gate",3.5f},{ IN_GATE,"stand",2},{ IN_GATE,"boarding",2},
 { IN_GATE,"which",1.5f},{ IN_GATE,"where",1.5f},{ IN_GATE,"door",2},
 { IN_GATE,"pier",2},

 { IN_TIME,"time",3},{ IN_TIME,"when",3},{ IN_TIME,"depart",2.5f},
 { IN_TIME,"departs",2.5f},{ IN_TIME,"arrive",2.5f},{ IN_TIME,"arrives",2.5f},
 { IN_TIME,"eta",3},{ IN_TIME,"etd",3},{ IN_TIME,"schedule",2},
 { IN_TIME,"leaving",2.5f},{ IN_TIME,"landing",2},{ IN_TIME,"late",1.5f},

 { IN_DEPARTURES,"departures",3.5f},{ IN_DEPARTURES,"departing",3},
 { IN_DEPARTURES,"outbound",3},{ IN_DEPARTURES,"next",2},
 { IN_DEPARTURES,"leaving",2},{ IN_DEPARTURES,"upcoming",2},
 { IN_DEPARTURES,"board",1.5f},

 { IN_ARRIVALS,"arrivals",3.5f},{ IN_ARRIVALS,"arriving",3},
 { IN_ARRIVALS,"inbound",3},{ IN_ARRIVALS,"landing",2},
 { IN_ARRIVALS,"incoming",2.5f},{ IN_ARRIVALS,"approach",2},

 { IN_BAGTRACK,"bag",3},{ IN_BAGTRACK,"baggage",2.5f},{ IN_BAGTRACK,"luggage",3},
 { IN_BAGTRACK,"suitcase",3},{ IN_BAGTRACK,"track",2.5f},{ IN_BAGTRACK,"trace",2.5f},
 { IN_BAGTRACK,"find",2},{ IN_BAGTRACK,"tag",2.5f},{ IN_BAGTRACK,"lost",2.5f},
 { IN_BAGTRACK,"missing",2.5f},{ IN_BAGTRACK,"locate",2.5f},

 { IN_BAGSTATS,"bags",2.5f},{ IN_BAGSTATS,"baggage",2},{ IN_BAGSTATS,"system",2},
 { IN_BAGSTATS,"screening",3},{ IN_BAGSTATS,"screened",3},
 { IN_BAGSTATS,"carousel",3},{ IN_BAGSTATS,"belt",2.5f},{ IN_BAGSTATS,"loaded",2},
 { IN_BAGSTATS,"how",1},{ IN_BAGSTATS,"many",2},{ IN_BAGSTATS,"flagged",3},
 { IN_BAGSTATS,"held",2.5f},{ IN_BAGSTATS,"mishandled",3},

 { IN_PAX,"passenger",3},{ IN_PAX,"pnr",3.5f},{ IN_PAX,"booking",3},
 { IN_PAX,"reference",2},{ IN_PAX,"seat",2.5f},{ IN_PAX,"traveller",3},
 { IN_PAX,"who",2},{ IN_PAX,"name",2},

 { IN_CHECKIN,"check",2.5f},{ IN_CHECKIN,"checkin",3.5f},{ IN_CHECKIN,"desk",3},
 { IN_CHECKIN,"desks",3},{ IN_CHECKIN,"counter",3},{ IN_CHECKIN,"open",2},
 { IN_CHECKIN,"queue",2},{ IN_CHECKIN,"hall",2.5f},

 { IN_SECURITY,"security",3.5f},{ IN_SECURITY,"wait",3},{ IN_SECURITY,"waiting",3},
 { IN_SECURITY,"queue",2.5f},{ IN_SECURITY,"screening",2},
 { IN_SECURITY,"long",2},{ IN_SECURITY,"lanes",3},{ IN_SECURITY,"search",2},
 { IN_SECURITY,"busy",1.5f},

 { IN_WEATHER,"weather",3.5f},{ IN_WEATHER,"wind",3},{ IN_WEATHER,"rain",3},
 { IN_WEATHER,"temperature",3},{ IN_WEATHER,"conditions",2.5f},
 { IN_WEATHER,"forecast",2},{ IN_WEATHER,"visibility",3},{ IN_WEATHER,"qnh",3},
 { IN_WEATHER,"metar",3},{ IN_WEATHER,"hot",2},{ IN_WEATHER,"cyclone",3},

 { IN_RUNWAY,"runway",3.5f},{ IN_RUNWAY,"rwy",3.5f},{ IN_RUNWAY,"active",2},
 { IN_RUNWAY,"use",1.5f},{ IN_RUNWAY,"crosswind",3},{ IN_RUNWAY,"14",2},
 { IN_RUNWAY,"32",2},{ IN_RUNWAY,"taxiway",2.5f},

 { IN_STAND,"stand",3},{ IN_STAND,"stands",3},{ IN_STAND,"apron",3},
 { IN_STAND,"parking",2.5f},{ IN_STAND,"remote",2.5f},{ IN_STAND,"contact",2},
 { IN_STAND,"allocation",3},{ IN_STAND,"jetbridge",3},{ IN_STAND,"bay",2.5f},

 { IN_PREDICT,"predict",3.5f},{ IN_PREDICT,"prediction",3.5f},
 { IN_PREDICT,"risk",3},{ IN_PREDICT,"likely",2.5f},{ IN_PREDICT,"chance",3},
 { IN_PREDICT,"probability",3.5f},{ IN_PREDICT,"forecast",2},
 { IN_PREDICT,"will",1.5f},{ IN_PREDICT,"expect",2},

 { IN_DELAYED,"delayed",3.5f},{ IN_DELAYED,"delays",3.5f},{ IN_DELAYED,"delay",3},
 { IN_DELAYED,"behind",2.5f},{ IN_DELAYED,"running",1.5f},
 { IN_DELAYED,"which",1.5f},{ IN_DELAYED,"any",1.5f},

 { IN_AIRPORT,"airport",3},{ IN_AIRPORT,"mru",3},{ IN_AIRPORT,"plaisance",3.5f},
 { IN_AIRPORT,"fimp",3.5f},{ IN_AIRPORT,"mauritius",2.5f},
 { IN_AIRPORT,"terminal",2.5f},{ IN_AIRPORT,"ramgoolam",3},

 { IN_AIRLINE,"airline",3},{ IN_AIRLINE,"carrier",3},{ IN_AIRLINE,"operator",3},
 { IN_AIRLINE,"airlines",3},{ IN_AIRLINE,"fleet",2.5f},

 { IN_DEST,"destination",3},{ IN_DEST,"route",3},{ IN_DEST,"routes",3},
 { IN_DEST,"fly",2},{ IN_DEST,"flies",2.5f},{ IN_DEST,"city",2},
 { IN_DEST,"country",2},{ IN_DEST,"far",2},{ IN_DEST,"distance",2.5f},

 { IN_STAFF,"staff",3.5f},{ IN_STAFF,"roster",3.5f},{ IN_STAFF,"shift",3},
 { IN_STAFF,"team",2.5f},{ IN_STAFF,"crew",2.5f},{ IN_STAFF,"duty",2.5f},
 { IN_STAFF,"working",2.5f},

 { IN_SUMMARY,"summary",3.5f},{ IN_SUMMARY,"overview",3.5f},
 { IN_SUMMARY,"today",2},{ IN_SUMMARY,"operations",2.5f},
 { IN_SUMMARY,"report",2.5f},{ IN_SUMMARY,"brief",2.5f},
 { IN_SUMMARY,"situation",2.5f},{ IN_SUMMARY,"total",2},

 { IN_BUSIEST,"busiest",3.5f},{ IN_BUSIEST,"peak",3},{ IN_BUSIEST,"busy",2.5f},
 { IN_BUSIEST,"rush",3},{ IN_BUSIEST,"quiet",2.5f},{ IN_BUSIEST,"hour",2},

 { IN_SAVE,"save",3.5f},{ IN_SAVE,"export",3.5f},{ IN_SAVE,"write",2.5f},
 { IN_SAVE,"file",2.5f},{ IN_SAVE,"csv",3},{ IN_SAVE,"disk",3},
 { IN_SAVE,"backup",3},

 { IN_ABOUTAI,"model",2.5f},{ IN_ABOUTAI,"neural",3.5f},{ IN_ABOUTAI,"network",3},
 { IN_ABOUTAI,"trained",3},{ IN_ABOUTAI,"algorithm",3},{ IN_ABOUTAI,"ai",2.5f},
 { IN_ABOUTAI,"machine",3},{ IN_ABOUTAI,"learning",3},{ IN_ABOUTAI,"work",1.5f},
 { IN_ABOUTAI,"you",1.2f},

 { IN_THANKS,"thanks",3.5f},{ IN_THANKS,"thank",3.5f},{ IN_THANKS,"cheers",3},
 { IN_THANKS,"merci",3.5f},{ IN_THANKS,"great",2},{ IN_THANKS,"perfect",2.5f},
 { IN_THANKS,"bye",3},{ IN_THANKS,"goodbye",3},
};

#define NCORPUS ((int)(sizeof CORPUS / sizeof CORPUS[0]))

static float g_idf[NCORPUS];
static int   g_idfReady;

static void build_idf(void)
{
    for (int i = 0; i < NCORPUS; i++) {
        int seen[IN_COUNT];
        memset(seen, 0, sizeof seen);
        for (int j = 0; j < NCORPUS; j++)
            if (strcmp(CORPUS[i].kw, CORPUS[j].kw) == 0) seen[CORPUS[j].intent] = 1;
        int df = 0;
        for (int k = 0; k < IN_COUNT; k++) df += seen[k];
        /* classic smoothed inverse document frequency */
        g_idf[i] = logf(1.f + (float)IN_COUNT / (float)(df > 0 ? df : 1));
    }
    g_idfReady = 1;
}

/* ==========================================================================
 *  tokenising
 * ========================================================================== */

#define MAX_TOK 40
#define TOK_LEN 26

typedef struct {
    char tok[MAX_TOK][TOK_LEN];
    char raw[MAX_TOK][TOK_LEN];
    int  n;
} Tokens;

static void tokenise(const char *in, Tokens *t)
{
    t->n = 0;
    int k = 0;
    char cur[TOK_LEN], rawc[TOK_LEN];
    for (const char *p = in; ; p++) {
        char c = *p;
        int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9');
        if (alnum && k < TOK_LEN - 1) {
            rawc[k] = c;
            cur[k]  = (char)tolower((unsigned char)c);
            k++;
        } else {
            if (k > 0 && t->n < MAX_TOK) {
                cur[k] = 0; rawc[k] = 0;
                memcpy(t->tok[t->n], cur, (size_t)k + 1);
                memcpy(t->raw[t->n], rawc, (size_t)k + 1);
                t->n++;
            }
            k = 0;
        }
        if (!c) break;
    }
}

/* ==========================================================================
 *  entity extraction
 * ========================================================================== */

typedef struct {
    Flight    *flight;
    Passenger *pax;
    Bag       *bag;
    int        airport;
    int        airline;
    int        standIdx;
    int        gate;
} Entities;

static int is_all_digits(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

static void upper_copy(const char *in, char *out, int cap)
{
    int i = 0;
    for (; in[i] && i < cap - 1; i++) out[i] = (char)toupper((unsigned char)in[i]);
    out[i] = 0;
}

static void extract(World *w, Tokens *t, Entities *e)
{
    memset(e, 0, sizeof *e);
    e->airport = e->airline = e->standIdx = -1;
    e->gate = -1;

    char up[TOK_LEN * 2];

    for (int i = 0; i < t->n; i++) {
        upper_copy(t->raw[i], up, sizeof up);

        /* a flight number on its own, e.g. MK046 / BA2065 */
        if (!e->flight && strlen(up) >= 4 && strlen(up) <= 7) {
            Flight *f = flight_by_no(w, up);
            if (f) { e->flight = f; continue; }
        }
        /* a flight number split across two tokens, e.g. "MK 046" */
        if (!e->flight && i + 1 < t->n && strlen(t->raw[i]) == 2 &&
            is_all_digits(t->raw[i+1])) {
            char join[16];
            snprintf(join, sizeof join, "%s%s", t->raw[i], t->raw[i+1]);
            upper_copy(join, up, sizeof up);
            Flight *f = flight_by_no(w, up);
            if (!f) {                       /* try zero padded: MK 46 -> MK046 */
                snprintf(join, sizeof join, "%s%03d", t->raw[i], atoi(t->raw[i+1]));
                upper_copy(join, up, sizeof up);
                f = flight_by_no(w, up);
            }
            if (f) { e->flight = f; i++; continue; }
        }
        /* baggage tag: two letters then six digits */
        if (!e->bag && strlen(up) == 8) {
            Bag *b = bag_by_tag(w, up);
            if (b) { e->bag = b; continue; }
        }
        /* booking reference */
        if (!e->pax && strlen(up) == 6) {
            Passenger *p = pax_by_pnr(w, up);
            if (p) { e->pax = p; continue; }
        }
        /* airport by code */
        if (e->airport < 0 && strlen(up) == 3) {
            for (int a = 0; a < w->nAirports; a++)
                if (strcmp(w->airport[a].iata, up) == 0) { e->airport = a; break; }
            if (e->airport >= 0) continue;
        }
        /* airport by city name */
        if (e->airport < 0 && strlen(t->tok[i]) >= 4) {
            for (int a = 0; a < w->nAirports; a++) {
                char city[26];
                int j = 0;
                for (; w->airport[a].city[j] && j < 25; j++)
                    city[j] = (char)tolower((unsigned char)w->airport[a].city[j]);
                city[j] = 0;
                if (strstr(city, t->tok[i])) { e->airport = a; break; }
            }
            if (e->airport >= 0) continue;
        }
        /* airline by code or by name */
        if (e->airline < 0) {
            for (int a = 0; a < w->nAirlines; a++) {
                if (strcmp(w->airline[a].iata, up) == 0) { e->airline = a; break; }
                char nm[36];
                int j = 0;
                for (; w->airline[a].name[j] && j < 35; j++)
                    nm[j] = (char)tolower((unsigned char)w->airline[a].name[j]);
                nm[j] = 0;
                if (strlen(t->tok[i]) >= 5 && strstr(nm, t->tok[i])) { e->airline = a; break; }
            }
        }
        /* stand designator */
        if (e->standIdx < 0 && strlen(up) >= 2 && strlen(up) <= 3) {
            for (int s = 0; s < w->nStands; s++)
                if (strcmp(w->stand[s].name, up) == 0) { e->standIdx = s; break; }
        }
        /* "gate 4" */
        if (strcmp(t->tok[i], "gate") == 0 && i + 1 < t->n &&
            is_all_digits(t->raw[i+1]))
            e->gate = atoi(t->raw[i+1]);
    }
}

/* ==========================================================================
 *  intent classification
 * ========================================================================== */

static int classify(Tokens *t, float *confOut)
{
    if (!g_idfReady) build_idf();

    float score[IN_COUNT];
    memset(score, 0, sizeof score);

    for (int i = 0; i < t->n; i++) {
        for (int c = 0; c < NCORPUS; c++) {
            if (strcmp(t->tok[i], CORPUS[c].kw) == 0)
                score[CORPUS[c].intent] += CORPUS[c].w * g_idf[c];
        }
    }
    /* length normalisation: a long sentence should not win by volume alone */
    float norm = sqrtf((float)(t->n > 0 ? t->n : 1));
    int best = IN_UNKNOWN;
    float bestS = 0.f, second = 0.f;
    for (int c = 0; c < IN_COUNT; c++) {
        score[c] /= norm;
        if (score[c] > bestS) { second = bestS; bestS = score[c]; best = c; }
        else if (score[c] > second) second = score[c];
    }
    if (bestS < 0.85f) { *confOut = bestS; return IN_UNKNOWN; }
    /* margin over the runner-up expressed as a confidence */
    float margin = bestS > 0.f ? (bestS - second) / bestS : 0.f;
    *confOut = cv_clampf(0.45f + 0.35f*margin + 0.20f*cv_clampf(bestS/4.f,0,1),
                         0.f, 0.99f);
    return best;
}

/* ==========================================================================
 *  response synthesis
 * ========================================================================== */

static void turn_add(ChatSession *s, int fromUser, const char *text,
                     int intent, float conf)
{
    if (s->n >= CHAT_MAX_TURNS) {
        memmove(&s->turn[0], &s->turn[1], sizeof(ChatTurn) * (CHAT_MAX_TURNS - 1));
        s->n = CHAT_MAX_TURNS - 1;
    }
    ChatTurn *t = &s->turn[s->n++];
    memset(t, 0, sizeof *t);
    t->fromUser  = fromUser;
    t->intent    = intent;
    t->confidence= conf;
    snprintf(t->text, CHAT_TEXT, "%s", text);
}

static void chips(ChatTurn *t, const char *a, const char *b, const char *c)
{
    t->chips = 0;
    if (a) snprintf(t->chip[t->chips++], 40, "%s", a);
    if (b) snprintf(t->chip[t->chips++], 40, "%s", b);
    if (c) snprintf(t->chip[t->chips++], 40, "%s", c);
}

static void describe_flight(World *w, Flight *f, char *out, int cap)
{
    char sch[8], est[8];
    fmt_hhmm(f->schedMin, sch);
    fmt_hhmm(f->estMin,   est);
    const char *dir = f->arrival ? "from" : "to";
    char standTxt[24];
    if (f->stand >= 0)
        snprintf(standTxt, sizeof standTxt, "%s", w->stand[f->stand].name);
    else
        snprintf(standTxt, sizeof standTxt, "not yet allocated");

    int n = snprintf(out, (size_t)cap,
        "%s  %s %s %s (%s)\n"
        "Status: %s\n"
        "Aircraft: %s, registration %s\n"
        "Scheduled %s, estimated %s%s\n"
        "Stand %s%s",
        f->no, w->airline[f->airline].name, dir,
        w->airport[f->airport].city, w->airport[f->airport].iata,
        fs_name(f->state),
        w->actype[f->acType].name, f->reg,
        sch, est,
        f->delayMin > 0 ? "  (delayed)" : "",
        standTxt,
        (f->gate > 0 && !f->arrival) ? "" : "");

    if (f->gate > 0 && !f->arrival && n < cap - 40)
        n += snprintf(out + n, (size_t)(cap - n), ", gate %d", f->gate);
    if (f->arrival && f->belt[0] && n < cap - 40)
        n += snprintf(out + n, (size_t)(cap - n), ", reclaim belt %s", f->belt);
    if (!f->arrival && n < cap - 90)
        snprintf(out + n, (size_t)(cap - n),
                 "\n%d of %d seats sold, %d checked in, %d bags loaded",
                 f->pax, f->paxCap, f->checkedIn, f->bagsLoaded);
}

static void respond(World *w, ChatSession *s, int intent, Entities *e,
                    Tokens *t, float conf)
{
    char b[CHAT_TEXT];
    b[0] = 0;

    int hadFlightInMessage = (e->flight != NULL);

    /* An entity named in this message outranks a keyword-only guess.  Without
     * this, "tell me about Rodrigues" scores as flight.status (on "tell" and
     * "about") and then answers about whatever flight was discussed last. */
    if (!hadFlightInMessage) {
        if (e->bag && (intent == IN_STATUS || intent == IN_UNKNOWN))
            intent = IN_BAGTRACK;
        else if (e->pax && (intent == IN_STATUS || intent == IN_UNKNOWN))
            intent = IN_PAX;
        else if (e->airport >= 0 && (intent == IN_STATUS || intent == IN_TIME ||
                                     intent == IN_UNKNOWN))
            intent = IN_DEST;
        else if (e->airline >= 0 && (intent == IN_STATUS || intent == IN_UNKNOWN))
            intent = IN_AIRLINE;
        else if (e->standIdx >= 0 && intent == IN_UNKNOWN)
            intent = IN_STAND;
    }

    /* dialogue context: "it" / "that flight" resolves to the last subject,
     * but only when this message named nothing of its own */
    if (!e->flight && s->lastFlight > 0 && e->airport < 0 && !e->bag && !e->pax) {
        Flight *f = flight_by_id(w, s->lastFlight);
        if (f && (intent == IN_GATE || intent == IN_TIME || intent == IN_STATUS ||
                  intent == IN_PREDICT))
            e->flight = f;
    }
    if (e->flight) s->lastFlight = e->flight->id;
    if (e->pax)    s->lastPax    = e->pax->id;
    if (e->bag)    s->lastBag    = e->bag->id;

    /* a bare entity with no keywords at all still does the obvious thing,
     * and should report the confidence the entity match earned */
    if (intent == IN_UNKNOWN) {
        if (e->flight)             { intent = IN_STATUS;    conf = 0.93f; }
        else if (e->bag)           { intent = IN_BAGTRACK;  conf = 0.93f; }
        else if (e->pax)           { intent = IN_PAX;       conf = 0.93f; }
        else if (e->airport >= 0)  { intent = IN_DEST;      conf = 0.88f; }
        else if (e->airline >= 0)  { intent = IN_AIRLINE;   conf = 0.88f; }
        else if (e->standIdx >= 0) { intent = IN_STAND;     conf = 0.88f; }
    }

    switch (intent) {

    case IN_GREET: {
        char hhmm[8]; fmt_hhmm((int)w->clock, hhmm);
        int hour = (int)w->clock / 60;
        const char *part = hour < 12 ? "Good morning" :
                           (hour < 18 ? "Good afternoon" : "Good evening");
        snprintf(b, CHAT_TEXT,
          "%s. AURA assistant, Plaisance operations.\n"
          "Local time %s, runway %02d in use, %s over the field.\n"
          "There are %d movements on today's programme. What do you need?",
          part, hhmm, w->activeRunway, wx_name(w->wxKind), w->nFlights);
    } break;

    case IN_HELP:
        snprintf(b, CHAT_TEXT,
          "I read the live operational state, so ask me in plain English.\n"
          "Things I handle well:\n"
          "  - any flight by number, e.g. \"where is MK046\"\n"
          "  - gates, stands, times and delay risk\n"
          "  - track a bag by its tag, or a passenger by booking reference\n"
          "  - security waiting time, check-in desks, baggage system load\n"
          "  - weather, runway in use and crosswind\n"
          "  - a summary of the whole operation\n"
          "I keep track of what we were discussing, so follow-ups like "
          "\"what gate?\" will work.");
        break;

    case IN_STATUS:
        if (e->flight) describe_flight(w, e->flight, b, CHAT_TEXT);
        else snprintf(b, CHAT_TEXT,
              "Give me a flight number and I will pull its record -- "
              "for example MK046, EK701 or BA2065.");
        break;

    case IN_GATE:
        if (e->flight) {
            Flight *f = e->flight;
            if (f->stand >= 0) {
                Stand *st = &w->stand[f->stand];
                snprintf(b, CHAT_TEXT,
                  "%s is allocated stand %s%s.\n%s\n%s",
                  f->no, st->name,
                  st->kind == ST_CONTACT ? " (contact stand, airbridge)"
                                         : " (remote stand, bussing)",
                  f->arrival ? "" : "Boarding gate follows the stand number.",
                  f->state == FS_BOARDING ? "Boarding is open now."
                  : (f->state == FS_FINAL ? "Final call is running." : ""));
            } else {
                snprintf(b, CHAT_TEXT,
                  "%s has no stand allocated yet. The allocator can propose "
                  "one -- open AI Suite and run the stand optimiser.", f->no);
            }
        } else if (e->standIdx >= 0) {
            Stand *st = &w->stand[e->standIdx];
            Flight *occ = NULL;
            for (int i = 0; i < w->nFlights; i++)
                if (w->flight[i].stand == e->standIdx &&
                    fabsf((float)w->flight[i].estMin - w->clock) < 90.f)
                    { occ = &w->flight[i]; break; }
            snprintf(b, CHAT_TEXT,
              "Stand %s: %s, maximum wingspan %.1f m%s.\n%s%s",
              st->name,
              st->kind == ST_CONTACT ? "contact stand" :
              (st->kind == ST_REMOTE ? "remote stand" : "cargo stand"),
              st->maxSpan, st->jetbridge ? ", airbridge fitted" : "",
              occ ? "Currently rostered to " : "No movement rostered nearby.",
              occ ? occ->no : "");
        } else {
            snprintf(b, CHAT_TEXT, "Which flight or stand did you mean?");
        }
        break;

    case IN_TIME:
        if (e->flight) {
            Flight *f = e->flight;
            char sch[8], est[8];
            fmt_hhmm(f->schedMin, sch);
            fmt_hhmm(f->estMin, est);
            float mins = (float)f->estMin - w->clock;
            char rel[64];
            if (mins > 0)  snprintf(rel, sizeof rel, "in %d minutes", (int)mins);
            else           snprintf(rel, sizeof rel, "%d minutes ago", (int)-mins);
            snprintf(b, CHAT_TEXT,
              "%s is scheduled %s and estimated %s -- that is %s.\n%s%s",
              f->no, sch, est, rel,
              f->delayMin > 0 ? "Running " : "On schedule.",
              f->delayMin > 0 ? (f->delayMin >= 60 ? "over an hour late."
                                                   : "a little behind.") : "");
        } else {
            char hhmm[8]; fmt_hhmm((int)w->clock, hhmm);
            snprintf(b, CHAT_TEXT,
              "Local time at Plaisance is %s (UTC+4). Give me a flight number "
              "for its timings.", hhmm);
        }
        break;

    case IN_DEPARTURES: {
        int n = 0, used = 0;
        used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                         "Next departures from Plaisance:\n");
        for (int pass = 0; pass < 2 && n < 5; pass++) {
            for (int i = 0; i < w->nFlights && n < 5; i++) {
                Flight *f = &w->flight[i];
                if (f->arrival || f->state >= FS_DEPARTED) continue;
                float d = (float)f->estMin - w->clock;
                if (pass == 0 && (d < 0.f || d > 180.f)) continue;
                if (pass == 1 && d >= 0.f) continue;
                char est[8]; fmt_hhmm(f->estMin, est);
                used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                    "  %s  %s  %-12s gate %-2d  %s\n", est, f->no,
                    w->airport[f->airport].city, f->gate, fs_name(f->state));
                n++;
            }
        }
        if (!n) snprintf(b, CHAT_TEXT, "Nothing further outbound today.");
    } break;

    case IN_ARRIVALS: {
        int n = 0, used = 0;
        used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                         "Next arrivals into Plaisance:\n");
        for (int i = 0; i < w->nFlights && n < 5; i++) {
            Flight *f = &w->flight[i];
            if (!f->arrival || f->state >= FS_ONBLOCK) continue;
            float d = (float)f->estMin - w->clock;
            if (d < -10.f || d > 240.f) continue;
            char est[8]; fmt_hhmm(f->estMin, est);
            used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                "  %s  %s  from %-12s belt %s  %s\n", est, f->no,
                w->airport[f->airport].city, f->belt, fs_name(f->state));
            n++;
        }
        if (!n) snprintf(b, CHAT_TEXT, "No arrivals due in the next four hours.");
    } break;

    case IN_BAGTRACK:
        if (e->bag) {
            Bag *g = e->bag;
            Flight *f = flight_by_id(w, g->flight);
            Passenger *p = NULL;
            for (int i = 0; i < w->nPax; i++)
                if (w->pax[i].id == g->pax) { p = &w->pax[i]; break; }
            snprintf(b, CHAT_TEXT,
              "Bag %s\n"
              "Owner: %s\nFlight: %s to %s\n"
              "Location: %s\nWeight: %.1f kg\n"
              "Screening score: %.2f%s",
              g->tag, p ? p->name : "unknown",
              f ? f->no : "----", f ? w->airport[f->airport].city : "----",
              bs_name(g->state), g->weight, g->threatScore,
              g->threat ? "  -- FLAGGED, diverted to manual search" : "  -- cleared");
        } else if (e->pax) {
            int n = 0, used = 0;
            used += snprintf(b, CHAT_TEXT, "Bags for %s:\n", e->pax->name);
            for (int i = 0; i < w->nBags; i++)
                if (w->bag[i].pax == e->pax->id) {
                    used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                        "  %s  %.1f kg  %s\n", w->bag[i].tag,
                        w->bag[i].weight, bs_name(w->bag[i].state));
                    n++;
                }
            if (!n) snprintf(b, CHAT_TEXT, "%s has no checked bags.", e->pax->name);
        } else {
            snprintf(b, CHAT_TEXT,
              "Give me a bag tag such as MK123456, or a booking reference and "
              "I will list that passenger's bags.");
        }
        break;

    case IN_BAGSTATS: {
        int inSys = sim_active_bags(w);
        int held  = sim_bags_in_state(w, BG_HELD);
        int load  = sim_bags_in_state(w, BG_LOADED);
        int scr   = sim_bags_in_state(w, BG_SCREEN);
        int srt   = sim_bags_in_state(w, BG_SORT);
        int mk    = sim_bags_in_state(w, BG_MAKEUP);
        snprintf(b, CHAT_TEXT,
          "Baggage handling system:\n"
          "  In the system now .... %d\n"
          "  In screening ......... %d\n"
          "  In sortation ......... %d\n"
          "  At make-up ........... %d\n"
          "  Loaded to aircraft ... %d\n"
          "  Held for search ...... %d\n"
          "  Mishandled today ..... %d\n"
          "Register holds %d bags in total.",
          inSys, scr, srt, mk, load, held, w->bagsMishandled, w->nBags);
    } break;

    case IN_PAX:
        if (e->pax) {
            Passenger *p = e->pax;
            Flight *f = flight_by_id(w, p->flight);
            const char *tier[] = { "none", "Silver", "Gold", "Platinum" };
            snprintf(b, CHAT_TEXT,
              "%s\nBooking %s\nFlight %s to %s\nSeat %s\n"
              "Bags checked: %d\nStatus: %s%s%s\nLoyalty tier: %s",
              p->name, p->pnr, f ? f->no : "----",
              f ? w->airport[f->airport].city : "----",
              p->seat, p->bags,
              p->boarded ? "boarded" : (p->security ? "through security"
                        : (p->checkedIn ? "checked in" : "not yet checked in")),
              p->wheelchair ? ", wheelchair assistance" : "",
              p->infant ? ", travelling with an infant" : "",
              tier[p->loyalty & 3]);
        } else {
            snprintf(b, CHAT_TEXT,
              "Give me a six character booking reference and I will pull the "
              "passenger record. You can also search the roster on the "
              "Check-in screen.");
        }
        break;

    case IN_CHECKIN: {
        int open = 0, q = 0;
        for (int i = 0; i < w->nDesks; i++)
            if (w->desk[i].open) { open++; q += w->desk[i].queue; }
        snprintf(b, CHAT_TEXT,
          "Check-in hall: %d of %d desks open, %d passengers queueing "
          "(about %d per desk).\n"
          "Desks open three hours before departure and close one hour before "
          "for long-haul, forty minutes for regional.",
          open, w->nDesks, q, open ? q/open : 0);
    } break;

    case IN_SECURITY: {
        float wait = sim_security_wait(w);
        int   q    = sim_security_queue(w);
        snprintf(b, CHAT_TEXT,
          "Central search: about %d passengers waiting, current wait "
          "approximately %.0f minutes across 6 lanes.\n%s",
          q, wait,
          wait > 14.f ? "That is above the service target -- consider opening "
                        "another lane."
                      : "Comfortably inside the ten minute service target.");
    } break;

    case IN_WEATHER:
        snprintf(b, CHAT_TEXT,
          "Plaisance weather:\n"
          "  Conditions ... %s\n"
          "  Temperature .. %.1f C\n"
          "  Wind ......... %03d degrees at %d knots\n"
          "  Visibility ... %.0f km\n"
          "  QNH .......... %.0f hPa\n"
          "  Humidity ..... %.0f%%\n"
          "Runway %02d is in use.",
          wx_name(w->wxKind), w->tempC, (int)w->windDir, (int)w->windKt,
          w->visKm, w->qnh, w->humidity, w->activeRunway);
        break;

    case IN_RUNWAY: {
        float rwyHdg = (w->activeRunway == 14) ? 140.f : 320.f;
        float diff = (w->windDir - rwyHdg) * 3.14159265f / 180.f;
        float cross = fabsf(sinf(diff)) * w->windKt;
        float head  = cosf(diff) * w->windKt;
        snprintf(b, CHAT_TEXT,
          "Runway %02d in use, 3,390 m of asphalt, single runway operation.\n"
          "Wind %03d/%02dkt gives a %.0f kt crosswind and a %.0f kt %s.\n"
          "%s",
          w->activeRunway, (int)w->windDir, (int)w->windKt, cross,
          fabsf(head), head >= 0.f ? "headwind" : "tailwind",
          cross > 20.f ? "Crosswind is significant -- watch for go-arounds."
                       : "Well within limits for all types on the programme.");
    } break;

    case IN_STAND: {
        int contact = 0, remote = 0, occ = 0;
        for (int i = 0; i < w->nStands; i++) {
            if (w->stand[i].kind == ST_CONTACT) contact++;
            else if (w->stand[i].kind == ST_REMOTE) remote++;
        }
        for (int i = 0; i < w->nFlights; i++)
            if (w->flight[i].stand >= 0 &&
                fabsf((float)w->flight[i].estMin - w->clock) < 60.f) occ++;
        snprintf(b, CHAT_TEXT,
          "Plaisance has %d contact stands with airbridges and %d remote "
          "stands, plus 2 cargo positions.\n"
          "%d movements are within an hour of their stand right now.\n"
          "The stand allocator in the AI Suite re-optimises the whole day by "
          "simulated annealing if you want a cleaner plan.",
          contact, remote, occ);
    } break;

    case IN_PREDICT:
        if (e->flight && !e->flight->arrival) {
            Flight *f = e->flight;
            snprintf(b, CHAT_TEXT,
              "Delay model for %s: %.0f%% probability of departing 15 minutes "
              "or more behind schedule.\n%s\n"
              "Largest contributors are shown on the Delay Oracle screen, "
              "where you can see each feature's weight.",
              f->no, f->delayRisk * 100.f,
              f->delayRisk > 0.6f ? "That is high -- worth a call to the "
                                    "handling agent."
              : (f->delayRisk > 0.3f ? "Moderate risk, keep an eye on it."
                                     : "Low risk on current inputs."));
        } else {
            int worst = -1; float wv = -1.f;
            for (int i = 0; i < w->nFlights; i++) {
                Flight *f = &w->flight[i];
                if (f->arrival || f->state >= FS_DEPARTED) continue;
                if (f->delayRisk > wv) { wv = f->delayRisk; worst = i; }
            }
            if (worst >= 0)
                snprintf(b, CHAT_TEXT,
                  "The highest departure delay risk on the board is %s to %s "
                  "at %.0f%%.\nAsk me about any flight by number for its own "
                  "prediction.", w->flight[worst].no,
                  w->airport[w->flight[worst].airport].city, wv*100.f);
            else
                snprintf(b, CHAT_TEXT, "No outbound movements left to score.");
        }
        break;

    case IN_DELAYED: {
        int used = snprintf(b, CHAT_TEXT, "Movements running behind:\n");
        int n = 0;
        for (int i = 0; i < w->nFlights && n < 6; i++) {
            Flight *f = &w->flight[i];
            if (f->delayMin < 15) continue;
            char est[8]; fmt_hhmm(f->estMin, est);
            used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                "  %s  %s %s %s  +%d min\n", est, f->no,
                f->arrival ? "from" : "to", w->airport[f->airport].city,
                f->delayMin);
            n++;
        }
        if (!n) snprintf(b, CHAT_TEXT,
                  "Nothing is more than fifteen minutes behind. "
                  "The programme is running clean.");
    } break;

    case IN_AIRPORT:
        snprintf(b, CHAT_TEXT,
          "Sir Seewoosagur Ramgoolam International Airport\n"
          "IATA MRU, ICAO FIMP, at Plaisance in the south east of Mauritius.\n"
          "Single runway 14/32, 3,390 m. Elevation 186 ft. Time zone UTC+4.\n"
          "The terminal opened in 2013 and handles the great majority of the "
          "island's traffic, with %d airlines on today's programme serving "
          "%d destinations.",
          w->nAirlines, w->nAirports - 1);
        break;

    case IN_AIRLINE:
        if (e->airline >= 0) {
            Airline *a = &w->airline[e->airline];
            int n = 0;
            for (int i = 0; i < w->nFlights; i++)
                if (w->flight[i].airline == e->airline) n++;
            snprintf(b, CHAT_TEXT,
              "%s (%s / %s), hub %s.\n%d movements at Plaisance today.",
              a->name, a->iata, a->icao, a->hub, n);
        } else {
            int used = snprintf(b, CHAT_TEXT, "Airlines on today's programme:\n");
            for (int i = 0; i < w->nAirlines && used < CHAT_TEXT - 40; i++) {
                int n = 0;
                for (int k = 0; k < w->nFlights; k++)
                    if (w->flight[k].airline == i) n++;
                if (!n) continue;
                used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
                    "  %s  %-22s %d movements\n", w->airline[i].iata,
                    w->airline[i].name, n);
            }
        }
        break;

    case IN_DEST:
        if (e->airport >= 0) {
            Airport *a = &w->airport[e->airport];
            int n = 0;
            char list[200]; list[0] = 0;
            for (int i = 0; i < w->nFlights; i++)
                if (w->flight[i].airport == e->airport && !w->flight[i].arrival) {
                    if (n < 4) {
                        char est[8]; fmt_hhmm(w->flight[i].estMin, est);
                        snprintf(list + strlen(list), 200 - strlen(list),
                                 "%s%s at %s", n ? ", " : "", w->flight[i].no, est);
                    }
                    n++;
                }
            snprintf(b, CHAT_TEXT,
              "%s (%s), %s.\nBlock time from Plaisance about %dh %02dm.\n"
              "%d departures today%s%s",
              a->city, a->iata, a->country,
              a->flightMin / 60, a->flightMin % 60, n,
              n ? ": " : ".", n ? list : "");
        } else {
            snprintf(b, CHAT_TEXT,
              "Plaisance serves %d destinations on today's programme, from "
              "Rodrigues at 95 minutes to Shanghai at over eleven hours. "
              "Name a city and I will give you its detail.", w->nAirports - 1);
        }
        break;

    case IN_STAFF: {
        int on = 0, e0 = 0, e1 = 0, e2 = 0;
        for (int i = 0; i < w->nStaff; i++) {
            if (w->staff[i].onDuty) on++;
            if (w->staff[i].shift == 0) e0++;
            else if (w->staff[i].shift == 1) e1++;
            else e2++;
        }
        snprintf(b, CHAT_TEXT,
          "Duty roster: %d of %d staff on duty.\n"
          "  Early shift ... %d\n  Late shift .... %d\n  Night shift ... %d\n"
          "The full roster is on the Records screen and writes to staff.csv.",
          on, w->nStaff, e0, e1, e2);
    } break;

    case IN_SUMMARY: {
        int dep = 0, arr = 0, dly = 0;
        for (int i = 0; i < w->nFlights; i++) {
            if (w->flight[i].arrival) arr++; else dep++;
            if (w->flight[i].delayMin >= 15) dly++;
        }
        char hhmm[8]; fmt_hhmm((int)w->clock, hhmm);
        snprintf(b, CHAT_TEXT,
          "Operations summary at %s\n"
          "  Movements ......... %d  (%d out, %d in)\n"
          "  Departed .......... %d\n"
          "  Landed ............ %d\n"
          "  Delayed 15+ ....... %d\n"
          "  On-time ........... %.0f%%\n"
          "  Seats sold ........ %d\n"
          "  Bags in system .... %d\n"
          "  Security wait ..... %.0f min\n"
          "  Runway ............ %02d, %s",
          hhmm, w->nFlights, dep, arr, w->totalDepartures, w->totalArrivals,
          dly, w->nFlights ? 100.f*(w->nFlights-dly)/w->nFlights : 100.f,
          w->totalPaxToday, sim_active_bags(w), sim_security_wait(w),
          w->activeRunway, wx_name(w->wxKind));
    } break;

    case IN_BUSIEST: {
        int bins[24];
        memset(bins, 0, sizeof bins);
        for (int i = 0; i < w->nFlights; i++)
            bins[(w->flight[i].estMin / 60) % 24]++;
        int best = 0, quiet = 0;
        for (int h = 1; h < 24; h++) {
            if (bins[h] > bins[best]) best = h;
            if (bins[h] < bins[quiet]) quiet = h;
        }
        snprintf(b, CHAT_TEXT,
          "The busiest hour today is %02d:00 to %02d:00 with %d movements. "
          "The quietest is %02d:00 with %d.\n"
          "Plaisance runs two clear banks -- European long-haul arriving at "
          "first light, and the outbound wave after 20:00.",
          best, (best+1)%24, bins[best], quiet, bins[quiet]);
    } break;

    case IN_SAVE:
        snprintf(b, CHAT_TEXT,
          "The Records screen writes the whole operational state to disk as "
          "comma separated files: flights.csv, passengers.csv, baggage.csv "
          "and staff.csv, plus an append-only journal.log.\n"
          "Open Records and use Write all records, or export the daily "
          "report as formatted text.");
        break;

    case IN_ABOUTAI: {
        BagNet *n = ai_bagnet();
        snprintf(b, CHAT_TEXT,
          "Five models run inside AURA, all written in C and all trained on "
          "this machine at start-up:\n"
          "  1. This assistant -- bag-of-words intent classification with "
          "IDF weighting and entity extraction\n"
          "  2. Delay Oracle -- logistic regression, batch gradient descent\n"
          "  3. BagScan -- a %d-%d-%d-1 perceptron trained by "
          "backpropagation, %.1f%% accurate with %.0f%% recall\n"
          "  4. Stand allocator -- simulated annealing over a constrained "
          "cost function\n"
          "  5. Flow forecast -- Holt double exponential smoothing into an "
          "M/M/c queue model\n"
          "Nothing here calls out to a service.",
          BAG_IN, BAG_H1, BAG_H2,
          n->trained ? n->accuracy*100.f : 0.f,
          n->trained ? n->recall*100.f : 0.f);
    } break;

    case IN_THANKS:
        snprintf(b, CHAT_TEXT,
          "Any time. I am here whenever you need the board read to you.");
        break;

    default: {
        /* Unknown: say so honestly, and offer the nearest thing that works */
        int used = snprintf(b, CHAT_TEXT,
          "I did not recognise that one with enough confidence to answer it "
          "properly.\n");
        if (t->n > 0)
            used += snprintf(b + used, (size_t)(CHAT_TEXT - used),
              "Try a flight number, a bag tag, or ask about security waits, "
              "weather, stands or the day's summary.");
        (void)used;
    } break;
    }

    turn_add(s, 0, b, intent, conf);
    ChatTurn *last = &s->turn[s->n - 1];

    switch (intent) {
    case IN_STATUS: case IN_GATE:
        chips(last, "Delay risk?", "What time?", "Bags loaded?"); break;
    case IN_GREET: case IN_HELP:
        chips(last, "Next departures", "Security wait", "Summary"); break;
    case IN_BAGSTATS:
        chips(last, "Any bags held?", "Baggage screen", "Summary"); break;
    case IN_SECURITY:
        chips(last, "Check-in desks", "Flow forecast", "Busiest hour"); break;
    case IN_WEATHER:
        chips(last, "Runway in use", "Crosswind?", "Any delays?"); break;
    case IN_SUMMARY:
        chips(last, "Which are delayed?", "Busiest hour", "Save records"); break;
    default:
        chips(last, "Next departures", "Weather", "Help"); break;
    }
}

/* ==========================================================================
 *  public interface
 * ========================================================================== */

void ai_chat_init(ChatSession *s)
{
    memset(s, 0, sizeof *s);
    if (!g_idfReady) build_idf();
    turn_add(s, 0,
      "AURA assistant online.\n"
      "I have the live movement board, the baggage register, the passenger "
      "roster and the weather in front of me. Ask me anything about the "
      "operation -- a flight number on its own works fine.",
      IN_GREET, 1.f);
    chips(&s->turn[0], "Next departures", "Security wait", "What can you do?");
}

void ai_chat_send(World *w, ChatSession *s, const char *text)
{
    if (!text || !*text) return;
    turn_add(s, 1, text, -1, 0.f);
    snprintf(s->pending, CHAT_TEXT, "%s", text);
    s->thinking = 1;
    s->thinkT   = 0.f;
    (void)w;
}

void ai_chat_update(World *w, ChatSession *s, float dt)
{
    for (int i = 0; i < s->n; i++) s->turn[i].t += dt;
    if (!s->thinking) return;

    s->thinkT += dt;
    /* a short, believable pause -- long enough to read as considered */
    if (s->thinkT < 0.55f) return;

    Tokens tk;
    Entities en;
    tokenise(s->pending, &tk);
    extract(w, &tk, &en);
    float conf = 0.f;
    int intent = classify(&tk, &conf);
    respond(w, s, intent, &en, &tk, conf);

    s->thinking = 0;
    s->pending[0] = 0;
}
