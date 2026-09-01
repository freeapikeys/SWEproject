#include "renderer.h"
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <math.h>

static const double REF_LAT = -20.4300;
static const double REF_LON = 57.6833;
#define METERS_PER_DEG_LAT 111320.0

static void geo_to_world(double lat, double lon, float *x, float *z){
    double dlat = lat - REF_LAT;
    double dlon = lon - REF_LON;
    double lat_factor = cos(REF_LAT * M_PI/180.0);
    double meters_north = dlat * METERS_PER_DEG_LAT;
    double meters_east = dlon * METERS_PER_DEG_LAT * lat_factor;
    double scale = 0.02;
    *x = (float)(meters_east * scale);
    *z = (float)(meters_north * scale);
}

typedef struct {
    const char *name;
    float cx, cz;
    float sx, sy, sz;
    float color[3];
} segment_t;

static segment_t segments[] = {
    {"Terminal A",  0.0f,  0.0f,  60.0f, 8.0f, 40.0f, {0.3f,0.6f,0.9f}},
    {"Arrivals Hall", -60.0f, -20.0f, 40.0f, 6.0f, 30.0f, {0.9f,0.6f,0.3f}},
    {"Departures Hall", 60.0f, 20.0f, 40.0f, 6.0f, 30.0f, {0.6f,0.9f,0.4f}},
    {"Gates", 0.0f, 40.0f,  120.0f, 4.0f, 20.0f, {0.8f,0.5f,0.8f}},
    {"Parking Area", -140.0f, 80.0f, 120.0f, 2.0f, 80.0f, {0.7f,0.7f,0.7f}},
    {"Taxiway", 0.0f, -120.0f, 20.0f, 1.0f, 200.0f, {0.2f,0.2f,0.2f}},
    {"Runway", 0.0f, -230.0f,  30.0f, 1.0f, 480.0f, {0.1f,0.1f,0.1f}}
};
static const int SEGMENT_COUNT = sizeof(segments)/sizeof(segments[0]);
static float avatar_x = 0.0f;
static float avatar_z = 0.0f;
static SDL_Window *win = NULL;
static SDL_GLContext glctx = NULL;

bool renderer_init(){
    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    win = SDL_CreateWindow("Mauritian Airport Metaverse - Prototype", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if(!win){ fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError()); return false; }
    glctx = SDL_GL_CreateContext(win);
    if(!glctx){ fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError()); return false; }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 1024.0/768.0, 0.1, 2000.0);
    glMatrixMode(GL_MODELVIEW);
    return true;
}

void renderer_shutdown(){
    if(glctx) SDL_GL_DeleteContext(glctx);
    if(win) SDL_DestroyWindow(win);
    SDL_Quit();
}

static void draw_box(float cx, float cy, float cz, float sx, float sy, float sz, float r, float g, float b){
    float hx = sx*0.5f, hy = sy*0.5f, hz = sz*0.5f;
    glColor3f(r,g,b);
    glBegin(GL_QUADS);
    glVertex3f(cx-hx, cy+hy, cz-hz); glVertex3f(cx+hx, cy+hy, cz-hz); glVertex3f(cx+hx, cy+hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz+hz);
    glVertex3f(cx-hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy-hy, cz+hz); glVertex3f(cx-hx, cy-hy, cz+hz);
    glVertex3f(cx-hx, cy-hy, cz+hz); glVertex3f(cx+hx, cy-hy, cz+hz); glVertex3f(cx+hx, cy+hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz+hz);
    glVertex3f(cx-hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy+hy, cz-hz); glVertex3f(cx-hx, cy+hy, cz-hz);
    glVertex3f(cx-hx, cy-hy, cz-hz); glVertex3f(cx-hx, cy-hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz+hz); glVertex3f(cx-hx, cy+hy, cz-hz);
    glVertex3f(cx+hx, cy-hy, cz-hz); glVertex3f(cx+hx, cy-hy, cz+hz); glVertex3f(cx+hx, cy+hy, cz+hz); glVertex3f(cx+hx, cy+hy, cz-hz);
    glEnd();
}

void renderer_frame(){
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    glViewport(0,0,w,h);
    glClearColor(0.53f, 0.8f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float camY = 80.0f;
    float camDist = 220.0f;
    gluLookAt(camDist, camY, camDist, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
    glColor3f(0.15f, 0.6f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(-1000.0f, -1.0f, -1000.0f);
    glVertex3f(1000.0f, -1.0f, -1000.0f);
    glVertex3f(1000.0f, -1.0f, 1000.0f);
    glVertex3f(-1000.0f, -1.0f, 1000.0f);
    glEnd();
    for(int i=0;i<SEGMENT_COUNT;i++){
        segment_t *s = &segments[i];
        draw_box(s->cx, s->sy*0.5f - 1.0f, s->cz, s->sx, s->sy, s->sz, s->color[0], s->color[1], s->color[2]);
    }
    glPushMatrix();
    glTranslatef(avatar_x, 2.0f, avatar_z);
    glScalef(3.0f, 4.0f, 3.0f);
    draw_box(0,0,0,1,1,1, 1.0f, 0.1f, 0.1f);
    glPopMatrix();
    SDL_GL_SwapWindow(win);
}

void renderer_set_avatar_geoposition(double lat, double lon){
    float x,z;
    geo_to_world(lat, lon, &x, &z);
    avatar_x = x;
    avatar_z = z;
}
