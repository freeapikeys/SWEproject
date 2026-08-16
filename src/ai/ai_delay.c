/* ==========================================================================
 *  AURA :: ai_delay.c   --   departure delay prediction
 *
 *  A logistic regression estimating P(this movement departs 15 minutes or
 *  more behind schedule).  It is trained on the machine at start-up by batch
 *  gradient descent over a generated operating history whose structure is
 *  taken from how Plaisance actually behaves: the late-evening long-haul
 *  bank is the fragile part of the day, a tight turnaround propagates, and
 *  weather over the island moves everything at once.
 *
 *  Logistic regression was chosen over something deeper on purpose.  The
 *  weights are readable, so the model can hand back the contribution of each
 *  feature and a duty manager can argue with it.
 * ========================================================================== */

#include "ai.h"
#include "../engine/anim.h"
#include <math.h>
#include <string.h>

const char *DELAY_FEATURE_NAME[DELAY_FEATURES] = {
    "Time of day",
    "Inbound aircraft late",
    "Turnaround slack",
    "Wide-body operation",
    "Seat load factor",
    "Crosswind component",
    "Weather severity",
    "Remote stand"
};

static float sigmoid(float z)
{
    if (z >  30.f) return 1.f;
    if (z < -30.f) return 0.f;
    return 1.f / (1.f + expf(-z));
}

/* --------------------------------------------------------------------------
 *  feature extraction -- everything normalised to roughly 0..1
 * ------------------------------------------------------------------------- */

static void features_for(World *w, Flight *f, float *x)
{
    /* 0: time of day, peaked around the evening long-haul bank */
    float hour = (float)f->schedMin / 60.f;
    float evening = expf(-((hour - 21.5f)*(hour - 21.5f)) / 12.f);
    float morning = expf(-((hour -  6.0f)*(hour -  6.0f)) / 10.f);
    x[0] = cv_clampf(evening * 0.85f + morning * 0.45f, 0.f, 1.f);

    /* 1: is the inbound aircraft already running late? */
    float inbound = 0.f;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *g = &w->flight[i];
        if (!g->arrival) continue;
        if (strcmp(g->reg, f->reg) != 0) continue;
        if (g->estMin > f->schedMin) continue;
        inbound = cv_clampf((float)g->delayMin / 60.f, 0.f, 1.f);
        break;
    }
    x[1] = inbound;

    /* 2: turnaround slack -- less ground time means less room to recover */
    float slack = 1.f;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *g = &w->flight[i];
        if (!g->arrival || strcmp(g->reg, f->reg) != 0) continue;
        float ground = (float)(f->schedMin - g->schedMin);
        if (ground <= 0.f) continue;
        slack = cv_clampf(ground / 180.f, 0.f, 1.f);
        break;
    }
    x[2] = 1.f - slack;

    /* 3..4: equipment and load */
    x[3] = w->actype[f->acType].widebody ? 1.f : 0.f;
    x[4] = f->paxCap > 0 ? cv_clampf((float)f->pax / (float)f->paxCap, 0.f, 1.f) : 0.f;

    /* 5: crosswind against the runway in use */
    float rwyHdg = (w->activeRunway == 14) ? 140.f : 320.f;
    float diff   = (w->windDir - rwyHdg) * 3.14159265f / 180.f;
    x[5] = cv_clampf(fabsf(sinf(diff)) * (w->windKt / 30.f), 0.f, 1.f);

    /* 6: weather severity */
    x[6] = cv_clampf((float)w->wxKind / 4.f, 0.f, 1.f);

    /* 7: remote stands need buses, which costs minutes */
    x[7] = (f->stand >= 0 && w->stand[f->stand].kind != ST_CONTACT) ? 1.f : 0.f;
}

/* --------------------------------------------------------------------------
 *  training set
 * ------------------------------------------------------------------------- */

#define TRAIN_N 900

static void make_training_set(float X[TRAIN_N][DELAY_FEATURES],
                              float y[TRAIN_N], uint32_t *rng)
{
    /* the generating process the model has to rediscover */
    const float truth[DELAY_FEATURES] =
        { 1.55f, 2.35f, 1.75f, 0.55f, 0.85f, 1.15f, 1.95f, 0.75f };
    const float bias = -2.55f;

    for (int i = 0; i < TRAIN_N; i++) {
        float z = bias;
        for (int k = 0; k < DELAY_FEATURES; k++) {
            float v;
            if (k == 3 || k == 7) v = rnd_f(rng) > 0.62f ? 1.f : 0.f;
            else if (k == 4)      v = rnd_range(rng, 0.55f, 1.f);
            else                  v = rnd_f(rng);
            v = cv_clampf(v, 0.f, 1.f);
            X[i][k] = v;
            z += truth[k] * v;
        }
        float p = 1.f / (1.f + expf(-z));
        /* label noise: real operations are not deterministic */
        y[i] = (rnd_f(rng) < p) ? 1.f : 0.f;
    }
}

void ai_delay_train(DelayModel *m, World *w)
{
    static float X[TRAIN_N][DELAY_FEATURES];
    static float y[TRAIN_N];

    uint32_t rng = 0x51EED17u ^ (uint32_t)w->nFlights;
    make_training_set(X, y, &rng);

    memset(m, 0, sizeof *m);
    m->samples = TRAIN_N;

    const float lr = 0.55f;
    const int   EPOCHS = 640;
    float grad[DELAY_FEATURES];

    for (int ep = 0; ep < EPOCHS; ep++) {
        float gb = 0.f, loss = 0.f;
        memset(grad, 0, sizeof grad);

        for (int i = 0; i < TRAIN_N; i++) {
            float z = m->b;
            for (int k = 0; k < DELAY_FEATURES; k++) z += m->w[k] * X[i][k];
            float p = sigmoid(z);
            float e = p - y[i];
            for (int k = 0; k < DELAY_FEATURES; k++) grad[k] += e * X[i][k];
            gb += e;
            float pc = cv_clampf(p, 1e-6f, 1.f - 1e-6f);
            loss += -(y[i]*logf(pc) + (1.f - y[i])*logf(1.f - pc));
        }
        float inv = 1.f / TRAIN_N;
        for (int k = 0; k < DELAY_FEATURES; k++)
            m->w[k] -= lr * (grad[k]*inv + 0.0015f * m->w[k]);   /* L2 */
        m->b -= lr * gb * inv;

        if ((ep % 10) == 0 && m->nCurve < 64)
            m->lossCurve[m->nCurve++] = loss * inv;
        m->loss = loss * inv;
    }

    /* accuracy on the training population */
    int right = 0;
    for (int i = 0; i < TRAIN_N; i++) {
        float z = m->b;
        for (int k = 0; k < DELAY_FEATURES; k++) z += m->w[k] * X[i][k];
        int pred = sigmoid(z) > 0.5f;
        if (pred == (int)y[i]) right++;
    }
    m->accuracy = (float)right / TRAIN_N;
    m->epochs   = EPOCHS;
    m->trained  = 1;
}

float ai_delay_predict(DelayModel *m, World *w, Flight *f, float *contrib)
{
    float x[DELAY_FEATURES];
    features_for(w, f, x);
    float z = m->b;
    for (int k = 0; k < DELAY_FEATURES; k++) {
        float c = m->w[k] * x[k];
        z += c;
        if (contrib) contrib[k] = c;
    }
    return sigmoid(z);
}

void ai_delay_run_all(DelayModel *m, World *w)
{
    if (!m->trained) return;
    for (int i = 0; i < w->nFlights; i++) {
        Flight *f = &w->flight[i];
        if (f->arrival) continue;
        float c[DELAY_FEATURES];
        f->delayRisk = ai_delay_predict(m, w, f, c);
        for (int k = 0; k < 6 && k < DELAY_FEATURES; k++) f->riskFeat[k] = c[k];
    }
}
