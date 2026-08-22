/* ==========================================================================
 *  AURA :: ai_bagscan.c   --   hold baggage threat triage
 *
 *  A 7-12-8-1 multilayer perceptron trained by backpropagation, standing in
 *  for the automatic tomography stage of a hold baggage screening system.
 *  It reads seven features taken off the scan and returns a single score;
 *  anything above the operating threshold is diverted to manual search
 *  instead of continuing to the make-up carousel.
 *
 *  The network is trained once at start-up.  Level 3 screening decisions are
 *  a triage problem, not a verdict -- the model is tuned to favour recall,
 *  because a missed threat costs far more than a bag opened unnecessarily.
 * ========================================================================== */

#include "ai.h"
#include "../engine/anim.h"
#include <math.h>
#include <string.h>

const char *BAG_FEATURE_NAME[BAG_IN] = {
    "Mean density",
    "Metal signature",
    "Organic ratio",
    "Edge complexity",
    "Shape entropy",
    "Mass anomaly",
    "Route risk index"
};

static BagNet g_net;

BagNet *ai_bagnet(void) { return &g_net; }

static float sigmoidf_(float z)
{
    if (z >  30.f) return 1.f;
    if (z < -30.f) return 0.f;
    return 1.f / (1.f + expf(-z));
}

/* leaky rectifier keeps gradients alive in the hidden stacks */
static float lrelu (float v) { return v > 0.f ? v : 0.01f * v; }
static float dlrelu(float v) { return v > 0.f ? 1.f : 0.01f; }

/* ==========================================================================
 *  forward pass
 * ========================================================================== */

float ai_bagnet_eval(BagNet *n, const float *x, float *h1out, float *h2out)
{
    float h1[BAG_H1], h2[BAG_H2];
    for (int j = 0; j < BAG_H1; j++) {
        float s = n->b1[j];
        for (int i = 0; i < BAG_IN; i++) s += n->w1[j][i] * x[i];
        h1[j] = lrelu(s);
        if (h1out) h1out[j] = h1[j];
    }
    for (int j = 0; j < BAG_H2; j++) {
        float s = n->b2[j];
        for (int i = 0; i < BAG_H1; i++) s += n->w2[j][i] * h1[i];
        h2[j] = lrelu(s);
        if (h2out) h2out[j] = h2[j];
    }
    float s = n->b3;
    for (int i = 0; i < BAG_H2; i++) s += n->w3[i] * h2[i];
    return sigmoidf_(s);
}

/* ==========================================================================
 *  synthetic scan corpus
 *
 *  Threat-positive bags are drawn from a different region of feature space:
 *  dense, metallic, structurally complex, with a mass that does not agree
 *  with the declared contents.  The two populations deliberately overlap --
 *  a perfectly separable training set would teach the network nothing about
 *  the false-alarm rate the screening hall actually lives with.
 * ========================================================================== */

#define BAG_TRAIN 3000

/*  The two populations, as [threat lo, threat hi, clean lo, clean hi] per
 *  feature.  Training and live scanning both draw from this one table, which
 *  is the only way to guarantee the network sees the same distribution in
 *  service that it saw in training.
 *
 *  The ranges deliberately overlap heavily.  Cleanly separable classes would
 *  train to a hundred percent and teach the model nothing about the false
 *  alarm rate a screening hall actually lives with.                          */
static const float FEAT_RANGE[BAG_IN][4] = {
    /*  threat lo, hi     clean lo, hi   */
    { 0.38f, 1.00f,     0.00f, 0.72f },   /* mean density        */
    { 0.34f, 1.00f,     0.00f, 0.70f },   /* metal signature     */
    { 0.00f, 0.68f,     0.22f, 1.00f },   /* organic ratio       */
    { 0.36f, 1.00f,     0.00f, 0.74f },   /* edge complexity     */
    { 0.30f, 1.00f,     0.00f, 0.70f },   /* shape entropy       */
    { 0.28f, 1.00f,     0.00f, 0.66f },   /* mass anomaly        */
    { 0.18f, 1.00f,     0.00f, 0.68f },   /* route risk          */
};

void bag_draw_features(float *f, int threat, uint32_t *rng)
{
    for (int k = 0; k < BAG_IN; k++) {
        float lo = threat ? FEAT_RANGE[k][0] : FEAT_RANGE[k][2];
        float hi = threat ? FEAT_RANGE[k][1] : FEAT_RANGE[k][3];
        f[k] = cv_clampf(rnd_range(rng, lo, hi) +
                         rnd_range(rng, -0.09f, 0.09f), 0.f, 1.f);
    }
}

static void make_bag_corpus(float X[BAG_TRAIN][BAG_IN], float y[BAG_TRAIN],
                            uint32_t *rng)
{
    for (int i = 0; i < BAG_TRAIN; i++) {
        int threat = (rnd_f(rng) < 0.20f);
        bag_draw_features(X[i], threat, rng);
        /* Label noise: the search team occasionally records a result
         * wrongly.  This sets a hard ceiling on achievable recall, so it is
         * kept to a realistic rate rather than an arbitrary one. */
        if (rnd_f(rng) < 0.025f) threat = !threat;
        y[i] = threat ? 1.f : 0.f;
    }
}

/* ==========================================================================
 *  training  --  backpropagation with momentum
 * ========================================================================== */

void ai_bagnet_train(BagNet *n)
{
    static float X[BAG_TRAIN][BAG_IN];
    static float y[BAG_TRAIN];
    uint32_t rng = 0xBA6C0DEu;

    make_bag_corpus(X, y, &rng);
    memset(n, 0, sizeof *n);
    n->samples = BAG_TRAIN;

    /* He-style initialisation keeps the first activations in range */
    float s1 = sqrtf(2.f / BAG_IN), s2 = sqrtf(2.f / BAG_H1), s3 = sqrtf(2.f / BAG_H2);
    for (int j = 0; j < BAG_H1; j++) {
        for (int i = 0; i < BAG_IN; i++) n->w1[j][i] = rnd_range(&rng, -1.f, 1.f) * s1;
        n->b1[j] = 0.f;
    }
    for (int j = 0; j < BAG_H2; j++) {
        for (int i = 0; i < BAG_H1; i++) n->w2[j][i] = rnd_range(&rng, -1.f, 1.f) * s2;
        n->b2[j] = 0.f;
    }
    for (int i = 0; i < BAG_H2; i++) n->w3[i] = rnd_range(&rng, -1.f, 1.f) * s3;
    n->b3 = 0.f;

    static float v1[BAG_H1][BAG_IN], vb1[BAG_H1];
    static float v2[BAG_H2][BAG_H1], vb2[BAG_H2];
    static float v3[BAG_H2]; static float vb3;
    memset(v1,0,sizeof v1); memset(vb1,0,sizeof vb1);
    memset(v2,0,sizeof v2); memset(vb2,0,sizeof vb2);
    memset(v3,0,sizeof v3); vb3 = 0.f;

    /* One "epoch" is BAG_TRAIN stochastic updates, so enlarging the corpus
     * also multiplies the number of steps taken.  Keeping the old step size
     * after tripling the corpus drove the weights past the point where the
     * leaky units still carry gradient, and the network collapsed onto the
     * majority class -- 79% accurate and completely useless, because it
     * flagged nothing at all.  The step is scaled to the new budget. */
    const float lr = 0.02f, mom = 0.86f;
    const int   EPOCHS = 220;

    for (int ep = 0; ep < EPOCHS; ep++) {
        float loss = 0.f;
        for (int s = 0; s < BAG_TRAIN; s++) {
            int i = rnd_int(&rng, 0, BAG_TRAIN - 1);
            const float *x = X[i];

            /* forward, keeping the pre-activations for the backward pass */
            float z1[BAG_H1], a1[BAG_H1], z2[BAG_H2], a2[BAG_H2];
            for (int j = 0; j < BAG_H1; j++) {
                float z = n->b1[j];
                for (int k = 0; k < BAG_IN; k++) z += n->w1[j][k]*x[k];
                z1[j] = z; a1[j] = lrelu(z);
            }
            for (int j = 0; j < BAG_H2; j++) {
                float z = n->b2[j];
                for (int k = 0; k < BAG_H1; k++) z += n->w2[j][k]*a1[k];
                z2[j] = z; a2[j] = lrelu(z);
            }
            float z3 = n->b3;
            for (int k = 0; k < BAG_H2; k++) z3 += n->w3[k]*a2[k];
            float out = sigmoidf_(z3);

            /* Class-weighted cross entropy.  Missing a threat is far more
             * costly than opening a clean bag, so a positive example is worth
             * POS_WEIGHT times a negative one.  This is what actually buys
             * recall -- moving the decision threshold alone barely shifts it
             * once the outputs have saturated. */
            const float POS_WEIGHT = 3.4f;
            float wgt = (y[i] > 0.5f) ? POS_WEIGHT : 1.f;

            float pc = cv_clampf(out, 1e-6f, 1.f-1e-6f);
            loss += -wgt * (y[i]*logf(pc) + (1.f-y[i])*logf(1.f-pc));

            /* backward: cross entropy through a logistic output gives out-y */
            float d3 = (out - y[i]) * wgt;
            float d2[BAG_H2], d1[BAG_H1];
            for (int j = 0; j < BAG_H2; j++) d2[j] = d3 * n->w3[j] * dlrelu(z2[j]);
            for (int j = 0; j < BAG_H1; j++) {
                float acc = 0.f;
                for (int k = 0; k < BAG_H2; k++) acc += d2[k] * n->w2[k][j];
                d1[j] = acc * dlrelu(z1[j]);
            }

            for (int j = 0; j < BAG_H2; j++) {
                v3[j] = mom*v3[j] - lr*d3*a2[j];
                n->w3[j] += v3[j];
            }
            vb3 = mom*vb3 - lr*d3; n->b3 += vb3;

            for (int j = 0; j < BAG_H2; j++) {
                for (int k = 0; k < BAG_H1; k++) {
                    v2[j][k] = mom*v2[j][k] - lr*d2[j]*a1[k];
                    n->w2[j][k] += v2[j][k];
                }
                vb2[j] = mom*vb2[j] - lr*d2[j]; n->b2[j] += vb2[j];
            }
            for (int j = 0; j < BAG_H1; j++) {
                for (int k = 0; k < BAG_IN; k++) {
                    v1[j][k] = mom*v1[j][k] - lr*d1[j]*x[k];
                    n->w1[j][k] += v1[j][k];
                }
                vb1[j] = mom*vb1[j] - lr*d1[j]; n->b1[j] += vb1[j];
            }
        }
        n->loss = loss / BAG_TRAIN;
        if ((ep % 4) == 0 && n->nCurve < 64) n->lossCurve[n->nCurve++] = n->loss;
    }

    /* confusion counts at the operating threshold */
    int tp = 0, fp = 0, fn = 0, right = 0;
    for (int i = 0; i < BAG_TRAIN; i++) {
        float p = ai_bagnet_eval(n, X[i], NULL, NULL);
        int pred = p > BAG_THRESHOLD, act = (int)y[i];
        if (pred == act) right++;
        if (pred && act)  tp++;
        if (pred && !act) fp++;
        if (!pred && act) fn++;
    }
    n->accuracy  = (float)right / BAG_TRAIN;
    n->precision = (tp + fp) ? (float)tp / (tp + fp) : 0.f;
    n->recall    = (tp + fn) ? (float)tp / (tp + fn) : 0.f;
    n->epochs    = EPOCHS;
    n->trained   = 1;
}

/* ==========================================================================
 *  live scoring
 * ========================================================================== */

void ai_bag_features(World *w, Bag *b, float *out)
{
    /* Deterministic per-bag "scan": the same bag always produces the same
     * feature vector, so a controller re-running a screen sees the same
     * numbers.
     *
     * These have to be drawn from the same two populations the network was
     * trained on.  Sampling them from some other distribution -- for example
     * pushing every long-haul bag's route index high -- shifts the whole
     * live set into the region the network learnt to call a threat, and the
     * search bay fills up with false alarms. */
    uint32_t r = (uint32_t)b->id * 2654435761u ^ 0x5A17u;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;

    /* about one bag in twenty five genuinely presents as suspicious */
    int suspicious = (rnd_f(&r) < 0.040f);

    float f[BAG_IN];
    bag_draw_features(f, suspicious, &r);

    /* Two of the seven are then nudged by things we genuinely measure: the
     * belt scale, and the sector.  Kept as a gentle blend so they inform the
     * score without dragging the vector out of distribution. */
    Flight *fl = flight_by_id(w, b->flight);
    float ma = cv_clampf(fabsf(b->weight - 17.5f) / 16.f, 0.f, 1.f);
    f[5] = cv_clampf(f[5]*0.80f + ma*0.20f, 0.f, 1.f);

    if (fl) {
        float rr = cv_clampf((float)w->airport[fl->airport].flightMin / 800.f,
                             0.05f, 0.95f);
        f[6] = cv_clampf(f[6]*0.85f + rr*0.15f, 0.f, 1.f);
    }

    memcpy(out, f, sizeof f);
}

float ai_bag_score(World *w, Bag *b)
{
    if (!g_net.trained) ai_bagnet_train(&g_net);
    float x[BAG_IN];
    ai_bag_features(w, b, x);
    return ai_bagnet_eval(&g_net, x, NULL, NULL);
}
