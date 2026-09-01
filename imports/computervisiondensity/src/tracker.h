#ifndef TRACKER_H
#define TRACKER_H
typedef struct {
    int id;
    int x,y,w,h;
    int missed;
} Track;
typedef struct {
    Track *tracks;
    int n;
    int next_id;
    int max_missed;
} Tracker;
Tracker* tracker_create();
void tracker_destroy(Tracker* t);
void tracker_update(Tracker* t, int ndet, int det_x[], int det_y[], int det_w[], int det_h[]);
int tracker_count(Tracker* t);
#endif
