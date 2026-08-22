/* ==========================================================================
 *  AURA :: auth.h  --  accounts
 *
 *  Anyone can hold an AURA account.  A traveller creates one with whatever
 *  address they already use and sets their own password; airport staff do the
 *  same and tick the box that says so.  There is no invitation list and no
 *  administrator to ask -- registration is open, which is the point.
 *
 *  What the account gives you is a profile the application can attach things
 *  to: your bookings on the Welcome screen, the notifications the airport
 *  sends you, and the reviews you leave.
 *
 *  Passwords:  never stored, here or on disk, in the source or anywhere else.
 *  What is written to data/accounts.dat is a per-account random salt and a
 *  SHA-256 digest iterated a hundred thousand times.  The password cannot be
 *  recovered from that file.
 *
 *  This is a local account system.  It is not Google sign-in and does not
 *  pretend to be: federated sign-in needs a browser, a network round trip and
 *  a client secret, none of which a self-contained C application has.  It
 *  protects the application, not the disk.
 * ========================================================================== */
#ifndef AURA_AUTH_H
#define AURA_AUTH_H

#include <stdint.h>
#include <stddef.h>

#define AUTH_MAX_ACCOUNTS 128
#define AUTH_EMAIL_MAX     72
#define AUTH_NAME_MAX      40
#define AUTH_ROLE_MAX      34
#define AUTH_PW_MAX        64
#define AUTH_MAX_TRIES      5
#define AUTH_LOCKOUT_SEC   45.f
#define AUTH_ITERATIONS 100000    /* deliberately slow: ~90 ms per attempt  */

/* ---------------------------------------------------------------------------
 *  SHA-256, FIPS 180-4, written out in full: the brief rules out external
 *  libraries, and a hand-rolled "hash" would make the exercise dishonest.
 * ------------------------------------------------------------------------- */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    uint8_t  buf[64];
    size_t   n;
} Sha256;

void sha256_init  (Sha256 *s);
void sha256_update(Sha256 *s, const void *data, size_t len);
void sha256_final (Sha256 *s, uint8_t out[32]);
void sha256_hex   (const uint8_t d[32], char *out /* >= 65 */);

/* --------------------------------------------------------------- accounts -- */

typedef enum { ACC_TRAVELLER = 0, ACC_STAFF } AccountKind;

typedef struct {
    char        email[AUTH_EMAIL_MAX];
    char        name [AUTH_NAME_MAX];
    AccountKind kind;
    uint8_t     salt[16];
    uint8_t     digest[32];
} Account;

typedef enum { AUTH_LOCKED = 0, AUTH_REGISTER, AUTH_OPEN } AuthStage;

typedef struct {
    Account   acct[AUTH_MAX_ACCOUNTS];
    int       nAcct;

    AuthStage stage;
    int       who;                    /* signed-in account, -1 when none    */
    int       failures;
    float     lockUntil;              /* anim_time() at which entry reopens */

    char      message[150];
    int       messageBad;

    char      email[AUTH_EMAIL_MAX];  /* who is signed in                   */
    char      name [AUTH_NAME_MAX];
    AccountKind kind;
    float     signedInAt;
} AuthSystem;

void auth_init      (AuthSystem *a, const char *dir);
int  auth_find      (const AuthSystem *a, const char *email);
int  auth_is_open   (const AuthSystem *a);
int  auth_locked_out(const AuthSystem *a, float *secondsLeft);
void auth_sign_out  (AuthSystem *a);

/* 1 on success.  On an unknown address, moves to AUTH_REGISTER so the entry
 * screen can offer to create the account rather than dead-ending. */
int  auth_verify    (AuthSystem *a, const char *email, const char *pw);

/* Open registration: any well-formed address, any name, own password. */
int  auth_register  (AuthSystem *a, const char *email, const char *name,
                     const char *pw, const char *confirm, AccountKind kind,
                     const char *dir);

int  auth_email_ok    (const char *email);   /* shape check, not delivery   */
int  auth_pw_strength (const char *pw, char *why, int cap);  /* 0,1,2       */

#endif /* AURA_AUTH_H */
