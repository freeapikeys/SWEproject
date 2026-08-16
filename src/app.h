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
#include "ai/ai.h"
#include "../include/theme.h"

enum {
    SC_AIRFIELD = 0,
    SC_BOARD,
    SC_CHECKIN,
    SC_BAGGAGE,
    SC_FLOW,
    SC_AI,
    SC_RECORDS,
    SC_COUNT
};

/* AI suite sub-tabs */
enum { AIT_ASSISTANT = 0, AIT_DELAY, AIT_BAGSCAN, AIT_STAND, AIT_FLOW, AIT_COUNT };

typedef struct {
    World        w;
    ChatSession  chat;
    DelayModel   delay;
    BagNet      *bagnet;
    StandPlan    plan;
    FlowModel    flow;

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

    /* flow */
    float  flowTimer;

    /* misc */
    int    showHelp;
    float  bootT;
    int    booted;
    ParticleSys fx;
    float  lastSaveT;
    char   statusLine[140];
} App;

/* screens ---------------------------------------------------------------- */
void screen_airfield(App *a, float x, float y, float w, float h);
void screen_board   (App *a, float x, float y, float w, float h);
void screen_checkin (App *a, float x, float y, float w, float h);
void screen_baggage (App *a, float x, float y, float w, float h);
void screen_flow    (App *a, float x, float y, float w, float h);
void screen_ai      (App *a, float x, float y, float w, float h);
void screen_records (App *a, float x, float y, float w, float h);

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

#endif /* AURA_APP_H */
