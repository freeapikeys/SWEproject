/* ==========================================================================
 *  AURA :: anim.h  --  motion system
 *
 *  Three kinds of motion live in this application:
 *
 *    tweens    a value chases a target with frame-rate independent smoothing
 *    springs   a value chases a target through a critically damped spring,
 *              which is what gives panels and cards their weight
 *    timelines a scripted 0..1 progress used for entrances and one-shots
 *
 *  Tweens and springs are addressed by a hashed identifier rather than being
 *  stored in the caller, so screens can stay stateless and still animate.
 * ========================================================================== */
#ifndef AURA_ANIM_H
#define AURA_ANIM_H

#include <stdint.h>

/* ---- easing ------------------------------------------------------------- */
float ease_lin       (float t);
float ease_in_quad   (float t);
float ease_out_quad  (float t);
float ease_in_out    (float t);
float ease_out_cubic (float t);
float ease_in_cubic  (float t);
float ease_out_quint (float t);
float ease_out_back  (float t);
float ease_out_elastic(float t);
float ease_out_bounce(float t);
float ease_in_out_cubic(float t);

/* ---- frame clock -------------------------------------------------------- */
void  anim_begin_frame(float dt);
float anim_dt(void);
float anim_time(void);          /* seconds since start                      */
long  anim_frame_no(void);

/* ---- identifiers -------------------------------------------------------- */
uint64_t uid  (const char *tag);
uint64_t uidi (const char *tag, int i);
uint64_t uidii(const char *tag, int i, int j);

/* ---- tweens / springs --------------------------------------------------- */
float anim_to    (uint64_t id, float target, float speed);
float anim_spring(uint64_t id, float target, float stiffness, float damping);
float anim_get   (uint64_t id);
void  anim_set   (uint64_t id, float v);
/* returns 0..1 progress that starts when first requested, then runs `dur` */
float anim_once  (uint64_t id, float dur);
void  anim_reset (uint64_t id);

/* ---- timelines ---------------------------------------------------------- */
float tl_stage(float now, float start, float dur);   /* clamped 0..1        */

/* ---- deterministic noise ------------------------------------------------ */
float rnd_f    (uint32_t *state);            /* 0..1                        */
float rnd_range(uint32_t *state, float a, float b);
int   rnd_int  (uint32_t *state, int a, int b);
float noise1   (float x);                    /* smooth value noise          */
float noise2   (float x, float y);

/* ---- particle pool ------------------------------------------------------ */
#define PARTICLE_MAX 900

typedef struct {
    float x, y, vx, vy;
    float life, maxLife;
    float size, rot, vrot;
    uint32_t col;
    int   kind;
    int   alive;
} Particle;

typedef struct {
    Particle p[PARTICLE_MAX];
    int      n;
    uint32_t seed;
} ParticleSys;

void ps_init  (ParticleSys *s, uint32_t seed);
void ps_clear (ParticleSys *s);
Particle *ps_spawn(ParticleSys *s);
void ps_update(ParticleSys *s, float dt, float gravity, float drag);

#endif /* AURA_ANIM_H */
