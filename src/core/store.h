/* ==========================================================================
 *  AURA :: store.h  --  persistence layer
 *
 *  The brief asks for the use of files, so the operational record is written
 *  as plain comma-separated text under data/.  Plain text was chosen over a
 *  binary dump deliberately: the files can be opened, diffed and reviewed in
 *  the repository, which is what makes them useful evidence in the report.
 *
 *      data/flights.csv     the movement schedule and its live state
 *      data/passengers.csv  the passenger roster
 *      data/baggage.csv     every bag and where it currently is
 *      data/staff.csv       the duty roster
 *      data/journal.log     an append-only audit trail
 *      data/aura.cfg        interface preferences
 * ========================================================================== */
#ifndef AURA_STORE_H
#define AURA_STORE_H

#include "model.h"

typedef struct {
    int   ok;
    int   records;
    char  message[160];
    char  path[320];
} StoreResult;

void store_set_root(const char *dir);
const char *store_root(void);

StoreResult store_save_flights   (World *w);
StoreResult store_load_flights   (World *w);
StoreResult store_save_passengers(World *w);
StoreResult store_load_passengers(World *w);
StoreResult store_save_baggage   (World *w);
StoreResult store_load_baggage   (World *w);
StoreResult store_save_staff     (World *w);
StoreResult store_save_all       (World *w);
StoreResult store_export_report  (World *w);

void store_journal(World *w, const char *what);
int  store_file_size(const char *name);
int  store_exists(const char *name);
int  store_read_tail(const char *name, char *out, int cap, int maxLines);

#endif /* AURA_STORE_H */
