#ifndef GPS_H
#define GPS_H

#include <stdbool.h>

typedef struct gps_state_t gps_state_t;

// initialize state
void gps_init(gps_state_t *s);

// start simulated playback from file (path: assets/sample_gps.txt)
bool gps_start_simulation(gps_state_t *s, const char *path);

// populate out lat,lon with last known position
void gps_get_position(gps_state_t *s, double *lat, double *lon);

// shutdown and free resources
void gps_shutdown(gps_state_t *s);

#endif
