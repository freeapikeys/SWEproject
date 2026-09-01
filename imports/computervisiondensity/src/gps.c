#include "gps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct gps_state_t {
    pthread_t thread;
    pthread_mutex_t lock;
    double lat;
    double lon;
    bool running;
    bool has_data;
    char sim_path[512];
};

static void *sim_thread(void *arg){
    gps_state_t *s = (gps_state_t*)arg;
    FILE *f = fopen(s->sim_path, "r");
    if(!f){
        perror("fopen sim gps");
        s->running = false;
        return NULL;
    }
    char line[256];
    while(s->running){
        if(fgets(line, sizeof(line), f)==NULL){
            fseek(f, 0, SEEK_SET);
            continue;
        }
        double lat, lon;
        double t;
        if(sscanf(line, "%lf %lf %lf", &lat, &lon, &t) >= 2){
            pthread_mutex_lock(&s->lock);
            s->lat = lat;
            s->lon = lon;
            s->has_data = true;
            pthread_mutex_unlock(&s->lock);
        }
        sleep(1);
    }
    fclose(f);
    return NULL;
}

void gps_init(gps_state_t *s){
    memset(s, 0, sizeof(*s));
    pthread_mutex_init(&s->lock, NULL);
    s->running = false;
}

bool gps_start_simulation(gps_state_t *s, const char *path){
    strncpy(s->sim_path, path, sizeof(s->sim_path)-1);
    s->running = true;
    if(pthread_create(&s->thread, NULL, sim_thread, s)!=0){
        perror("pthread_create");
        s->running = false;
        return false;
    }
    return true;
}

void gps_get_position(gps_state_t *s, double *lat, double *lon){
    pthread_mutex_lock(&s->lock);
    if(s->has_data){
        *lat = s->lat;
        *lon = s->lon;
    } else {
        *lat = 0.0; *lon = 0.0;
    }
    pthread_mutex_unlock(&s->lock);
}

void gps_shutdown(gps_state_t *s){
    if(!s) return;
    s->running = false;
    pthread_join(s->thread, NULL);
    pthread_mutex_destroy(&s->lock);
}
