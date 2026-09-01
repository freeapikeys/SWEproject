#define _GNU_SOURCE
#include "msgbus.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static agent_t *agents_head = NULL;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

void msgbus_init(void) { agents_head = NULL; }

agent_t *agent_create(const char *name, agent_handler_fn handler, void *state, int qsize) {
    agent_t *a = calloc(1, sizeof(agent_t));
    strncpy(a->name, name, sizeof(a->name)-1);
    a->handler = handler;
    a->state = state;
    a->qsize = (qsize>0? qsize: 64);
    a->queue = calloc(a->qsize, sizeof(message_t));
    a->qhead = a->qtail = 0;
    a->running = 0;
    pthread_mutex_init(&a->lock, NULL);
    pthread_cond_init(&a->cond, NULL);
    pthread_mutex_lock(&registry_lock);
    a->next = agents_head;
    agents_head = a;
    pthread_mutex_unlock(&registry_lock);
    return a;
}

agent_t *msgbus_find_by_name(const char *name) {
    pthread_mutex_lock(&registry_lock);
    agent_t *it = agents_head;
    while (it) {
        if (strcmp(it->name, name) == 0) { pthread_mutex_unlock(&registry_lock); return it; }
        it = it->next;
    }
    pthread_mutex_unlock(&registry_lock);
    return NULL;
}

static int enqueue(agent_t *a, const message_t *m) {
    pthread_mutex_lock(&a->lock);
    int next = (a->qtail + 1) % a->qsize;
    if (next == a->qhead) a->qhead = (a->qhead + 1) % a->qsize;
    a->queue[a->qtail] = *m;
    a->qtail = next;
    pthread_cond_signal(&a->cond);
    pthread_mutex_unlock(&a->lock);
    return 0;
}

void msgbus_send(agent_t *dst, const message_t *msg) { if (dst) enqueue(dst, msg); }

void msgbus_broadcast(const message_t *msg) {
    pthread_mutex_lock(&registry_lock);
    agent_t *it = agents_head;
    while (it) { enqueue(it, msg); it = it->next; }
    pthread_mutex_unlock(&registry_lock);
}

static void *agent_loop(void *arg) {
    agent_t *a = (agent_t*)arg;
    a->running = 1;
    while (a->running) {
        pthread_mutex_lock(&a->lock);
        while (a->qhead == a->qtail && a->running) pthread_cond_wait(&a->cond, &a->lock);
        if (!a->running) { pthread_mutex_unlock(&a->lock); break; }
        message_t msg = a->queue[a->qhead];
        a->qhead = (a->qhead + 1) % a->qsize;
        pthread_mutex_unlock(&a->lock);
        if (a->handler) a->handler(a, &msg);
        usleep(1000);
    }
    return NULL;
}

void agent_start(agent_t *a) { pthread_create(&a->thread, NULL, agent_loop, a); }

void agent_stop(agent_t *a) {
    if (!a) return;
    pthread_mutex_lock(&a->lock);
    a->running = 0;
    pthread_cond_signal(&a->cond);
    pthread_mutex_unlock(&a->lock);
    pthread_join(a->thread, NULL);
}

void msgbus_shutdown_all(void) {
    pthread_mutex_lock(&registry_lock);
    agent_t *it = agents_head;
    while (it) { agent_stop(it); it = it->next; }
    pthread_mutex_unlock(&registry_lock);
}
