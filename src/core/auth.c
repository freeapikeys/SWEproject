/* ==========================================================================
 *  AURA :: auth.c  --  accounts
 * ========================================================================== */

#include "auth.h"
#include "../engine/anim.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 *  SHA-256   (FIPS 180-4)
 * ========================================================================== */

static const uint32_t K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,
    0x923f82a4u,0xab1c5ed5u,0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,0xe49b69c1u,0xefbe4786u,
    0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,
    0x06ca6351u,0x14292967u,0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,0xa2bfe8a1u,0xa81a664bu,
    0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,
    0x5b9cca4fu,0x682e6ff3u,0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(Sha256 *s, const uint8_t *p)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19)  ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3];
    uint32_t e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}

void sha256_init(Sha256 *s)
{
    s->h[0]=0x6a09e667u; s->h[1]=0xbb67ae85u; s->h[2]=0x3c6ef372u;
    s->h[3]=0xa54ff53au; s->h[4]=0x510e527fu; s->h[5]=0x9b05688cu;
    s->h[6]=0x1f83d9abu; s->h[7]=0x5be0cd19u;
    s->bits = 0; s->n = 0;
}

void sha256_update(Sha256 *s, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    s->bits += (uint64_t)len * 8u;
    while (len) {
        size_t take = 64 - s->n;
        if (take > len) take = len;
        memcpy(s->buf + s->n, p, take);
        s->n += take; p += take; len -= take;
        if (s->n == 64) { sha256_block(s, s->buf); s->n = 0; }
    }
}

void sha256_final(Sha256 *s, uint8_t out[32])
{
    uint64_t bits = s->bits;
    uint8_t pad = 0x80;
    sha256_update(s, &pad, 1);
    pad = 0x00;
    while (s->n != 56) sha256_update(s, &pad, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - i*8));
    /* written straight into the block: going through update() here would
     * add these eight bytes to the length counter we have just encoded */
    memcpy(s->buf + 56, len, 8);
    sha256_block(s, s->buf);
    s->n = 0;
    for (int i = 0; i < 8; i++) {
        out[i*4]   = (uint8_t)(s->h[i] >> 24);
        out[i*4+1] = (uint8_t)(s->h[i] >> 16);
        out[i*4+2] = (uint8_t)(s->h[i] >> 8);
        out[i*4+3] = (uint8_t)(s->h[i]);
    }
}

static void hex_of(const uint8_t *d, int n, char *out)
{
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        out[i*2]   = H[(d[i] >> 4) & 15];
        out[i*2+1] = H[d[i] & 15];
    }
    out[n*2] = 0;
}

void sha256_hex(const uint8_t d[32], char *out) { hex_of(d, 32, out); }

/* ==========================================================================
 *  password digests
 *
 *  A single hash of a password is worth very little: an attacker holding the
 *  file tries a word list against it at millions of guesses a second.  Two
 *  things fix that -- a per-account random salt, so one precomputed table
 *  cannot attack every account at once, and a large iteration count, so each
 *  guess costs real time.  A hundred thousand rounds is about ninety
 *  milliseconds: nobody signing in notices, a word-list attack does.
 * ========================================================================== */

static void derive(const uint8_t salt[16], const char *pw, uint8_t out[32])
{
    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, salt, 16);
    sha256_update(&s, pw, strlen(pw));
    sha256_final(&s, out);

    for (int i = 1; i < AUTH_ITERATIONS; i++) {
        uint8_t counter[4] = { (uint8_t)(i >> 24), (uint8_t)(i >> 16),
                               (uint8_t)(i >> 8),  (uint8_t)i };
        sha256_init(&s);
        sha256_update(&s, out, 32);
        sha256_update(&s, salt, 16);
        sha256_update(&s, counter, 4);
        sha256_final(&s, out);
    }
}

/* Constant time: returning on the first differing byte tells anybody who can
 * time the call how much of their guess was right. */
static int same_digest(const uint8_t a[32], const uint8_t b[32])
{
    uint8_t diff = 0;
    for (int i = 0; i < 32; i++) diff |= (uint8_t)(a[i] ^ b[i]);
    return diff == 0;
}

static void make_salt(uint8_t salt[16])
{
    /* No CryptoAPI here, so the salt is mixed from the high-resolution
     * counters the platform does expose.  A salt has to be unique per
     * account, not unpredictable, so this is adequate for its job. */
    FILETIME ft;
    LARGE_INTEGER qpc;
    GetSystemTimeAsFileTime(&ft);
    QueryPerformanceCounter(&qpc);

    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, &ft,  sizeof ft);
    sha256_update(&s, &qpc, sizeof qpc);
    DWORD pid = GetCurrentProcessId(), tid = GetCurrentThreadId();
    DWORD tick = GetTickCount();
    sha256_update(&s, &pid,  sizeof pid);
    sha256_update(&s, &tid,  sizeof tid);
    sha256_update(&s, &tick, sizeof tick);
    void *addr = (void *)salt;
    sha256_update(&s, &addr, sizeof addr);

    uint8_t d[32];
    sha256_final(&s, d);
    memcpy(salt, d, 16);
}

/* ==========================================================================
 *  small helpers
 * ========================================================================== */

static int ieq(const char *a, const char *b)
{
    for (;; a++, b++) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}

static void trim(char *s)
{
    size_t n = strlen(s);
    while (n && (s[n-1]=='\n' || s[n-1]=='\r' || s[n-1]==' ' || s[n-1]=='\t'))
        s[--n] = 0;
}

static void path_join(char *out, int cap, const char *dir, const char *file)
{
    snprintf(out, (size_t)cap, "%s%s%s", (dir && *dir) ? dir : ".",
             (dir && *dir && dir[strlen(dir)-1] == '/') ? "" : "/", file);
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_bytes(const char *s, uint8_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int hi = hex_nibble(s[i*2]), lo = hex_nibble(s[i*2+1]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 1;
}

int auth_find(const AuthSystem *a, const char *email)
{
    for (int i = 0; i < a->nAcct; i++)
        if (ieq(a->acct[i].email, email)) return i;
    return -1;
}

/*  A shape check, not a delivery check.  Nothing here can tell whether an
 *  address exists -- that needs a mail server -- so it rejects only what
 *  cannot possibly be an address and lets everything else through.  Being
 *  stricter would start refusing perfectly real addresses, which is a worse
 *  failure than accepting a typo.                                          */
int auth_email_ok(const char *email)
{
    int at = 0, dotAfterAt = 0, lenBefore = 0, lenAfter = 0;
    const char *p = email;
    if (!p || !*p) return 0;
    for (; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == ',' || *p == ';') return 0;
        if (*p == '@') { at++; continue; }
        if (!at) lenBefore++;
        else {
            lenAfter++;
            if (*p == '.') dotAfterAt = 1;
        }
    }
    if (at != 1 || !lenBefore || lenAfter < 3 || !dotAfterAt) return 0;
    if (email[0] == '.' || email[0] == '@') return 0;
    size_t n = strlen(email);
    if (email[n-1] == '.' || email[n-1] == '@') return 0;
    if (n >= AUTH_EMAIL_MAX) return 0;
    return 1;
}

/* ==========================================================================
 *  the account file
 * ========================================================================== */

static void auth_save(const AuthSystem *a, const char *dir)
{
    char path[320];
    path_join(path, sizeof path, dir, "accounts.dat");
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "# AURA accounts.\n"
               "# email | name | kind | salt | salted SHA-256, %d iterations\n"
               "# No password is stored here and none can be recovered from"
               " this file.\n", AUTH_ITERATIONS);
    char sh[33], dh[65];
    for (int i = 0; i < a->nAcct; i++) {
        hex_of(a->acct[i].salt,   16, sh);
        hex_of(a->acct[i].digest, 32, dh);
        fprintf(f, "%s|%s|%d|%s|%s\n", a->acct[i].email, a->acct[i].name,
                (int)a->acct[i].kind, sh, dh);
    }
    fclose(f);
}

static char *split(char *s, char sep)
{
    char *p = strchr(s, sep);
    if (!p) return NULL;
    *p = 0;
    return p + 1;
}

static void auth_load(AuthSystem *a, const char *dir)
{
    char path[320];
    path_join(path, sizeof path, dir, "accounts.dat");
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[420];
    while (fgets(line, sizeof line, f) && a->nAcct < AUTH_MAX_ACCOUNTS) {
        trim(line);
        if (!line[0] || line[0] == '#') continue;

        char *name = split(line, '|');   if (!name) continue;
        char *kind = split(name, '|');   if (!kind) continue;
        char *salt = split(kind, '|');   if (!salt) continue;
        char *dig  = split(salt, '|');   if (!dig)  continue;
        if (strlen(salt) < 32 || strlen(dig) < 64) continue;
        if (strlen(line) >= AUTH_EMAIL_MAX) continue;
        if (auth_find(a, line) >= 0) continue;

        Account *ac = &a->acct[a->nAcct];
        memset(ac, 0, sizeof *ac);
        if (!hex_bytes(salt, ac->salt, 16))   continue;
        if (!hex_bytes(dig,  ac->digest, 32)) continue;
        snprintf(ac->email, sizeof ac->email, "%s", line);
        snprintf(ac->name,  sizeof ac->name,  "%s", name);
        ac->kind = (atoi(kind) == 1) ? ACC_STAFF : ACC_TRAVELLER;
        a->nAcct++;
    }
    fclose(f);
}

void auth_init(AuthSystem *a, const char *dir)
{
    memset(a, 0, sizeof *a);
    a->stage = AUTH_LOCKED;
    a->who   = -1;
    auth_load(a, dir);
    snprintf(a->message, sizeof a->message,
             a->nAcct ? "Welcome back. Sign in to continue."
                      : "New here? Create an account -- it takes a moment.");
    a->messageBad = 0;
}

int auth_is_open(const AuthSystem *a) { return a->stage == AUTH_OPEN; }

int auth_locked_out(const AuthSystem *a, float *secondsLeft)
{
    float left = a->lockUntil - anim_time();
    if (left <= 0.f) return 0;
    if (secondsLeft) *secondsLeft = left;
    return 1;
}

/* ==========================================================================
 *  password rules
 * ========================================================================== */

int auth_pw_strength(const char *pw, char *why, int cap)
{
    int len = (int)strlen(pw);
    int lower = 0, upper = 0, digit = 0, other = 0;
    for (const char *p = pw; *p; p++) {
        if      (*p >= 'a' && *p <= 'z') lower = 1;
        else if (*p >= 'A' && *p <= 'Z') upper = 1;
        else if (*p >= '0' && *p <= '9') digit = 1;
        else                             other = 1;
    }
    int classes = lower + upper + digit + other;

    if (len < 8) {
        snprintf(why, (size_t)cap, "At least 8 characters -- %d so far.", len);
        return 0;
    }
    if (classes < 2) {
        snprintf(why, (size_t)cap,
                 "Mix at least two of: lower case, capitals, digits, symbols.");
        return 0;
    }
    if (len >= 12 && classes >= 3) {
        snprintf(why, (size_t)cap, "Strong password.");
        return 2;
    }
    snprintf(why, (size_t)cap, "Good enough.");
    return 1;
}

/* ==========================================================================
 *  sign in and register
 * ========================================================================== */

static void open_session(AuthSystem *a, int idx)
{
    a->who        = idx;
    a->stage      = AUTH_OPEN;
    a->failures   = 0;
    a->signedInAt = anim_time();
    a->kind       = a->acct[idx].kind;
    snprintf(a->email, sizeof a->email, "%s", a->acct[idx].email);
    snprintf(a->name,  sizeof a->name,  "%s", a->acct[idx].name);
}

int auth_verify(AuthSystem *a, const char *email, const char *pw)
{
    float left = 0.f;
    if (auth_locked_out(a, &left)) {
        snprintf(a->message, sizeof a->message,
                 "Too many attempts. Try again in %d seconds.", (int)left + 1);
        a->messageBad = 1;
        return 0;
    }

    if (!auth_email_ok(email)) {
        snprintf(a->message, sizeof a->message,
                 "That does not look like an email address.");
        a->messageBad = 1;
        return 0;
    }

    int idx = auth_find(a, email);
    if (idx < 0) {
        /*  Registration is open, so there is nothing to protect by being
         *  coy about which addresses exist -- anybody can find that out by
         *  trying to register.  Saying so plainly is the more useful
         *  behaviour and does not leak anything.                          */
        a->stage = AUTH_REGISTER;
        snprintf(a->message, sizeof a->message,
                 "No account for that address yet. Create one below.");
        a->messageBad = 0;
        return 0;
    }

    uint8_t got[32];
    derive(a->acct[idx].salt, pw, got);
    if (!same_digest(got, a->acct[idx].digest)) {
        a->failures++;
        int leftTries = AUTH_MAX_TRIES - a->failures;
        if (leftTries > 0)
            snprintf(a->message, sizeof a->message,
                     "Wrong password. %d attempt%s left.",
                     leftTries, leftTries == 1 ? "" : "s");
        else {
            a->lockUntil = anim_time() + AUTH_LOCKOUT_SEC;
            snprintf(a->message, sizeof a->message,
                     "Too many attempts. Locked for %d seconds.",
                     (int)AUTH_LOCKOUT_SEC);
        }
        a->messageBad = 1;
        return 0;
    }

    open_session(a, idx);
    snprintf(a->message, sizeof a->message, "Signed in.");
    a->messageBad = 0;
    return 1;
}

int auth_register(AuthSystem *a, const char *email, const char *name,
                  const char *pw, const char *confirm, AccountKind kind,
                  const char *dir)
{
    if (a->nAcct >= AUTH_MAX_ACCOUNTS) {
        snprintf(a->message, sizeof a->message,
                 "This installation is holding its maximum of %d accounts.",
                 AUTH_MAX_ACCOUNTS);
        a->messageBad = 1;
        return 0;
    }
    if (!auth_email_ok(email)) {
        snprintf(a->message, sizeof a->message,
                 "That does not look like an email address.");
        a->messageBad = 1;
        return 0;
    }
    if (auth_find(a, email) >= 0) {
        snprintf(a->message, sizeof a->message,
                 "There is already an account for that address. Sign in.");
        a->messageBad = 1;
        a->stage = AUTH_LOCKED;
        return 0;
    }
    if (!name || !*name) {
        snprintf(a->message, sizeof a->message, "Please give a name.");
        a->messageBad = 1;
        return 0;
    }
    char why[130];
    if (!auth_pw_strength(pw, why, (int)sizeof why)) {
        snprintf(a->message, sizeof a->message, "%s", why);
        a->messageBad = 1;
        return 0;
    }
    if (strcmp(pw, confirm) != 0) {
        snprintf(a->message, sizeof a->message,
                 "The two passwords do not match.");
        a->messageBad = 1;
        return 0;
    }

    Account *ac = &a->acct[a->nAcct];
    memset(ac, 0, sizeof *ac);
    snprintf(ac->email, sizeof ac->email, "%s", email);
    snprintf(ac->name,  sizeof ac->name,  "%s", name);
    ac->kind = kind;
    make_salt(ac->salt);
    derive(ac->salt, pw, ac->digest);
    a->nAcct++;

    auth_save(a, dir);
    open_session(a, a->nAcct - 1);
    snprintf(a->message, sizeof a->message, "Account created. Welcome.");
    a->messageBad = 0;
    return 1;
}

void auth_sign_out(AuthSystem *a)
{
    a->stage    = AUTH_LOCKED;
    a->who      = -1;
    a->failures = 0;
    a->email[0] = 0;
    a->name[0]  = 0;
    snprintf(a->message, sizeof a->message, "Signed out.");
    a->messageBad = 0;
}
