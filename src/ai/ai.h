/* ==========================================================================
 *  AURA :: ai.h  --  the five decision-support engines
 *
 *  None of these call out to a service.  Every model here is implemented in
 *  C, trains on the machine at start-up and runs against live state:
 *
 *    1. ai_chat     natural language assistant   -- bag-of-words intent
 *                   classification with inverse document frequency weighting,
 *                   entity extraction and a dialogue context stack
 *    2. ai_delay    departure delay prediction   -- logistic regression
 *                   trained by batch gradient descent, with per-feature
 *                   contributions so a controller can see *why*
 *    3. ai_bagscan  hold-baggage threat triage   -- 7-12-8-1 multilayer
 *                   perceptron trained by backpropagation
 *    4. ai_stand    stand allocation             -- simulated annealing over
 *                   a constrained assignment cost function
 *    5. ai_flow     terminal flow forecasting    -- Holt double exponential
 *                   smoothing feeding an M/M/c queue model
 * ========================================================================== */
#ifndef AURA_AI_H
#define AURA_AI_H

#include "../core/model.h"

/* ==========================================================================
 *  1. assistant
 * ========================================================================== */

#define CHAT_MAX_TURNS  60
#define CHAT_TEXT       620

typedef struct {
    int   fromUser;
    char  text[CHAT_TEXT];
    float t;                 /* age, for the typing animation              */
    int   intent;
    float confidence;
    int   chips;             /* how many follow-up suggestions             */
    char  chip[3][40];
} ChatTurn;

typedef struct {
    ChatTurn turn[CHAT_MAX_TURNS];
    int      n;
    int      lastFlight;     /* dialogue context                            */
    int      lastPax;
    int      lastBag;
    int      thinking;
    float    thinkT;
    char     pending[CHAT_TEXT];
} ChatSession;

void        ai_chat_init(ChatSession *s);
void        ai_chat_send(World *w, ChatSession *s, const char *text);
void        ai_chat_update(World *w, ChatSession *s, float dt);
const char *ai_intent_name(int intent);

/* ==========================================================================
 *  2. delay prediction
 * ========================================================================== */

#define DELAY_FEATURES 8

typedef struct {
    float w[DELAY_FEATURES];
    float b;
    int   trained;
    int   epochs;
    float loss;
    float accuracy;
    float lossCurve[64];
    int   nCurve;
    int   samples;
} DelayModel;

extern const char *DELAY_FEATURE_NAME[DELAY_FEATURES];

void  ai_delay_train  (DelayModel *m, World *w);
float ai_delay_predict(DelayModel *m, World *w, Flight *f, float *contrib);
void  ai_delay_run_all(DelayModel *m, World *w);

/* ==========================================================================
 *  3. baggage threat classifier
 * ========================================================================== */

/* Operating threshold for the diverter.  Deliberately below 0.5: a missed
 * threat costs far more than a bag opened unnecessarily, so the point is
 * pushed down the ROC curve to buy recall at the expense of precision. */
#define BAG_THRESHOLD 0.38f

#define BAG_IN   7
#define BAG_H1  12
#define BAG_H2   8

typedef struct {
    float w1[BAG_H1][BAG_IN], b1[BAG_H1];
    float w2[BAG_H2][BAG_H1], b2[BAG_H2];
    float w3[BAG_H2],         b3;
    int   trained, epochs, samples;
    float loss, accuracy, precision, recall;
    float lossCurve[64];
    int   nCurve;
} BagNet;

extern const char *BAG_FEATURE_NAME[BAG_IN];

void  ai_bagnet_train(BagNet *n);
float ai_bagnet_eval (BagNet *n, const float *x, float *hidden1, float *hidden2);
void  ai_bag_features(World *w, Bag *b, float *out);
void  bag_draw_features(float *f, int threat, uint32_t *rng);
float ai_bag_score   (World *w, Bag *b);
BagNet *ai_bagnet(void);

/* ==========================================================================
 *  4. stand allocation
 * ========================================================================== */

typedef struct {
    int   assign[MAX_FLIGHTS];      /* flight index -> stand index          */
    float cost;
    float startCost;
    float bestCurve[80];
    int   nCurve;
    int   iterations;
    int   conflicts;
    int   contactUsed;
    int   towMoves;
    float walkAvg;
    int   ready;
} StandPlan;

void  ai_stand_optimise(StandPlan *p, World *w, int iterations);
float ai_stand_cost    (World *w, const int *assign, int *conflicts,
                        int *contactUsed, float *walkAvg);
int   ai_stand_hard    (World *w, const int *assign);
void  ai_stand_apply   (StandPlan *p, World *w);
void  ai_stand_scramble(World *w);

/* ==========================================================================
 *  5. passenger flow forecasting
 * ========================================================================== */

#define FLOW_HIST 48
#define FLOW_HORIZON 24

typedef struct {
    float history[FLOW_HIST];       /* passengers presenting, 5-min bins    */
    int   nHist;
    float level, trend;
    float alpha, beta;
    float forecast[FLOW_HORIZON];
    float upper[FLOW_HORIZON];
    float lower[FLOW_HORIZON];
    float waitNow, waitPeak;
    int   lanesOpen, lanesNeeded;
    float utilisation;
    float mape;
    int   ready;
} FlowModel;

void  ai_flow_observe (FlowModel *m, World *w);
void  ai_flow_forecast(FlowModel *m, World *w);
float ai_flow_queue_wait(float arrivalRate, float serviceRate, int servers);

#endif /* AURA_AI_H */
