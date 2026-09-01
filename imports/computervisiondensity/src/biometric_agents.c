#define _GNU_SOURCE
#include "biometric_agents.h"
#include "msgbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_ENTRIES 256

typedef struct {
    char id[64];
    char template[128];
} bio_entry_t;

typedef struct {
    bio_entry_t entries[MAX_ENTRIES];
    int count;
    pthread_mutex_t lock;
} biometric_store_t;

/* Helper to find entry index */
static int store_find(biometric_store_t *s, const char *id) {
    for (int i = 0; i < s->count; ++i) {
        if (strcmp(s->entries[i].id, id) == 0) return i;
    }
    return -1;
}

/* Biometric agent handler */
void biometric_handler(agent_t *self, const message_t *msg) {
    biometric_store_t *store = (biometric_store_t*)self->state;
    agent_t *monitor = msgbus_find_by_name("Monitor");
    if (!store) return;

    if (msg->type == EVT_BIOMETRIC_ENROLL) {
        char pid[64];
        sscanf(msg->payload, "%63s", pid);
        pthread_mutex_lock(&store->lock);
        if (store->count >= MAX_ENTRIES) {
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "[BiometricAgent] Store full; cannot enroll %s", pid);
            msgbus_send(monitor, &m);
        } else if (store_find(store, pid) >= 0) {
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "[BiometricAgent] %s already enrolled", pid);
            msgbus_send(monitor, &m);
        } else {
            // simulate template capture by hashing id
            bio_entry_t *e = &store->entries[store->count];
            strncpy(e->id, pid, sizeof(e->id)-1);
            // simple "template"
            snprintf(e->template, sizeof(e->template), "tmpl_%08x", (unsigned)strlen(pid) ^ (unsigned)pid[0]);
            store->count++;
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "ENROLL_SUCCESS %s", pid);
            msgbus_send(monitor, &m);
        }
        pthread_mutex_unlock(&store->lock);
    } else if (msg->type == EVT_BIOMETRIC_VERIFY) {
        char pid[64];
        sscanf(msg->payload, "%63s", pid);
        pthread_mutex_lock(&store->lock);
        int idx = store_find(store, pid);
        if (idx < 0) {
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "VERIFY_FAIL %s", pid);
            msgbus_send(monitor, &m);
        } else {
            // success
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "VERIFY_SUCCESS %s", pid);
            msgbus_send(monitor, &m);
            // notify door agent to open
            agent_t *door = msgbus_find_by_name("DoorAgent");
            if (door) {
                message_t d = { .type = EVT_LOG_MESSAGE };
                snprintf(d.payload, sizeof(d.payload), "OPEN_DOOR %s", pid);
                msgbus_send(door, &d);
            }
        }
        pthread_mutex_unlock(&store->lock);
    }
}

/* Door agent: acts on OPEN_DOOR messages */
void door_handler(agent_t *self, const message_t *msg) {
    agent_t *monitor = msgbus_find_by_name("Monitor");
    if (msg->type == EVT_LOG_MESSAGE) {
        if (strncmp(msg->payload, "OPEN_DOOR", 9) == 0) {
            char tag[64], pid[64];
            sscanf(msg->payload, "%63s %63s", tag, pid);
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "[DoorAgent] Door opened for %s", pid);
            msgbus_send(monitor, &m);
            // simulate door close
            sleep(1);
            message_t c = { .type = EVT_LOG_MESSAGE };
            snprintf(c.payload, sizeof(c.payload), "[DoorAgent] Door closed for %s", pid);
            msgbus_send(monitor, &c);
        }
    }
}

agent_t *create_biometric_agent(void) {
    biometric_store_t *s = calloc(1, sizeof(biometric_store_t));
    pthread_mutex_init(&s->lock, NULL);
    agent_t *a = agent_create("BiometricAgent", biometric_handler, s, 128);
    return a;
}

agent_t *create_door_agent(void) {
    agent_t *a = agent_create("DoorAgent", door_handler, NULL, 64);
    return a;
}
