#include "tracker.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static float iou_rects(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){
    int a1 = ax, b1 = ay, a2 = ax+aw, b2 = ay+ah;
    int c1 = bx, d1 = by, c2 = bx+bw, d2 = by+bh;
    int ix1 = a1>c1?a1:c1;
    int iy1 = b1>d1?b1:d1;
    int ix2 = a2<c2?a2:c2;
    int iy2 = b2<d2?b2:d2;
    int iw = ix2 - ix1;
    int ih = iy2 - iy1;
    if(iw<=0 || ih<=0) return 0.0f;
    int inter = iw*ih;
    int areaA = aw*ah;
    int areaB = bw*bh;
    return (float)inter / (areaA + areaB - inter + 1e-6f);
}

Tracker* tracker_create(){
    Tracker* t = (Tracker*)calloc(1,sizeof(Tracker));
    t->tracks = NULL;
    t->n = 0;
    t->next_id = 1;
    t->max_missed = 5;
    return t;
}
void tracker_destroy(Tracker* t){
    if(!t) return;
    free(t->tracks);
    free(t);
}

// very simple greedy matching by IoU
void tracker_update(Tracker* t, int ndet, int det_x[], int det_y[], int det_w[], int det_h[]){
    // mark all tracks as unmatched initially
    int i,j;
    int *matched = (int*)calloc(t->n, sizeof(int));
    int *det_assigned = (int*)calloc(ndet, sizeof(int));
    // compute IoU matrix and greedily match
    for(i=0;i<t->n;i++){
        float best_iou = 0.0f;
        int best_j = -1;
        for(j=0;j<ndet;j++){
            if(det_assigned[j]) continue;
            float iou = iou_rects(t->tracks[i].x,t->tracks[i].y,t->tracks[i].w,t->tracks[i].h,
                                  det_x[j],det_y[j],det_w[j],det_h[j]);
            if(iou > best_iou){ best_iou = iou; best_j = j; }
        }
        if(best_j!=-1 && best_iou>0.15f){
            // update track
            t->tracks[i].x = det_x[best_j];
            t->tracks[i].y = det_y[best_j];
            t->tracks[i].w = det_w[best_j];
            t->tracks[i].h = det_h[best_j];
            t->tracks[i].missed = 0;
            matched[i]=1;
            det_assigned[best_j]=1;
        } else {
            t->tracks[i].missed++;
        }
    }
    // create new tracks for unassigned detections
    for(j=0;j<ndet;j++){
        if(det_assigned[j]) continue;
        t->tracks = (Track*)realloc(t->tracks, sizeof(Track)*(t->n+1));
        t->tracks[t->n].id = t->next_id++;
        t->tracks[t->n].x = det_x[j];
        t->tracks[t->n].y = det_y[j];
        t->tracks[t->n].w = det_w[j];
        t->tracks[t->n].h = det_h[j];
        t->tracks[t->n].missed = 0;
        t->n++;
    }
    // remove too-missed tracks
    int write=0;
    for(i=0;i<t->n;i++){
        if(t->tracks[i].missed <= t->max_missed){
            if(write!=i) t->tracks[write] = t->tracks[i];
            write++;
        }
    }
    t->n = write;
    // shrink array
    t->tracks = (Track*)realloc(t->tracks, sizeof(Track)*(t->n));
    free(matched);
    free(det_assigned);
}

int tracker_count(Tracker* t){
    return t ? t->n : 0;
}
