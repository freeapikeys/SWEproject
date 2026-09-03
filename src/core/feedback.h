/* ==========================================================================
 *  AURA :: feedback.h  --  passenger reviews, and lost property
 *
 *  Reviews are the airport asking the only people who can answer whether it
 *  is any good. Each one is tied to a service rather than left as a general
 *  grumble, so a two-star average on security means something a controller
 *  can act on, and a passenger reading them can tell whether the complaint is
 *  about the thing they care about.
 *
 *  Lost property sits here too because it is the same shape of problem: an
 *  item, a person, a description, and a match to be made between them.
 * ========================================================================== */
#ifndef AURA_FEEDBACK_H
#define AURA_FEEDBACK_H

#include "model.h"

#define MAX_REVIEWS   96
#define MAX_LOST      64
#define REVIEW_TEXT  300

/* ---------------------------------------------------------------- reviews -- */

typedef enum {
    RV_OVERALL = 0, RV_CHECKIN, RV_SECURITY, RV_LOUNGE, RV_FASTTRACK,
    RV_BAGGAGE, RV_SHOPS, RV_TRANSPORT, RV_STAFF, RV_SERVICE_COUNT
} ReviewService;

typedef struct {
    int           id;
    int           stars;             /* 1..5                                */
    ReviewService service;
    char          author[42];
    char          text[REVIEW_TEXT];
    int           day, month, year;
    int           verified;          /* left by an account with a booking   */
    int           helpful;
    int           ownReview;         /* written by the signed-in account    */
    int           userSubmitted;     /* posted in the app (not a seed) -> saved */
} Review;

typedef struct {
    Review r[MAX_REVIEWS];
    int    n;
    int    nextId;
} ReviewBook;

void          rv_init      (ReviewBook *B, World *w);
int           rv_add       (ReviewBook *B, World *w, ReviewService s, int stars,
                            const char *author, const char *text, int verified,
                            int own);
float         rv_average   (const ReviewBook *B, ReviewService s);  /* 0 none */
int           rv_count     (const ReviewBook *B, ReviewService s);
int           rv_distribution(const ReviewBook *B, ReviewService s, int out[5]);
int           rv_filter    (const ReviewBook *B, ReviewService s, int *out,
                            int max);
const char   *rv_service_name(ReviewService s);
const char   *rv_service_blurb(ReviewService s);

/*  Persistence.  Only reviews posted in the app are written (the seeds live
 *  in the source), so a passenger's own review survives closing the app and
 *  a review posted by another account on the same machine shows up on reload.
 *  The file is data/reviews.csv, which is git-ignored.                      */
void          rv_save      (const ReviewBook *B, const char *path);
int           rv_load      (ReviewBook *B, World *w, const char *path);

/* --------------------------------------------------------- lost property -- */

typedef enum { LP_REPORTED = 0, LP_FOUND, LP_MATCHED, LP_RETURNED,
               LP_LP_COUNT } LostState;

typedef struct {
    int       id;
    LostState state;
    int       isBag;                 /* a checked bag rather than an item   */
    char      ref[12];               /* claim reference                     */
    char      what[60];
    char      where[40];
    char      owner[42];
    char      contact[72];
    int       paxId;                 /* 0 if reported by a walk-in          */
    int       bagIndex;              /* -1 unless it is a registered bag    */
    float     atMin;
    int       day, month;
} LostItem;

typedef struct {
    LostItem it[MAX_LOST];
    int      n;
    int      nextId;
    int      returned;
} LostBook;

void  lp_init   (LostBook *L, World *w);
int   lp_report (LostBook *L, World *w, const char *what, const char *where,
                 const char *owner, const char *contact, int paxId);
int   lp_found  (LostBook *L, World *w, const char *what, const char *where);
int   lp_match  (LostBook *L, World *w, int id);
int   lp_return (LostBook *L, World *w, int id);
int   lp_open   (const LostBook *L);
int   lp_find   (const LostBook *L, const char *query, int *out, int max);
const char *lp_state_name(LostState s);

/* Report a checked bag missing: links the claim to the bag register. */
int   lp_report_bag(LostBook *L, World *w, int bagIndex, const char *contact);

#endif /* AURA_FEEDBACK_H */
