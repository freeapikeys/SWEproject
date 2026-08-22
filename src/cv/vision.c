/* ==========================================================================
 *  AURA :: vision.c  --  the surveillance analytics pipeline
 * ========================================================================== */

#include "vision.h"
#include "../core/sim.h"
#include "../ai/ai.h"
#include "../engine/anim.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==========================================================================
 *  cameras
 * ========================================================================== */

static const struct {
    const char *name, *code; int zone; float dir; int oneWay, restricted;
    float queueX; int servers;
} CAMDEF[CV_CAMS] = {
    { "Arrivals Hall",      "CAM-01", ZN_ARRIVALS,    0.00f, 0, 0, 0.00f, 0 },
    { "Immigration",        "CAM-02", ZN_IMMIGRATION, 4.71f, 1, 0, 0.14f, 8 },
    { "Baggage Reclaim",    "CAM-03", ZN_BAGGAGE,     0.00f, 0, 0, 0.00f, 0 },
    { "Duty Free",          "CAM-04", ZN_DUTYFREE,    0.00f, 0, 0, 0.00f, 0 },
    { "Departures Search",  "CAM-05", ZN_DEPARTURES,  4.71f, 1, 0, 0.16f, 6 },
    { "Tarmac Gate A3",     "CAM-06", ZN_TARMAC,      0.00f, 0, 1, 0.00f, 0 },
};

const char *cv_zone_name(ZoneKind z)
{
    switch (z) {
    case ZN_ARRIVALS:    return "Arrivals";
    case ZN_IMMIGRATION: return "Immigration";
    case ZN_BAGGAGE:     return "Baggage reclaim";
    case ZN_DUTYFREE:    return "Retail";
    case ZN_DEPARTURES:  return "Central search";
    default:             return "Restricted apron";
    }
}

const char *cv_alert_name(AlertKind k)
{
    switch (k) {
    case AL_LOITER:      return "Loitering";
    case AL_UNATTENDED:  return "Unattended baggage";
    case AL_COUNTERFLOW: return "Counter-flow";
    default:             return "Crowding";
    }
}

/* ==========================================================================
 *  small maths
 * ========================================================================== */

static float clamp01f(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

static float gauss(uint32_t *rng, float sd)
{
    /* Box-Muller, one half kept */
    float u1 = rnd_f(rng); if (u1 < 1e-6f) u1 = 1e-6f;
    float u2 = rnd_f(rng);
    return sqrtf(-2.f*logf(u1)) * cosf(2.f*(float)M_PI*u2) * sd;
}

static float cosine_sim(const float *a, const float *b)
{
    float dot = 0.f, na = 0.f, nb = 0.f;
    for (int i = 0; i < CV_APPEAR; i++) {
        dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i];
    }
    if (na <= 1e-9f || nb <= 1e-9f) return 0.f;
    return dot / (sqrtf(na)*sqrtf(nb));
}

static float box_iou(float ax,float ay,float aw,float ah,
                     float bx,float by,float bw,float bh)
{
    float a0x = ax-aw*0.5f, a1x = ax+aw*0.5f, a0y = ay-ah*0.5f, a1y = ay+ah*0.5f;
    float b0x = bx-bw*0.5f, b1x = bx+bw*0.5f, b0y = by-bh*0.5f, b1y = by+bh*0.5f;
    float ix = fminf(a1x,b1x) - fmaxf(a0x,b0x);
    float iy = fminf(a1y,b1y) - fmaxf(a0y,b0y);
    if (ix <= 0.f || iy <= 0.f) return 0.f;
    float inter = ix*iy;
    float uni = aw*ah + bw*bh - inter;
    return uni > 1e-9f ? inter/uni : 0.f;
}

/* ==========================================================================
 *  scene: people walking the terminal
 * ========================================================================== */

static void agent_spawn(VisionSystem *v, CameraState *s, Camera *c, Agent *a,
                        int fresh)
{
    uint32_t *r = &s->rng;
    a->live  = 1;
    a->gtId  = fresh ? v->nextGlobalId++ : a->gtId;
    a->staff = rnd_f(r) < 0.12f;
    a->hasBag = (c->zone == ZN_ARRIVALS || c->zone == ZN_BAGGAGE ||
                 c->zone == ZN_DEPARTURES) ? (rnd_f(r) < 0.55f) : (rnd_f(r) < 0.2f);
    a->bagDropped = 0;
    a->bagTimer = 0.f;
    a->dwell = 0.f;
    a->life = rnd_range(r, 45.f, 150.f);
    a->counterFlow = (c->oneWay && rnd_f(r) < 0.06f);

    /* Centred on zero.  All-positive descriptors give two unrelated people a
     * cosine similarity around 0.75 purely from sharing one quadrant, which
     * leaves the re-identifier nothing to discriminate with. */
    for (int i = 0; i < CV_APPEAR; i++) a->appear[i] = rnd_range(r, -1.f, 1.f);

    if (c->oneWay) {
        /* one way zones: enter at the bottom, leave at the top */
        a->x = rnd_range(r, 0.08f, 0.92f);
        a->y = a->counterFlow ? rnd_range(r, 0.15f, 0.35f) : 1.05f;
        a->tx = rnd_range(r, 0.10f, 0.90f);
        a->ty = a->counterFlow ? 1.32f : -0.32f;
    } else {
        int side = rnd_int(r, 0, 3);
        if (side == 0)      { a->x = -0.05f; a->y = rnd_f(r); }
        else if (side == 1) { a->x =  1.05f; a->y = rnd_f(r); }
        else if (side == 2) { a->x = rnd_f(r); a->y = -0.05f; }
        else                { a->x = rnd_f(r); a->y =  1.05f; }
        a->tx = rnd_range(r, 0.12f, 0.88f);
        a->ty = rnd_range(r, 0.12f, 0.88f);
    }
    a->vx = a->vy = 0.f;
}

static void scene_update(VisionSystem *v, int ci, float dt)
{
    CameraState *s = &v->st[ci];
    Camera *c = &v->cam[ci];
    uint32_t *r = &s->rng;

    for (int i = 0; i < CV_AGENTS; i++) {
        Agent *a = &s->agent[i];
        if (!a->live) {
            if (rnd_f(r) < dt * 0.22f) agent_spawn(v, s, c, a, 1);
            continue;
        }

        a->life -= dt;

        /* steer toward the waypoint */
        float dx = a->tx - a->x, dy = a->ty - a->y;
        float d  = sqrtf(dx*dx + dy*dy);
        float speed = a->staff ? 0.075f : 0.045f;
        if (c->zone == ZN_IMMIGRATION || c->zone == ZN_DEPARTURES) speed *= 0.55f;

        if (d < 0.05f) {
            /* reached it: dwell a moment, then pick another */
            a->dwell += dt;
            if (a->dwell > (c->restricted ? 9.f : 4.5f) || rnd_f(r) < dt*0.6f) {
                a->dwell = 0.f;
                if (c->oneWay) {
                    a->tx = rnd_range(r, 0.10f, 0.90f);
                    a->ty = a->counterFlow ? 1.32f : -0.32f;
                } else if (rnd_f(r) < 0.45f) {
                    /* Head for an exit.  Without this people wander inside one
                     * frame until they expire, nobody ever crosses between
                     * cameras, and the re-identifier has nothing to match. */
                    int side = rnd_int(r, 0, 3);
                    if      (side == 0) { a->tx = -0.32f;    a->ty = rnd_f(r); }
                    else if (side == 1) { a->tx =  1.32f;    a->ty = rnd_f(r); }
                    else if (side == 2) { a->tx = rnd_f(r);  a->ty = -0.32f;   }
                    else                { a->tx = rnd_f(r);  a->ty =  1.32f;   }
                } else {
                    a->tx = rnd_range(r, 0.05f, 0.95f);
                    a->ty = rnd_range(r, 0.05f, 0.95f);
                }
            }
            a->vx *= 0.85f; a->vy *= 0.85f;
        } else {
            a->vx = dx/d * speed;
            a->vy = dy/d * speed;
            a->dwell = 0.f;
        }

        a->x += a->vx * dt;
        a->y += a->vy * dt;

        /* a few people put a bag down and walk off */
        if (a->hasBag && !a->bagDropped && rnd_f(r) < dt * 0.006f) {
            a->bagDropped = 1;
            a->bagX = a->x; a->bagY = a->y;
            for (int b = 0; b < 8; b++) {
                if (s->bag[b].live) continue;
                s->bag[b].live = 1;
                s->bag[b].x = a->x; s->bag[b].y = a->y;
                s->bag[b].still = 0.f;
                s->bag[b].ownerGt = a->gtId;
                s->bag[b].flagged = 0;
                break;
            }
        }

        if (a->life <= 0.f || a->x < -0.15f || a->x > 1.15f ||
            a->y < -0.15f || a->y > 1.15f) {
            int walkedOut = (a->life > 0.f);
            a->live = 0;
            /* Most people who walk out of one camera walk into another a few
             * seconds later.  Carrying the same identity and appearance over
             * is what gives the re-identifier something real to solve. */
            if (walkedOut && rnd_f(r) < 0.62f) {
                for (int k = 0; k < CV_TRANSIT; k++) {
                    if (v->transit[k].live) continue;
                    TransitPerson *tp = &v->transit[k];
                    tp->live = 1;
                    tp->gtId = a->gtId;
                    memcpy(tp->appear, a->appear, sizeof tp->appear);
                    tp->arriveAt = v->clock + rnd_range(r, 3.5f, 22.f);
                    do { tp->destCam = rnd_int(r, 0, CV_CAMS-1); }
                    while (tp->destCam == ci);
                    v->handoffs++;
                    break;
                }
            }
        }
    }

    /* unattended baggage: a bag whose owner has left the area */
    for (int b = 0; b < 8; b++) {
        BagObject *g = &s->bag[b];
        if (!g->live) continue;
        float nearest = 9.f;
        for (int i = 0; i < CV_AGENTS; i++) {
            Agent *a = &s->agent[i];
            if (!a->live || a->gtId != g->ownerGt) continue;
            float dx = a->x - g->x, dy = a->y - g->y;
            nearest = sqrtf(dx*dx + dy*dy);
            break;
        }
        if (nearest > 0.18f) g->still += dt;
        else                 g->still = 0.f;
        if (g->still > 90.f) g->live = 0;   /* collected by an operative */
    }

    /* people arriving from another camera keep their identity */
    for (int k = 0; k < CV_TRANSIT; k++) {
        TransitPerson *tp = &v->transit[k];
        if (!tp->live || tp->destCam != ci || v->clock < tp->arriveAt) continue;
        for (int i = 0; i < CV_AGENTS; i++) {
            Agent *a = &s->agent[i];
            if (a->live) continue;
            agent_spawn(v, s, c, a, 0);
            a->gtId = tp->gtId;
            memcpy(a->appear, tp->appear, sizeof a->appear);
            tp->live = 0;
            break;
        }
    }
}

/* ==========================================================================
 *  the synthetic detector
 *
 *  Emits boxes with the failure modes a real detector has: positional noise,
 *  missed detections, and occasional false positives.  Nothing downstream is
 *  told which is which -- the gtId travels alongside purely so the tracker can
 *  be scored afterwards.
 * ========================================================================== */

static void detect(VisionSystem *v, int ci)
{
    CameraState *s = &v->st[ci];
    uint32_t *r = &s->rng;
    s->nDet = 0;

    for (int i = 0; i < CV_AGENTS && s->nDet < CV_DETS; i++) {
        Agent *a = &s->agent[i];
        if (!a->live) continue;
        if (a->x < 0.f || a->x > 1.f || a->y < 0.f || a->y > 1.f) continue;
        if (rnd_f(r) < v->missRate) { s->missed++; v->totalMissed++; continue; }

        Detection *d = &s->det[s->nDet++];
        d->x = clamp01f(a->x + gauss(r, v->jitter));
        d->y = clamp01f(a->y + gauss(r, v->jitter));
        d->w = 0.036f + gauss(r, 0.004f);
        d->h = 0.085f + gauss(r, 0.006f);
        if (d->w < 0.012f) d->w = 0.012f;
        if (d->h < 0.02f)  d->h = 0.02f;
        d->conf = clamp01f(0.72f + gauss(r, 0.11f));
        d->gtId = a->gtId;
        d->claimed = 0;
        for (int k = 0; k < CV_APPEAR; k++)
            d->appear[k] = a->appear[k] + gauss(r, 0.06f);
        v->totalDetections++;
    }

    /* false positives: reflections, trolleys, signage */
    while (s->nDet < CV_DETS && rnd_f(r) < v->fpRate) {
        Detection *d = &s->det[s->nDet++];
        d->x = rnd_f(r); d->y = rnd_f(r);
        d->w = 0.030f + rnd_f(r)*0.02f;
        d->h = 0.070f + rnd_f(r)*0.03f;
        d->conf = clamp01f(0.45f + rnd_f(r)*0.2f);
        d->gtId = -1;
        d->claimed = 0;
        for (int k = 0; k < CV_APPEAR; k++) d->appear[k] = rnd_range(r, -1.f, 1.f);
        s->falsePos++; v->totalFalsePos++; v->totalDetections++;
    }
}

/* ==========================================================================
 *  association and the alpha-beta estimator
 *
 *  Cost blends geometry with appearance.  Boxes that do not overlap and are
 *  far apart are gated out entirely, which is what stops a track jumping onto
 *  an unrelated person walking past.
 * ========================================================================== */

static float assoc_cost(VisionSystem *v, Track *t, Detection *d)
{
    float dx = t->x - d->x, dy = t->y - d->y;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist > v->gate) return 1e9f;

    float iou  = box_iou(t->x,t->y,t->w,t->h, d->x,d->y,d->w,d->h);
    float app  = 1.f - cosine_sim(t->appear, d->appear);
    /* geometry dominates within the gate; appearance breaks the ties */
    return dist*2.2f + (1.f - iou)*0.9f + app*0.7f;
}

static void track_push_trail(Track *t)
{
    t->trail[t->trailHead][0] = t->x;
    t->trail[t->trailHead][1] = t->y;
    t->trailHead = (t->trailHead + 1) % CV_TRAIL;
    if (t->nTrail < CV_TRAIL) t->nTrail++;
}

static void associate(VisionSystem *v, int ci, float dt)
{
    CameraState *s = &v->st[ci];

    /* ---- predict --------------------------------------------------------- */
    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state == TR_FREE) continue;
        t->x += t->vx * dt;
        t->y += t->vy * dt;
        t->age++;
    }

    /* ---- greedy assignment on the cost matrix ---------------------------- */
    /* Repeatedly take the globally cheapest surviving pair.  With at most 40
     * tracks and 32 detections this is trivially cheap and, unlike a naive
     * per-track nearest neighbour, it cannot have two tracks claim the same
     * detection. */
    for (;;) {
        float best = 1e8f;
        int bt = -1, bd = -1;
        for (int i = 0; i < CV_TRACKS; i++) {
            Track *t = &s->track[i];
            if (t->state == TR_FREE || t->hits < 0) continue;
            if (t->misses == -1) continue;              /* already matched    */
            for (int j = 0; j < s->nDet; j++) {
                if (s->det[j].claimed) continue;
                float c = assoc_cost(v, t, &s->det[j]);
                if (c < best) { best = c; bt = i; bd = j; }
            }
        }
        if (bt < 0 || best > 1e7f) break;

        Track *t = &s->track[bt];
        Detection *d = &s->det[bd];
        d->claimed = 1;

        /* ---- alpha-beta update ------------------------------------------ */
        float rx = d->x - t->x, ry = d->y - t->y;
        t->x  += v->alpha * rx;
        t->y  += v->alpha * ry;
        if (dt > 1e-4f) {
            t->vx += (v->beta / dt) * rx;
            t->vy += (v->beta / dt) * ry;
        }
        float sp = sqrtf(t->vx*t->vx + t->vy*t->vy);
        if (sp > 0.35f) { t->vx *= 0.35f/sp; t->vy *= 0.35f/sp; }

        t->w = t->w*0.7f + d->w*0.3f;
        t->h = t->h*0.7f + d->h*0.3f;
        t->conf = t->conf*0.6f + d->conf*0.4f;
        for (int k = 0; k < CV_APPEAR; k++)
            t->appear[k] = t->appear[k]*0.88f + d->appear[k]*0.12f;

        /* identity switch scoring, against ground truth */
        if (t->gtId >= 0 && d->gtId >= 0 && t->gtId != d->gtId) {
            s->idSwitches++; v->totalIdSwitches++;
        }
        t->gtIdPrev = t->gtId;
        t->gtId = d->gtId;

        t->hits++;
        t->misses = -1;                                  /* matched marker    */
        v->totalMatched++;
        track_push_trail(t);
    }

    /* ---- lifecycle ------------------------------------------------------- */
    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state == TR_FREE) continue;
        if (t->misses == -1) {
            t->misses = 0;
            if (t->state == TR_TENTATIVE && t->hits >= 3) t->state = TR_CONFIRMED;
            else if (t->state == TR_COASTING)             t->state = TR_CONFIRMED;
        } else {
            t->misses++;
            if (t->state == TR_CONFIRMED && t->misses >= 2) t->state = TR_COASTING;
            if ((t->state == TR_TENTATIVE && t->misses >= 2) ||
                 t->misses >= 14 ||
                 t->x < -0.2f || t->x > 1.2f || t->y < -0.2f || t->y > 1.2f) {
                memset(t, 0, sizeof *t);
                t->state = TR_FREE;
            }
        }
    }

    /* ---- births ---------------------------------------------------------- */
    for (int j = 0; j < s->nDet; j++) {
        Detection *d = &s->det[j];
        if (d->claimed || d->conf < 0.5f) continue;
        for (int i = 0; i < CV_TRACKS; i++) {
            Track *t = &s->track[i];
            if (t->state != TR_FREE) continue;
            memset(t, 0, sizeof *t);
            t->state = TR_TENTATIVE;
            t->localId = ++s->nextLocalId;
            t->globalId = -1;
            t->x = d->x; t->y = d->y;
            t->w = d->w; t->h = d->h;
            t->vx = t->vy = 0.f;
            t->conf = d->conf;
            t->hits = 1; t->misses = 0;
            t->gtId = d->gtId; t->gtIdPrev = d->gtId;
            memcpy(t->appear, d->appear, sizeof t->appear);
            track_push_trail(t);
            break;
        }
    }
}

/* ==========================================================================
 *  cross camera re-identification
 *
 *  Fields of view do not overlap, so a person leaving CAM-01 reappears in
 *  CAM-02 some seconds later with no positional continuity at all.  The only
 *  thing that carries across is appearance, so a confirmed track without a
 *  global identity is matched against a gallery of recently seen descriptors
 *  from other cameras by cosine similarity.
 * ========================================================================== */

static void reidentify(VisionSystem *v, int ci)
{
    CameraState *s = &v->st[ci];

    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state != TR_CONFIRMED || t->globalId >= 0) continue;

        int   best = -1;
        float bestSim = v->reidThreshold;

        for (int g = 0; g < CV_GALLERY; g++) {
            GalleryEntry *e = &v->gallery[g];
            if (!e->live) continue;
            if (e->lastCam == ci) continue;                   /* another camera */
            float dtSeen = v->clock - e->lastSeen;
            if (dtSeen < 1.5f || dtSeen > 75.f) continue;     /* plausible gap  */
            float sim = cosine_sim(t->appear, e->appear);
            if (sim > bestSim) { bestSim = sim; best = g; }
        }

        if (best >= 0) {
            /* scored against truth: did it re-attach the right person? */
            if (t->gtId >= 0 && v->gallery[best].gtId >= 0) {
                if (v->gallery[best].gtId == t->gtId) v->reidCorrect++;
                else                                  v->reidWrong++;
            }
            t->globalId = v->gallery[best].gid;
            v->gallery[best].lastSeen = v->clock;
            v->gallery[best].lastCam  = ci;
            for (int k = 0; k < CV_APPEAR; k++)
                v->gallery[best].appear[k] =
                    v->gallery[best].appear[k]*0.7f + t->appear[k]*0.3f;
            v->totalReids++;
        } else {
            /* new identity: take a free slot, retiring anything stale first */
            for (int g = 0; g < CV_GALLERY; g++)
                if (v->gallery[g].live && v->clock - v->gallery[g].lastSeen > 90.f)
                    v->gallery[g].live = 0;
            int slot = -1;
            for (int g = 0; g < CV_GALLERY; g++)
                if (!v->gallery[g].live) { slot = g; break; }
            if (slot < 0) {
                float oldest = 1e9f;
                for (int g = 0; g < CV_GALLERY; g++)
                    if (v->gallery[g].lastSeen < oldest)
                        { oldest = v->gallery[g].lastSeen; slot = g; }
            }
            GalleryEntry *e = &v->gallery[slot];
            e->live = 1;
            e->gid = v->nextGlobalId++;
            e->lastSeen = v->clock;
            e->lastCam = ci;
            e->gtId = t->gtId;
            memcpy(e->appear, t->appear, sizeof e->appear);
            t->globalId = e->gid;
        }
    }

    /* keep the gallery fresh for tracks still on screen */
    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state != TR_CONFIRMED || t->globalId < 0) continue;
        for (int g = 0; g < CV_GALLERY; g++)
            if (v->gallery[g].live && v->gallery[g].gid == t->globalId) {
                v->gallery[g].lastSeen = v->clock;
                v->gallery[g].lastCam  = ci;
                break;
            }
    }
}

/* ==========================================================================
 *  alerts
 * ========================================================================== */

static void raise_alert(VisionSystem *v, AlertKind k, int cam, int gid,
                        float x, float y, const char *text)
{
    /* do not stack duplicates for the same subject and rule */
    for (int i = 0; i < CV_ALERTS; i++) {
        CvAlert *a = &v->alert[i];
        if (a->live && a->kind == k && a->cam == cam && a->globalId == gid)
            { a->age = 0.f; return; }
    }
    int slot = -1;
    for (int i = 0; i < CV_ALERTS; i++) if (!v->alert[i].live) { slot = i; break; }
    if (slot < 0) {
        float oldest = -1.f;
        for (int i = 0; i < CV_ALERTS; i++)
            if (v->alert[i].age > oldest) { oldest = v->alert[i].age; slot = i; }
    }
    CvAlert *a = &v->alert[slot];
    a->live = 1; a->kind = k; a->cam = cam; a->globalId = gid;
    a->x = x; a->y = y; a->age = 0.f;
    snprintf(a->text, sizeof a->text, "%s", text);
}

static void behaviour(VisionSystem *v, int ci, float dt)
{
    CameraState *s = &v->st[ci];
    Camera *c = &v->cam[ci];
    char msg[92];

    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state != TR_CONFIRMED) continue;

        float sp = sqrtf(t->vx*t->vx + t->vy*t->vy);

        /* --- loitering: stationary inside a restricted zone --------------- */
        if (sp < 0.012f) t->dwell += dt; else t->dwell *= 0.90f;
        if (c->restricted && t->dwell > 6.f && !t->flaggedLoiter) {
            t->flaggedLoiter = 1;
            snprintf(msg, sizeof msg, "ID %d stationary %.0fs in %s",
                     t->globalId, t->dwell, cv_zone_name(c->zone));
            raise_alert(v, AL_LOITER, ci, t->globalId, t->x, t->y, msg);
        }

        /* --- counter-flow: sustained travel against the one-way lane ------ */
        if (c->oneWay && sp > 0.015f) {
            float dir = atan2f(t->vy, t->vx);
            float diff = dir - c->flowDir;
            while (diff >  (float)M_PI) diff -= 2.f*(float)M_PI;
            while (diff < -(float)M_PI) diff += 2.f*(float)M_PI;
            if (fabsf(diff) > 2.09f) t->against += dt;      /* beyond 120 deg */
            else                     t->against *= 0.85f;
        } else {
            t->against *= 0.9f;
        }
        if (t->against > 2.2f && !t->flaggedCounter) {
            t->flaggedCounter = 1;
            snprintf(msg, sizeof msg, "ID %d against one-way flow in %s",
                     t->globalId, cv_zone_name(c->zone));
            raise_alert(v, AL_COUNTERFLOW, ci, t->globalId, t->x, t->y, msg);
        }
    }

    /* --- unattended baggage ---------------------------------------------- */
    for (int b = 0; b < 8; b++) {
        BagObject *g = &s->bag[b];
        if (!g->live || g->flagged) continue;
        if (g->still > 12.f) {
            g->flagged = 1;
            snprintf(msg, sizeof msg, "Item unattended %.0fs in %s",
                     g->still, cv_zone_name(c->zone));
            raise_alert(v, AL_UNATTENDED, ci, -1, g->x, g->y, msg);
        }
    }

    /* --- crowding -------------------------------------------------------- */
    if (s->peakDensity > 0.93f) {
        snprintf(msg, sizeof msg, "Density %.0f%% of capacity in %s",
                 s->peakDensity*100.f, cv_zone_name(c->zone));
        raise_alert(v, AL_DENSITY, ci, -2, 0.5f, 0.5f, msg);
    }
}

/* ==========================================================================
 *  density and flow accumulation
 * ========================================================================== */

static void accumulate(VisionSystem *v, int ci, float dt)
{
    CameraState *s = &v->st[ci];
    float decay = 1.f - dt * 1.1f;
    if (decay < 0.f) decay = 0.f;

    for (int y = 0; y < CV_GRID_Y; y++)
        for (int x = 0; x < CV_GRID_X; x++) {
            s->density[y][x] *= decay;
            s->flowX[y][x]   *= decay;
            s->flowY[y][x]   *= decay;
        }

    int conf = 0;
    for (int i = 0; i < CV_TRACKS; i++) {
        Track *t = &s->track[i];
        if (t->state != TR_CONFIRMED && t->state != TR_COASTING) continue;
        conf++;
        int gx = (int)(clamp01f(t->x) * (CV_GRID_X - 1));
        int gy = (int)(clamp01f(t->y) * (CV_GRID_Y - 1));
        s->density[gy][gx] += dt * 0.55f;
        s->flowX[gy][gx]   += t->vx * dt * 8.f;
        s->flowY[gy][gx]   += t->vy * dt * 8.f;
    }
    s->confirmed = conf;
    s->occupancy = conf;

    float peak = 0.f;
    for (int y = 0; y < CV_GRID_Y; y++)
        for (int x = 0; x < CV_GRID_X; x++)
            if (s->density[y][x] > peak) peak = s->density[y][x];
    s->peakDensity = peak > 1.f ? 1.f : peak;

    /* queue wait, from the same M/M/c model the flow screen uses */
    Camera *c = &v->cam[ci];
    if (c->servers > 0) {
        float lambda = (float)conf / 3.0f;      /* arrivals per minute        */
        s->queueWait = ai_flow_queue_wait(lambda, 1.9f, c->servers);
    } else {
        s->queueWait = 0.f;
    }
}

/* ==========================================================================
 *  public
 * ========================================================================== */

void vision_reset_metrics(VisionSystem *v)
{
    v->totalIdSwitches = v->totalReids = v->totalFalsePos = 0;
    v->totalMissed = v->totalDetections = v->totalMatched = 0;
    v->reidCorrect = v->reidWrong = v->handoffs = 0;
    for (int i = 0; i < CV_CAMS; i++) {
        v->st[i].idSwitches = v->st[i].falsePos = v->st[i].missed = 0;
    }
}

void vision_init(VisionSystem *v, World *w)
{
    memset(v, 0, sizeof *v);

    for (int i = 0; i < CV_CAMS; i++) {
        Camera *c = &v->cam[i];
        snprintf(c->name, sizeof c->name, "%s", CAMDEF[i].name);
        snprintf(c->code, sizeof c->code, "%s", CAMDEF[i].code);
        c->zone       = (ZoneKind)CAMDEF[i].zone;
        c->flowDir    = CAMDEF[i].dir;
        c->oneWay     = CAMDEF[i].oneWay;
        c->restricted = CAMDEF[i].restricted;
        c->queueX     = CAMDEF[i].queueX;
        c->servers    = CAMDEF[i].servers;
        v->st[i].rng  = 0x5EED0000u ^ (uint32_t)(i * 2654435761u);
        v->st[i].nextLocalId = 0;
    }

    v->nextGlobalId  = 1;
    /* Defaults describe a current-generation detector rather than an old
     * one: fewer dropped people, fewer spurious boxes, tighter boxes.  The
     * sliders still take it back to a worse detector on demand. */
    v->missRate      = 0.040f;
    v->fpRate        = 0.030f;
    v->jitter        = 0.005f;
    v->alpha         = 0.55f;
    v->beta          = 0.028f;
    v->gate          = 0.085f;
    v->reidThreshold = 0.90f;
    v->clock         = 0.f;

    /* let the scene fill before the operator looks at it */
    for (int i = 0; i < CV_CAMS; i++)
        for (int k = 0; k < 40; k++) scene_update(v, i, 0.25f);

    vision_reset_metrics(v);
    v->ready = 1;

    /* Run the pipeline forward half a minute so the screen opens on a system
     * that has already been working: tracks established, the gallery
     * populated, and people who crossed between cameras already re-identified.
     * Opening on empty counters reads as a broken pipeline rather than a new
     * one. */
    for (int k = 0; k < 900; k++) vision_tick(v, w, 1.f/30.f);
    v->totalMissed = v->totalFalsePos = 0;   /* score only what is watched   */
    v->totalDetections = v->totalMatched = 0;
    v->totalIdSwitches = 0;
    (void)w;
}

void vision_tick(VisionSystem *v, World *w, float dt)
{
    if (!v->ready) return;
    if (dt > 0.1f) dt = 0.1f;          /* never let a stall teleport people   */

    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);

    v->clock += dt;

    for (int i = 0; i < CV_CAMS; i++) {
        scene_update(v, i, dt);
        detect(v, i);
        associate(v, i, dt);
        reidentify(v, i);
        accumulate(v, i, dt);
        behaviour(v, i, dt);
    }

    /* age the alert list */
    for (int i = 0; i < CV_ALERTS; i++) {
        CvAlert *a = &v->alert[i];
        if (!a->live) continue;
        a->age += dt;
        if (a->age > 26.f) a->live = 0;
    }

    /* MOTA = 1 - (misses + false positives + id switches) / ground truth */
    int gt = v->totalDetections - v->totalFalsePos + v->totalMissed;
    if (gt > 0) {
        float err = (float)(v->totalMissed + v->totalFalsePos + v->totalIdSwitches);
        v->mota = 1.f - err / (float)gt;
        if (v->mota < -1.f) v->mota = -1.f;
    }

    QueryPerformanceCounter(&t1);
    v->procMs = (float)(t1.QuadPart - t0.QuadPart) * 1000.f / (float)f.QuadPart;
    v->procHistory[v->nProc % 64] = v->procMs;
    v->nProc++;
    (void)w;
}

int vision_alert_count(VisionSystem *v)
{
    int n = 0;
    for (int i = 0; i < CV_ALERTS; i++) if (v->alert[i].live) n++;
    return n;
}

int vision_total_tracked(VisionSystem *v)
{
    int n = 0;
    for (int i = 0; i < CV_CAMS; i++) n += v->st[i].confirmed;
    return n;
}
