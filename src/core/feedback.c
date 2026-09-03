/* ==========================================================================
 *  AURA :: feedback.c  --  passenger reviews, and lost property
 * ========================================================================== */

#include "feedback.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==========================================================================
 *  reviews
 * ========================================================================== */

const char *rv_service_name(ReviewService s)
{
    switch (s) {
    case RV_OVERALL:   return "Overall";
    case RV_CHECKIN:   return "Check-in";
    case RV_SECURITY:  return "Security";
    case RV_LOUNGE:    return "Lounge";
    case RV_FASTTRACK: return "Fast track";
    case RV_BAGGAGE:   return "Baggage";
    case RV_SHOPS:     return "Shops";
    case RV_TRANSPORT: return "Transport";
    default:           return "Staff";
    }
}

const char *rv_service_blurb(ReviewService s)
{
    switch (s) {
    case RV_OVERALL:   return "the airport as a whole";
    case RV_CHECKIN:   return "desks, bag drop and the departures hall";
    case RV_SECURITY:  return "screening lanes and the wait to get through";
    case RV_LOUNGE:    return "the departures lounge and its facilities";
    case RV_FASTTRACK: return "the priority lane and meet-and-greet";
    case RV_BAGGAGE:   return "reclaim belts and how bags arrived";
    case RV_SHOPS:     return "duty free, cafes and restaurants";
    case RV_TRANSPORT: return "buses, taxis, car hire and parking";
    default:           return "the people who work here";
    }
}

/*  Seed reviews.
 *
 *  Written for this application rather than copied from anywhere: a review
 *  book that lifted real people's words from a real company's website would
 *  be passing off their writing, and their names, as something they had said
 *  about Plaisance. These are the kinds of things passengers say about an
 *  airport, spread across the services so the averages differ and the screen
 *  has something to actually show.                                         */
typedef struct {
    ReviewService s; int stars; const char *who; const char *text;
    int day, month;
} SeedReview;

static const SeedReview SEED[] = {
    { RV_OVERALL, 5, "Ashvin R.",
      "Landed at six in the morning and was through immigration and out of "
      "the door in twenty minutes. The new terminal is bright, the signage is "
      "clear and there is enough seating airside for once.", 12, 8 },
    { RV_SECURITY, 3, "Marie-Claire L.",
      "Screening was quick at the fast track lane but the standard lane was "
      "backed up to the escalator at eight in the morning. Two of the five "
      "lanes were closed. Fine once you get through.", 9, 8 },
    { RV_LOUNGE, 5, "Devraj S.",
      "The lounge is genuinely good value. Hot food, proper coffee, quiet "
      "corners with power sockets and a view over the apron. Staff came round "
      "to tell me my gate had changed, which I appreciated.", 3, 8 },
    { RV_BAGGAGE, 2, "Priya N.",
      "Waited fifty minutes for bags off a full widebody. Only one belt was "
      "running for two flights that landed within ten minutes of each other. "
      "Nobody announced anything.", 28, 7 },
    { RV_CHECKIN, 4, "Jean-Paul A.",
      "Air Mauritius opened all four desks for the Paris flight and it moved "
      "fast. Bag drop for online check-in could be signposted better -- I "
      "queued in the wrong place first.", 22, 7 },
    { RV_FASTTRACK, 5, "Hemant B.",
      "Booked fast track for my parents who are both in their eighties. Met "
      "at the kerb, wheelchairs ready, through security and to the gate "
      "without them having to stand once. Worth every rupee.", 19, 7 },
    { RV_SHOPS, 3, "Sandrine V.",
      "Decent duty free and the local rum selection is good. The cafe airside "
      "is expensive for what it is and there is not much open before five in "
      "the morning for the early bank.", 15, 7 },
    { RV_TRANSPORT, 4, "Yash K.",
      "Taxi rank is well organised and the fares are posted, so no haggling. "
      "The bus to Port Louis is cheap but slow and the timetable at the stop "
      "was out of date.", 11, 7 },
    { RV_STAFF, 5, "Anjali D.",
      "My flight was cancelled and the ground staff had rebooked me and "
      "arranged a hotel within half an hour. Calm and straightforward about "
      "it, which is all you want at eleven at night.", 6, 7 },
    { RV_OVERALL, 4, "Thomas W.",
      "Efficient little airport. Immigration was slow because two flights "
      "landed together, but the wifi worked, the toilets were clean and the "
      "walk to the gate is short.", 2, 7 },
    { RV_SECURITY, 5, "Nadia G.",
      "Through security in six minutes at midday. The officer explained "
      "exactly what to take out of the bag before I reached the tray, which "
      "made the whole thing painless.", 30, 6 },
    { RV_BAGGAGE, 4, "Rajesh P.",
      "Bags were on the belt before I had cleared immigration. One of my "
      "cases had a scuff but nothing broken. Trolleys were free and there "
      "were plenty of them.", 24, 6 },
};

void rv_init(ReviewBook *B, World *w)
{
    memset(B, 0, sizeof *B);
    B->nextId = 1;
    int n = (int)(sizeof SEED / sizeof SEED[0]);
    for (int i = 0; i < n && B->n < MAX_REVIEWS; i++) {
        Review *r = &B->r[B->n++];
        memset(r, 0, sizeof *r);
        r->id       = B->nextId++;
        r->stars    = SEED[i].stars;
        r->service  = SEED[i].s;
        r->day      = SEED[i].day;
        r->month    = SEED[i].month;
        r->year     = w->year;
        r->verified = 1;
        r->helpful  = (int)((w->rng >> (i & 15)) % 24u);
        snprintf(r->author, sizeof r->author, "%s", SEED[i].who);
        snprintf(r->text,   sizeof r->text,   "%s", SEED[i].text);
    }
}

int rv_add(ReviewBook *B, World *w, ReviewService s, int stars,
           const char *author, const char *text, int verified, int own)
{
    if (stars < 1 || stars > 5) return 0;
    if (!text || !*text) return 0;

    if (B->n >= MAX_REVIEWS) {
        /* the book is a ring: the oldest review makes way */
        memmove(&B->r[0], &B->r[1], sizeof(Review) * (MAX_REVIEWS - 1));
        B->n = MAX_REVIEWS - 1;
    }
    Review *r = &B->r[B->n++];
    memset(r, 0, sizeof *r);
    r->id        = B->nextId++;
    r->stars     = stars;
    r->service   = s;
    r->day       = w->day;
    r->month     = w->month;
    r->year      = w->year;
    r->verified  = verified;
    r->ownReview = own;
    r->userSubmitted = 1;            /* everything added here is a real post */
    snprintf(r->author, sizeof r->author, "%s",
             (author && *author) ? author : "Anonymous");
    snprintf(r->text, sizeof r->text, "%s", text);

    world_log(w, LG_INFO, "Review left: %d stars for %s",
              stars, rv_service_name(s));
    return r->id;
}

/* --------------------------------------------------------------------------
 *  Persistence.  A pipe-delimited line per posted review; the free text has
 *  its pipes and newlines stripped so one review is always exactly one line.
 * ------------------------------------------------------------------------- */
void rv_save(const ReviewBook *B, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "# AURA passenger reviews posted in the app.\n"
               "# service|stars|verified|day|month|year|author|text\n");
    for (int i = 0; i < B->n; i++) {
        const Review *r = &B->r[i];
        if (!r->userSubmitted) continue;         /* seeds live in the source */
        char au[64], tx[REVIEW_TEXT];
        int k = 0;
        for (const char *p = r->author; *p && k < (int)sizeof au - 1; p++)
            au[k++] = (*p == '|' || *p == '\n' || *p == '\r') ? ' ' : *p;
        au[k] = 0;
        k = 0;
        for (const char *p = r->text; *p && k < (int)sizeof tx - 1; p++)
            tx[k++] = (*p == '|' || *p == '\n' || *p == '\r') ? ' ' : *p;
        tx[k] = 0;
        fprintf(f, "%d|%d|%d|%d|%d|%d|%s|%s\n", (int)r->service, r->stars,
                r->verified, r->day, r->month, r->year, au, tx);
    }
    fclose(f);
}

int rv_load(ReviewBook *B, World *w, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    /*  Idempotent: drop the reviews we previously loaded/posted and rebuild
     *  them from the file, so reloading (on entering the screen, or picking
     *  up another account's post) never duplicates anything.  The seeds are
     *  left untouched.                                                      */
    int keep = 0;
    for (int i = 0; i < B->n; i++)
        if (!B->r[i].userSubmitted) B->r[keep++] = B->r[i];
    B->n = keep;

    char line[REVIEW_TEXT + 128];
    int loaded = 0;
    while (fgets(line, sizeof line, f) && B->n < MAX_REVIEWS) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        size_t ln = strlen(line);
        while (ln && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = 0;

        /* split on the first seven pipes; the text keeps any remainder */
        char *fld[8]; int nf = 0;
        char *p = line;
        for (; nf < 7; nf++) {
            char *bar = strchr(p, '|');
            if (!bar) break;
            *bar = 0; fld[nf] = p; p = bar + 1;
        }
        if (nf < 7) continue;                    /* malformed line          */
        fld[7] = p;                              /* the rest is the text    */

        Review *r = &B->r[B->n++];
        memset(r, 0, sizeof *r);
        r->id        = B->nextId++;
        r->service   = (ReviewService)atoi(fld[0]);
        r->stars     = atoi(fld[1]);
        r->verified  = atoi(fld[2]);
        r->day       = atoi(fld[3]);
        r->month     = atoi(fld[4]);
        r->year      = atoi(fld[5]);
        r->userSubmitted = 1;
        if (r->service < 0 || r->service >= RV_SERVICE_COUNT) r->service = RV_OVERALL;
        if (r->stars < 1 || r->stars > 5) r->stars = 3;
        snprintf(r->author, sizeof r->author, "%s", fld[6]);
        snprintf(r->text,   sizeof r->text,   "%s", fld[7]);
        loaded++;
    }
    fclose(f);
    (void)w;
    return loaded;
}

int rv_count(const ReviewBook *B, ReviewService s)
{
    if (s == RV_SERVICE_COUNT) return B->n;
    int n = 0;
    for (int i = 0; i < B->n; i++) if (B->r[i].service == s) n++;
    return n;
}

float rv_average(const ReviewBook *B, ReviewService s)
{
    int n = 0, sum = 0;
    for (int i = 0; i < B->n; i++) {
        if (s != RV_SERVICE_COUNT && B->r[i].service != s) continue;
        sum += B->r[i].stars;
        n++;
    }
    return n ? (float)sum / (float)n : 0.f;
}

int rv_distribution(const ReviewBook *B, ReviewService s, int out[5])
{
    int n = 0;
    for (int i = 0; i < 5; i++) out[i] = 0;
    for (int i = 0; i < B->n; i++) {
        if (s != RV_SERVICE_COUNT && B->r[i].service != s) continue;
        int st = B->r[i].stars;
        if (st >= 1 && st <= 5) { out[st-1]++; n++; }
    }
    return n;
}

int rv_filter(const ReviewBook *B, ReviewService s, int *out, int max)
{
    int n = 0;
    /* newest first */
    for (int i = B->n - 1; i >= 0 && n < max; i--) {
        if (s != RV_SERVICE_COUNT && B->r[i].service != s) continue;
        out[n++] = i;
    }
    return n;
}

/* ==========================================================================
 *  lost property
 * ========================================================================== */

const char *lp_state_name(LostState s)
{
    switch (s) {
    case LP_REPORTED: return "Reported missing";
    case LP_FOUND:    return "Handed in";
    case LP_MATCHED:  return "Matched to owner";
    default:          return "Returned";
    }
}

static void make_ref(LostBook *L, char *out, size_t cap)
{
    snprintf(out, cap, "LP%05d", L->nextId);
}

void lp_init(LostBook *L, World *w)
{
    memset(L, 0, sizeof *L);
    L->nextId = 1;

    /* a handful of open cases, so the desk is not empty at start-up */
    static const struct { const char *what, *where; int found; } S[] = {
        { "Black cabin case, no name tag",   "Gate 8 seating",        1 },
        { "Reading glasses in a red case",   "Security lane 2 tray",  1 },
        { "Child's soft toy, grey rabbit",   "Departures play area",  1 },
        { "Passport wallet, navy leather",   "Immigration desk 4",    1 },
        { "Silver laptop, 13 inch",          "Lounge, seat 22",       1 },
    };
    for (int i = 0; i < (int)(sizeof S / sizeof S[0]); i++)
        lp_found(L, w, S[i].what, S[i].where);

    /* and one passenger looking for something */
    lp_report(L, w, "Beige sun hat, wide brim", "Arrivals hall",
              "K. Appadoo", "airport desk", 0);
}

static LostItem *lp_new(LostBook *L, World *w)
{
    if (L->n >= MAX_LOST) {
        int drop = -1;
        for (int i = 0; i < L->n; i++)
            if (L->it[i].state == LP_RETURNED) { drop = i; break; }
        if (drop < 0) drop = 0;
        memmove(&L->it[drop], &L->it[drop+1],
                sizeof(LostItem) * (size_t)(L->n - drop - 1));
        L->n--;
    }
    LostItem *it = &L->it[L->n++];
    memset(it, 0, sizeof *it);
    it->id       = L->nextId;
    it->bagIndex = -1;
    it->atMin    = w->clock;
    it->day      = w->day;
    it->month    = w->month;
    make_ref(L, it->ref, sizeof it->ref);
    L->nextId++;
    return it;
}

int lp_report(LostBook *L, World *w, const char *what, const char *where,
              const char *owner, const char *contact, int paxId)
{
    if (!what || !*what) return 0;
    LostItem *it = lp_new(L, w);
    it->state = LP_REPORTED;
    it->paxId = paxId;
    snprintf(it->what,    sizeof it->what,    "%s", what);
    snprintf(it->where,   sizeof it->where,   "%s", where   ? where   : "Unknown");
    snprintf(it->owner,   sizeof it->owner,   "%s", owner   ? owner   : "");
    snprintf(it->contact, sizeof it->contact, "%s", contact ? contact : "");
    world_log(w, LG_WARN, "Lost property %s reported: %s", it->ref, it->what);
    return it->id;
}

int lp_found(LostBook *L, World *w, const char *what, const char *where)
{
    if (!what || !*what) return 0;
    LostItem *it = lp_new(L, w);
    it->state = LP_FOUND;
    snprintf(it->what,  sizeof it->what,  "%s", what);
    snprintf(it->where, sizeof it->where, "%s", where ? where : "Unknown");
    return it->id;
}

int lp_report_bag(LostBook *L, World *w, int bagIndex, const char *contact)
{
    if (bagIndex < 0 || bagIndex >= w->nBags) return 0;
    Bag *b = &w->bag[bagIndex];

    /* one open claim per bag */
    for (int i = 0; i < L->n; i++)
        if (L->it[i].bagIndex == bagIndex && L->it[i].state != LP_RETURNED)
            return 0;

    LostItem *it = lp_new(L, w);
    it->state    = LP_REPORTED;
    it->isBag    = 1;
    it->bagIndex = bagIndex;
    it->paxId    = b->pax;
    snprintf(it->what,  sizeof it->what,  "Checked bag %s, %.1f kg",
             b->tag, b->weight);
    snprintf(it->where, sizeof it->where, "%s", bs_name(b->state));
    snprintf(it->contact, sizeof it->contact, "%s", contact ? contact : "");
    for (int i = 0; i < w->nPax; i++)
        if (w->pax[i].id == b->pax) {
            snprintf(it->owner, sizeof it->owner, "%s", w->pax[i].name);
            break;
        }

    b->state = BG_MISHANDLED;
    w->bagsMishandled++;
    world_log(w, LG_ERR, "Bag %s reported missing -- claim %s", b->tag, it->ref);
    return it->id;
}

static LostItem *lp_by_id(LostBook *L, int id)
{
    for (int i = 0; i < L->n; i++) if (L->it[i].id == id) return &L->it[i];
    return NULL;
}

int lp_match(LostBook *L, World *w, int id)
{
    LostItem *it = lp_by_id(L, id);
    if (!it || it->state == LP_RETURNED || it->state == LP_MATCHED) return 0;
    it->state = LP_MATCHED;
    world_log(w, LG_OK, "Lost property %s matched to its owner", it->ref);
    return 1;
}

int lp_return(LostBook *L, World *w, int id)
{
    LostItem *it = lp_by_id(L, id);
    if (!it || it->state == LP_RETURNED) return 0;
    it->state = LP_RETURNED;
    L->returned++;
    if (it->isBag && it->bagIndex >= 0 && it->bagIndex < w->nBags) {
        w->bag[it->bagIndex].state = BG_DELIVERED;
        if (w->bagsMishandled > 0) w->bagsMishandled--;
    }
    world_log(w, LG_OK, "Lost property %s returned to its owner", it->ref);
    return 1;
}

int lp_open(const LostBook *L)
{
    int n = 0;
    for (int i = 0; i < L->n; i++) if (L->it[i].state != LP_RETURNED) n++;
    return n;
}

static int ci_has(const char *hay, const char *needle)
{
    if (!needle || !*needle) return 1;
    for (const char *h = hay; *h; h++) {
        const char *a = h, *b = needle;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) break;
            a++; b++;
        }
        if (!*b) return 1;
    }
    return 0;
}

int lp_find(const LostBook *L, const char *query, int *out, int max)
{
    int n = 0;
    for (int i = L->n - 1; i >= 0 && n < max; i--) {
        const LostItem *it = &L->it[i];
        if (!query || !*query ||
            ci_has(it->what, query) || ci_has(it->where, query) ||
            ci_has(it->owner, query) || ci_has(it->ref, query))
            out[n++] = i;
    }
    return n;
}
