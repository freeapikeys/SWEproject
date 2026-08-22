/* ==========================================================================
 *  AURA :: vision.h  --  terminal surveillance analytics
 *
 *  WHAT IS REAL HERE AND WHAT IS NOT
 *  ---------------------------------
 *  There is no camera plugged into this machine, and a pure C build with no
 *  third-party libraries cannot run a convolutional detector over real video.
 *  Pretending otherwise would be dishonest, so the split is drawn explicitly:
 *
 *    SIMULATED   the scene and the detector.  People walk the terminal under
 *                the simulation, and a synthetic detector emits boxes for
 *                them with positional jitter, missed detections and false
 *                positives at rates a real detector exhibits.
 *
 *    REAL        everything downstream.  The association, the state
 *                estimator, the track lifecycle, the cross-camera
 *                re-identification, the three anomaly detectors, the density
 *                accumulator and the flow field are all genuinely computed
 *                here and would run unchanged on real detections.
 *
 *  Because the ground truth identity of every person is known, the tracker
 *  can be scored rather than merely demonstrated: identity switches, false
 *  positives and misses are counted every frame and reported on screen.
 *
 *  PIPELINE
 *    scene -> detector -> gating -> greedy association -> alpha-beta filter
 *          -> track lifecycle -> cross-camera re-id -> anomaly rules
 *          -> density / flow accumulation -> telemetry
 * ========================================================================== */
#ifndef AURA_VISION_H
#define AURA_VISION_H

#include "../core/model.h"

/* ---------------------------------------------------------------- limits -- */

#define CV_CAMS        6
#define CV_AGENTS     12        /* people simultaneously inside one camera   */
#define CV_TRANSIT    48        /* people walking between camera coverages   */
#define CV_DETS       32
#define CV_TRACKS     40
#define CV_TRAIL      24
#define CV_GRID_X     20
#define CV_GRID_Y     12
#define CV_ALERTS     32
#define CV_GALLERY    96        /* cross-camera identity gallery             */
#define CV_APPEAR     12        /* appearance descriptor length              */

/* Camera frames are normalised to 0..1 in both axes, so the analytics do not
 * depend on the resolution the tile happens to be drawn at. */

/* ----------------------------------------------------------------- zones -- */

typedef enum {
    ZN_ARRIVALS = 0, ZN_IMMIGRATION, ZN_BAGGAGE, ZN_DUTYFREE,
    ZN_DEPARTURES, ZN_TARMAC, ZN_COUNT
} ZoneKind;

typedef struct {
    char     name[28];
    char     code[8];
    ZoneKind zone;
    float    flowDir;       /* designated direction of travel, radians       */
    int      oneWay;        /* counter-flow is an offence in this zone       */
    int      restricted;    /* loitering is an offence in this zone          */
    float    queueX;        /* x of the served position, for wait estimation */
    int      servers;       /* desks / e-gates in view                       */
} Camera;

/* ---------------------------------------------------------------- people -- */

typedef struct {
    int   live;
    int   gtId;             /* ground truth identity                          */
    float x, y, vx, vy;
    float tx, ty;           /* current waypoint                               */
    float appear[CV_APPEAR];/* clothing / build descriptor                    */
    float dwell;            /* seconds spent barely moving                    */
    int   staff;
    int   hasBag;
    int   bagDropped;
    float bagX, bagY;
    float bagTimer;
    int   counterFlow;      /* deliberately walking the wrong way             */
    float life;
} Agent;

/* ------------------------------------------------------------ detections -- */

typedef struct {
    float x, y, w, h;       /* normalised box centre and size                 */
    float conf;
    float appear[CV_APPEAR];
    int   gtId;             /* -1 for a false positive; used only for scoring */
    int   claimed;
} Detection;

/* ---------------------------------------------------------------- tracks -- */

typedef enum { TR_FREE = 0, TR_TENTATIVE, TR_CONFIRMED, TR_COASTING } TrackState;

typedef struct {
    TrackState state;
    int   localId;
    int   globalId;         /* stable across cameras once re-identified       */
    float x, y, vx, vy;     /* alpha-beta filter state                        */
    float w, h;
    float appear[CV_APPEAR];
    int   hits, misses, age;
    float conf;
    float trail[CV_TRAIL][2];
    int   nTrail, trailHead;

    /* behaviour accumulators */
    float dwell;            /* seconds nearly stationary                      */
    float against;          /* seconds moving against the designated flow     */
    int   flaggedLoiter, flaggedCounter;

    int   gtId;             /* ground truth of the detection last matched     */
    int   gtIdPrev;
} Track;

/* -------------------------------------------------------------- unattended -*/

typedef struct {
    int   live;
    float x, y;
    float still;            /* seconds without its owner nearby               */
    int   ownerGt;
    int   flagged;
} BagObject;

/* --------------------------------------------------------------- alerts --- */

typedef enum { AL_LOITER = 0, AL_UNATTENDED, AL_COUNTERFLOW, AL_DENSITY } AlertKind;

typedef struct {
    int       live;
    AlertKind kind;
    int       cam;
    int       globalId;
    float     x, y;
    float     age;
    char      text[92];
} CvAlert;

/* ------------------------------------------------------------- per camera - */

typedef struct {
    Agent     agent[CV_AGENTS];
    Detection det[CV_DETS];     int nDet;
    Track     track[CV_TRACKS];
    BagObject bag[8];

    float density[CV_GRID_Y][CV_GRID_X];
    float flowX  [CV_GRID_Y][CV_GRID_X];
    float flowY  [CV_GRID_Y][CV_GRID_X];

    int   nextLocalId;
    int   confirmed;            /* confirmed tracks this frame                */
    int   occupancy;            /* people believed present                    */
    float queueWait;            /* minutes, from the M/M/c model              */
    float peakDensity;
    int   idSwitches;
    int   falsePos, missed;
    uint32_t rng;
} CameraState;

/* ------------------------------------------------------------- the system - */

/* A person who has left one camera and is walking to the next.  Fields of
 * view do not overlap, so this gap is exactly what cross-camera
 * re-identification has to bridge. */
typedef struct {
    int   live;
    int   gtId;
    float appear[CV_APPEAR];
    float arriveAt;
    int   destCam;
} TransitPerson;

typedef struct {
    int    gid;
    int    gtId;            /* truth, kept only to score the re-identifier   */
    float  appear[CV_APPEAR];
    float  lastSeen;            /* seconds on the vision clock                */
    int    lastCam;
    int    live;
} GalleryEntry;

typedef struct {
    Camera       cam[CV_CAMS];
    CameraState  st [CV_CAMS];
    GalleryEntry gallery[CV_GALLERY];
    TransitPerson transit[CV_TRANSIT];
    CvAlert      alert[CV_ALERTS];

    int   nextGlobalId;
    float clock;                /* seconds since the pipeline started         */

    /* tuning, exposed on screen so the demonstrator can move it */
    float missRate;             /* probability a person is not detected       */
    float fpRate;               /* expected false positives per frame         */
    float jitter;               /* detector positional noise, normalised      */
    float alpha, beta;          /* alpha-beta filter gains                    */
    float gate;                 /* association gate, normalised distance      */
    float reidThreshold;        /* cosine similarity to accept a re-id        */

    /* rolling scoring against ground truth */
    int   totalIdSwitches, totalReids, totalFalsePos, totalMissed;
    int   reidCorrect, reidWrong;   /* re-id scored against ground truth   */
    int   handoffs;                 /* people who crossed between cameras  */
    int   totalDetections, totalMatched;
    float mota;                 /* multi object tracking accuracy             */
    float procMs;               /* time the last pipeline tick took           */
    float procHistory[64];
    int   nProc;

    int   ready;
} VisionSystem;

/* ------------------------------------------------------------------- api -- */

void  vision_init (VisionSystem *v, World *w);
void  vision_tick (VisionSystem *v, World *w, float dt);
void  vision_reset_metrics(VisionSystem *v);

const char *cv_zone_name(ZoneKind z);
const char *cv_alert_name(AlertKind k);
int   vision_alert_count(VisionSystem *v);
int   vision_total_tracked(VisionSystem *v);

#endif /* AURA_VISION_H */
