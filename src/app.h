/* ==========================================================================
 *  AURA :: app.h  --  application state and screen contract
 *
 *  AURA -- Airport Unified Resource Administration
 *  Sir Seewoosagur Ramgoolam International Airport, Plaisance, Mauritius
 *  Trois Freres Systems Ltd.
 * ========================================================================== */
#ifndef AURA_APP_H
#define AURA_APP_H

#include "engine/canvas.h"
#include "engine/text.h"
#include "engine/anim.h"
#include "engine/ui.h"
#include "engine/icons.h"
#include "core/model.h"
#include "core/sim.h"
#include "core/store.h"
#include "core/auth.h"
#include "core/queue.h"
#include "core/ops.h"
#include "core/incident.h"
#include "core/feedback.h"
#include "core/analytics.h"
#include "ai/ai.h"
#include "cv/vision.h"
#include "../include/theme.h"

/*  Fifteen screens is too many for a flat list, so the sidebar is grouped.
 *  The order here is the order they appear, and NAV in main.c gives each one
 *  its section heading.                                                    */
enum {
    SC_WELCOME = 0,     /* passenger  */
    SC_BOARD,
    SC_SERVICES,
    SC_REVIEWS,

    SC_OPS,             /* operations */
    SC_AIRFIELD,
    SC_CHECKIN,
    SC_BAGGAGE,
    SC_FLOW,

    SC_EMERGENCY,       /* safety     */
    SC_VISION,

    SC_AI,              /* insight    */
    SC_ANALYTICS,

    SC_RESOURCES,       /* admin      */
    SC_RECORDS,
    SC_COUNT
};

/* AI suite sub-tabs */
enum { AIT_ASSISTANT = 0, AIT_DELAY, AIT_BAGSCAN, AIT_STAND, AIT_FLOW, AIT_COUNT };

/* Operations sub-tabs */
enum { OPT_FLIGHTS = 0, OPT_GATES, OPT_RUNWAY, OPT_NEW, OPT_COUNT };

/* Resources sub-tabs */
enum { RST_AIRCRAFT = 0, RST_STAFF, RST_SHIFTS, RST_ASSIST, RST_LOST,
       RST_COUNT };

typedef struct {
    World        w;
    AuthSystem   auth;
    ChatSession  chat;
    DelayModel   delay;
    BagNet      *bagnet;
    StandPlan    plan;
    FlowModel    flow;
    VisionSystem vision;

    /* operational subsystems */
    EmergencyLog emg;
    NotifyCentre notify;
    ReviewBook   reviews;
    LostBook     lost;
    PaxQueue     qSecurity;
    PaxQueue     qBoarding;
    Report       report;
    float        reportTimer;
    float        queueTimer;
    float        notifyTimer;

    Canvas       cv;
    Input        in;
    float        dt;

    int    screen;
    int    aiTab;
    int    boardArrivals;
    int    prevScreen;
    float  screenFade;

    /* selections */
    int    selFlight;          /* flight id, 0 = none                       */
    int    selBag;
    int    selPax;
    int    selStand;
    int    inspectOpen;

    /* the way in */
    char   loginEmail[AUTH_EMAIL_MAX];
    char   loginName [AUTH_NAME_MAX];
    char   loginPw   [AUTH_PW_MAX];
    char   loginPw2  [AUTH_PW_MAX];
    int    loginKind;

    /* the forward schedule browser */
    int    boardMode;          /* 0 live board, 1 forward timetable         */
    int    schedOffset;        /* days from today, 0 .. SCHEDULE_DAYS-1     */

    /* operations */
    int    opsTab;
    char   opsSearch[64];
    int    opsGateTarget;
    char   nfNo[8];            /* the new-flight form                       */
    char   nfTime[8];
    int    nfAirline, nfType, nfDest, nfArrival;

    /* emergency */
    int    emgSel;
    int    emgNewKind;

    /* reviews */
    int    rvService;
    int    rvStars;
    int    rvIndex;            /* carousel position                         */
    int    rvShowAll;
    char   rvText[REVIEW_TEXT];

    /* resources */
    int    resTab;
    char   resSearch[64];
    char   lpWhat[60];
    char   lpWhere[40];

    /* check-in */
    char   ciName[38];
    int    ciFlight;           /* flight id for the new passenger           */

    /* welcome screen: one-shot match of the signed-in traveller's booking */
    int    welcomeMatched;

    /* text buffers */
    char   chatInput[220];
    char   searchBoard[64];
    char   searchPax[64];
    char   searchBag[64];

    /* chat dock */
    int    chatDock;
    float  chatDockT;

    /* airfield view */
    float  fieldZoom, fieldPanX, fieldPanY;
    int    fieldLabels, fieldTrails, fieldNight;

    /* baggage view */
    int    bagAutoInject;
    float  bagInjectT;
    int    bagFocus;
    int    bagIdentify;        /* CV-style per-bag tracking overlay          */

    /* flow */
    float  flowTimer;

    /* surveillance overlays */
    int    cvFocus;
    int    cvIds, cvTrails, cvHeatmap, cvFlow, cvDets, cvTruth;

    /* misc */
    int    showHelp;
    float  bootT;
    int    booted;
    ParticleSys fx;
    float  lastSaveT;
    char   statusLine[140];
} App;

/* screens ---------------------------------------------------------------- */
void screen_airfield (App *a, float x, float y, float w, float h);
void screen_board    (App *a, float x, float y, float w, float h);
void screen_checkin  (App *a, float x, float y, float w, float h);
void screen_baggage  (App *a, float x, float y, float w, float h);
void screen_flow     (App *a, float x, float y, float w, float h);
void screen_vision   (App *a, float x, float y, float w, float h);
void screen_ai       (App *a, float x, float y, float w, float h);
void screen_records  (App *a, float x, float y, float w, float h);
void screen_services (App *a, float x, float y, float w, float h);
void screen_welcome  (App *a, float x, float y, float w, float h);
void screen_ops      (App *a, float x, float y, float w, float h);
void screen_emergency(App *a, float x, float y, float w, float h);
void screen_analytics(App *a, float x, float y, float w, float h);
void screen_reviews  (App *a, float x, float y, float w, float h);
void screen_resources(App *a, float x, float y, float w, float h);
void screen_login    (App *a, float sw, float sh);

/* shared drawing helpers ------------------------------------------------- */
void draw_aircraft (Canvas *c, float cx, float cy, float heading, float span,
                    Color body, Color accent, int strobeOn, float detail);
void draw_stat_tile(Canvas *c, float x, float y, float w, float h,
                    const char *label, const char *value, const char *sub,
                    int icon, Color accent);
void draw_spark    (Canvas *c, float x, float y, float w, float h,
                    const float *v, int n, Color col, int fill);
void draw_chat_panel(App *a, float x, float y, float w, float h, int compact);
void draw_flight_row(App *a, float x, float y, float w, float h, Flight *f,
                     int selected, int showRisk);
const char *ordinal_gate(int gate, char *buf);

/* shared by several screens: a row of stars, and a service rating pill */
void draw_stars(Canvas *c, float x, float y, float size, float value,
                int outOf, Color on, Color off);

/* the AURA badge -- sidebar, login and boot all draw the same mark */
void draw_logo(Canvas *c, float cx, float cy, float r);

#endif /* AURA_APP_H */
