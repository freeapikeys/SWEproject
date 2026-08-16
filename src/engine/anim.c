/* ==========================================================================
 *  AURA :: anim.c
 * ========================================================================== */

#include "anim.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float g_dt    = 1.f/60.f;
static float g_time  = 0.f;
static long  g_frame = 0;

/* ==========================================================================
 *  easing
 * ========================================================================== */

static float clamp01(float t) { return t < 0.f ? 0.f : (t > 1.f ? 1.f : t); }

float ease_lin      (float t) { return clamp01(t); }
float ease_in_quad  (float t) { t = clamp01(t); return t*t; }
float ease_out_quad (float t) { t = clamp01(t); return 1.f-(1.f-t)*(1.f-t); }
float ease_in_cubic (float t) { t = clamp01(t); return t*t*t; }

float ease_out_cubic(float t)
{
    t = clamp01(t); float u = 1.f - t; return 1.f - u*u*u;
}

float ease_out_quint(float t)
{
    t = clamp01(t); float u = 1.f - t; return 1.f - u*u*u*u*u;
}

float ease_in_out(float t)
{
    t = clamp01(t);
    return t < 0.5f ? 2.f*t*t : 1.f - 2.f*(1.f-t)*(1.f-t);
}

float ease_in_out_cubic(float t)
{
    t = clamp01(t);
    if (t < 0.5f) return 4.f*t*t*t;
    float u = -2.f*t + 2.f;
    return 1.f - u*u*u*0.5f;
}

float ease_out_back(float t)
{
    t = clamp01(t);
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    float u = t - 1.f;
    return 1.f + c3*u*u*u + c1*u*u;
}

float ease_out_elastic(float t)
{
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    const float c4 = (float)(2.0*M_PI/3.0);
    return powf(2.f, -10.f*t) * sinf((t*10.f - 0.75f)*c4) + 1.f;
}

float ease_out_bounce(float t)
{
    t = clamp01(t);
    const float n1 = 7.5625f, d1 = 2.75f;
    if (t < 1.f/d1)      return n1*t*t;
    else if (t < 2.f/d1) { t -= 1.5f/d1;   return n1*t*t + 0.75f; }
    else if (t < 2.5f/d1){ t -= 2.25f/d1;  return n1*t*t + 0.9375f; }
    t -= 2.625f/d1;      return n1*t*t + 0.984375f;
}

/* ==========================================================================
 *  frame clock
 * ========================================================================== */

void anim_begin_frame(float dt)
{
    if (dt < 0.f)     dt = 0.f;
    if (dt > 0.100f)  dt = 0.100f;      /* never let a stall teleport things */
    g_dt = dt;
    g_time += dt;
    g_frame++;
}

float anim_dt(void)       { return g_dt; }
float anim_time(void)     { return g_time; }
long  anim_frame_no(void) { return g_frame; }

/* ==========================================================================
 *  identifiers  (FNV-1a)
 * ========================================================================== */

static uint64_t fnv(const char *s, uint64_t h)
{
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

uint64_t uid(const char *tag) { return fnv(tag, 1469598103934665603ULL); }

uint64_t uidi(const char *tag, int i)
{
    uint64_t h = uid(tag);
    h ^= (uint64_t)(uint32_t)i; h *= 1099511628211ULL;
    return h;
}

uint64_t uidii(const char *tag, int i, int j)
{
    uint64_t h = uidi(tag, i);
    h ^= (uint64_t)(uint32_t)j; h *= 1099511628211ULL;
    return h;
}

/* ==========================================================================
 *  tween / spring table
 * ========================================================================== */

#define ASLOTS 4096

typedef struct {
    uint64_t id;
    float    v, vel;
    long     touched;
    int      used;
} ASlot;

static ASlot g_slots[ASLOTS];

static ASlot *slot_for(uint64_t id, int *fresh)
{
    uint32_t h = (uint32_t)(id ^ (id >> 32)) & (ASLOTS - 1);
    for (int probe = 0; probe < 64; probe++) {
        ASlot *s = &g_slots[(h + probe) & (ASLOTS - 1)];
        if (s->used && s->id == id) { *fresh = 0; s->touched = g_frame; return s; }
        if (!s->used) {
            s->used = 1; s->id = id; s->v = 0.f; s->vel = 0.f;
            s->touched = g_frame; *fresh = 1;
            return s;
        }
    }
    /* table pressure: evict the coldest of the probe window */
    ASlot *best = &g_slots[h];
    for (int probe = 1; probe < 64; probe++) {
        ASlot *s = &g_slots[(h + probe) & (ASLOTS - 1)];
        if (s->touched < best->touched) best = s;
    }
    best->id = id; best->v = 0.f; best->vel = 0.f;
    best->touched = g_frame; *fresh = 1;
    return best;
}

float anim_to(uint64_t id, float target, float speed)
{
    int fresh;
    ASlot *s = slot_for(id, &fresh);
    if (fresh) { s->v = target; return target; }
    /* frame-rate independent exponential approach */
    float k = 1.f - expf(-speed * g_dt);
    s->v += (target - s->v) * k;
    if (fabsf(target - s->v) < 0.0004f) s->v = target;
    return s->v;
}

float anim_spring(uint64_t id, float target, float stiffness, float damping)
{
    int fresh;
    ASlot *s = slot_for(id, &fresh);
    if (fresh) { s->v = target; s->vel = 0.f; return target; }

    /* sub-step so stiff springs stay stable at low frame rates */
    float remaining = g_dt;
    while (remaining > 0.f) {
        float h = remaining > 1.f/120.f ? 1.f/120.f : remaining;
        remaining -= h;
        float a = (target - s->v) * stiffness - s->vel * damping;
        s->vel += a * h;
        s->v   += s->vel * h;
    }
    if (fabsf(target - s->v) < 0.0004f && fabsf(s->vel) < 0.004f) {
        s->v = target; s->vel = 0.f;
    }
    return s->v;
}

float anim_get(uint64_t id)
{
    int fresh; ASlot *s = slot_for(id, &fresh);
    return s->v;
}

void anim_set(uint64_t id, float v)
{
    int fresh; ASlot *s = slot_for(id, &fresh);
    s->v = v; s->vel = 0.f;
}

float anim_once(uint64_t id, float dur)
{
    int fresh; ASlot *s = slot_for(id, &fresh);
    if (fresh) s->v = 0.f;
    if (dur <= 0.f) return 1.f;
    s->v += g_dt / dur;
    if (s->v > 1.f) s->v = 1.f;
    return s->v;
}

void anim_reset(uint64_t id)
{
    int fresh; ASlot *s = slot_for(id, &fresh);
    s->v = 0.f; s->vel = 0.f;
}

float tl_stage(float now, float start, float dur)
{
    if (dur <= 0.f) return now >= start ? 1.f : 0.f;
    return clamp01((now - start) / dur);
}

/* ==========================================================================
 *  noise
 * ========================================================================== */

float rnd_f(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (float)(x & 0x00FFFFFFu) / (float)0x01000000u;
}

float rnd_range(uint32_t *s, float a, float b) { return a + (b-a)*rnd_f(s); }

int rnd_int(uint32_t *s, int a, int b)
{
    if (b <= a) return a;
    return a + (int)(rnd_f(s) * (float)(b - a + 1)) % (b - a + 1);
}

static float hash1(int i)
{
    uint32_t x = (uint32_t)i * 374761393u + 668265263u;
    x = (x ^ (x >> 13)) * 1274126177u;
    return (float)((x ^ (x >> 16)) & 0xFFFFFF) / (float)0xFFFFFF;
}

static float hash2(int i, int j)
{
    uint32_t x = (uint32_t)i*374761393u + (uint32_t)j*668265263u;
    x = (x ^ (x >> 13)) * 1274126177u;
    return (float)((x ^ (x >> 16)) & 0xFFFFFF) / (float)0xFFFFFF;
}

static float smoothstep(float t) { return t*t*(3.f - 2.f*t); }

float noise1(float x)
{
    int i = (int)floorf(x);
    float f = smoothstep(x - (float)i);
    return hash1(i)*(1.f-f) + hash1(i+1)*f;
}

float noise2(float x, float y)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = smoothstep(x - (float)xi), fy = smoothstep(y - (float)yi);
    float a = hash2(xi,   yi  ), b = hash2(xi+1, yi  );
    float c = hash2(xi,   yi+1), d = hash2(xi+1, yi+1);
    float ab = a + (b-a)*fx, cd = c + (d-c)*fx;
    return ab + (cd-ab)*fy;
}

/* ==========================================================================
 *  particles
 * ========================================================================== */

void ps_init(ParticleSys *s, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->seed = seed ? seed : 0x1234567u;
}

void ps_clear(ParticleSys *s) { for (int i = 0; i < PARTICLE_MAX; i++) s->p[i].alive = 0; s->n = 0; }

Particle *ps_spawn(ParticleSys *s)
{
    for (int i = 0; i < PARTICLE_MAX; i++) {
        int k = (s->n + i) % PARTICLE_MAX;
        if (!s->p[k].alive) {
            s->n = (k + 1) % PARTICLE_MAX;
            memset(&s->p[k], 0, sizeof(Particle));
            s->p[k].alive = 1;
            s->p[k].size  = 3.f;
            s->p[k].maxLife = 1.f;
            s->p[k].life    = 1.f;
            return &s->p[k];
        }
    }
    return NULL;
}

void ps_update(ParticleSys *s, float dt, float gravity, float drag)
{
    float d = 1.f - drag*dt;
    if (d < 0.f) d = 0.f;
    for (int i = 0; i < PARTICLE_MAX; i++) {
        Particle *p = &s->p[i];
        if (!p->alive) continue;
        p->vy += gravity*dt;
        p->vx *= d; p->vy *= d;
        p->x  += p->vx*dt;
        p->y  += p->vy*dt;
        p->rot += p->vrot*dt;
        p->life -= dt;
        if (p->life <= 0.f) p->alive = 0;
    }
}
