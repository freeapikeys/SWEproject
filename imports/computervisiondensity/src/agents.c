#define _GNU_SOURCE
#include "agents.h"
#include "msgbus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

/* Utility logging message helper */
static void emit_log(agent_t *monitor, const char *fmt, ...) {
    if (!monitor) return;
    message_t lm = { .type = EVT_LOG_MESSAGE };
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lm.payload, sizeof(lm.payload), fmt, ap);
    va_end(ap);
    strncpy(lm.topic, "log", sizeof(lm.topic)-1);
    msgbus_send(monitor, &lm);
}

/* GateAgent: manages gate allocation */
void gate_handler(agent_t *self, const message_t *msg) {
    gate_state_t *gs = (gate_state_t*)self->state;
    agent_t *monitor = msgbus_find_by_name("Monitor");
    if (!gs) return;

    if (msg->type == EVT_GATE_ASSIGN) {
        char flight[16]; int preferred = -1;
        sscanf(msg->payload, "%15s %d", flight, &preferred);
        pthread_mutex_lock(&gs->lock);
        int assigned = -1;
        if (preferred >= 0 && preferred < gs->gate_count && !gs->gates[preferred].occupied) {
            assigned = preferred;
        } else {
            for (int i=0;i<gs->gate_count;i++) {
                if (!gs->gates[i].occupied) { assigned = i; break; }
            }
        }
        if (assigned >= 0) {
            gs->gates[assigned].occupied = 1;
            strncpy(gs->gates[assigned].flight, flight, sizeof(gs->gates[assigned].flight)-1);
            pthread_mutex_unlock(&gs->lock);
            emit_log(monitor, "[GateAgent] Assigned gate %d -> flight %s", assigned, flight);
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "GATE_ASSIGNED %s %d", flight, assigned);
            strcpy(m.topic, "gate");
            msgbus_send(msgbus_find_by_name("Coordinator"), &m);
        } else {
            pthread_mutex_unlock(&gs->lock);
            emit_log(monitor, "[GateAgent] No gate available for %s", flight);
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "GATE_FULL %s", flight);
            strcpy(m.topic, "gate");
            msgbus_send(msgbus_find_by_name("Coordinator"), &m);
        }
    } else if (msg->type == EVT_GATE_RELEASE) {
        int gid;
        char flight[16];
        sscanf(msg->payload, "%15s %d", flight, &gid);
        pthread_mutex_lock(&gs->lock);
        if (gid >=0 && gid < gs->gate_count) {
            gs->gates[gid].occupied = 0;
            gs->gates[gid].flight[0] = '\0';
            pthread_mutex_unlock(&gs->lock);
            emit_log(monitor, "[GateAgent] Released gate %d from %s", gid, flight);
        } else {
            pthread_mutex_unlock(&gs->lock);
            emit_log(monitor, "[GateAgent] Release request invalid gate %d", gid);
        }
    }
}

void baggage_handler(agent_t *self, const message_t *msg) {
    agent_t *monitor = msgbus_find_by_name("Monitor");
    if (msg->type == EVT_BAGGAGE_DROPPED) {
        char flight[16]; int bags;
        sscanf(msg->payload, "%15s %d", flight, &bags);
        emit_log(monitor, "[BaggageAgent] Received %d bags for %s. Routing...", bags, flight);
        int sum = 0;
        for (int i=0; flight[i]; ++i) sum += flight[i];
        int carousel = (sum % 4) + 1;
        sleep(1);
        message_t routed = { .type = EVT_BAGGAGE_ROUTED };
        snprintf(routed.payload, sizeof(routed.payload), "%s %d %d", flight, bags, carousel);
        msgbus_send(monitor, &routed);
    }
}

void security_handler(agent_t *self, const message_t *msg) {
    agent_t *monitor = msgbus_find_by_name("Monitor");
    static int lanes = 2;
    if (msg->type == EVT_FLIGHT_ARRIVAL) {
        char flight[16]; int pax; int arrival;
        sscanf(msg->payload, "%15s %d %d", flight, &pax, &arrival);
        int needed = (pax + 74) / 75;
        if (needed < 1) needed = 1;
        if (needed > 6) needed = 6;
        if (needed != lanes) {
            lanes = needed;
            message_t m = { .type = EVT_SECURITY_CHECKPOINT_UPDATED };
            snprintf(m.payload, sizeof(m.payload), "lanes=%d flight=%s", lanes, flight);
            msgbus_send(monitor, &m);
        } else {
            message_t m = { .type = EVT_LOG_MESSAGE };
            snprintf(m.payload, sizeof(m.payload), "[SecurityAgent] Lanes remain %d for %s", lanes, flight);
            msgbus_send(monitor, &m);
        }
    }
}

void monitor_handler(agent_t *self, const message_t *msg) {
    static int tick = 0;
    if (msg->type == EVT_LOG_MESSAGE) {
        printf("%s\n", msg->payload);
    } else if (msg->type == EVT_BAGGAGE_ROUTED) {
        printf("[Monitor] Baggage Routed -> %s\n", msg->payload);
    } else if (msg->type == EVT_SECURITY_CHECKPOINT_UPDATED) {
        printf("[Monitor] Security update -> %s\n", msg->payload);
    } else if (msg->type == EVT_SHUTDOWN) {
        printf("[Monitor] Shutdown requested\n");
    } else {
        tick++;
        if ((tick % 50) == 0) printf("[Monitor] heartbeat %d\n", tick);
    }
}

void coordinator_handler(agent_t *self, const message_t *msg) {
    agent_t *gate = msgbus_find_by_name("GateAgent");
    agent_t *baggage = msgbus_find_by_name("BaggageAgent");
    agent_t *security = msgbus_find_by_name("SecurityAgent");
    agent_t *monitor = msgbus_find_by_name("Monitor");

    if (msg->type == EVT_FLIGHT_ARRIVAL) {
        char flight[16]; int pax; int arrival; int priority=0;
        sscanf(msg->payload, "%15s %d %d %d", flight, &pax, &arrival, &priority);
        emit_log(monitor, "[Coordinator] Flight %s arrived: pax=%d priority=%d", flight, pax, priority);
        message_t secmsg = { .type = EVT_FLIGHT_ARRIVAL };
        snprintf(secmsg.payload, sizeof(secmsg.payload), "%s %d %d", flight, pax, arrival);
        msgbus_send(security, &secmsg);
        int est_bags = pax / 2;
        if (est_bags > 0) {
            message_t b = { .type = EVT_BAGGAGE_DROPPED };
            snprintf(b.payload, sizeof(b.payload), "%s %d", flight, est_bags);
            msgbus_send(baggage, &b);
        }
        int preferred = -1;
        if (priority == 1) preferred = 0;
        message_t g = { .type = EVT_GATE_ASSIGN };
        snprintf(g.payload, sizeof(g.payload), "%s %d", flight, preferred);
        msgbus_send(gate, &g);
    } else if (msg->type == EVT_LOG_MESSAGE) {
        if (strncmp(msg->payload, "GATE_ASSIGNED", 13) == 0) {
            char tag[32], flight[16]; int gid;
            sscanf(msg->payload, "%31s %15s %d", tag, flight, &gid);
            emit_log(monitor, "[Coordinator] Noted gate %d for %s", gid, flight);
            sleep(2);
            message_t r = { .type = EVT_GATE_RELEASE };
            snprintf(r.payload, sizeof(r.payload), "%s %d", flight, gid);
            msgbus_send(msgbus_find_by_name("GateAgent"), &r);
        } else if (strncmp(msg->payload, "GATE_FULL", 9) == 0) {
            char tag[32], flight[16];
            sscanf(msg->payload, "%31s %15s", tag, flight);
            emit_log(monitor, "[Coordinator] Gate full for %s; attempting reschedule", flight);
            sleep(1);
            message_t g = { .type = EVT_GATE_ASSIGN };
            snprintf(g.payload, sizeof(g.payload), "%s %d", flight, -1);
            msgbus_send(msgbus_find_by_name("GateAgent"), &g);
        } else {
            emit_log(monitor, "[Coordinator] log: %s", msg->payload);
        }
    }
}

agent_t *create_coordinator(gate_state_t *gate_state) { return agent_create("Coordinator", coordinator_handler, gate_state, 128); }
agent_t *create_gate_agent(gate_state_t *gate_state) { return agent_create("GateAgent", gate_handler, gate_state, 128); }
agent_t *create_baggage_agent(void) { return agent_create("BaggageAgent", baggage_handler, NULL, 64); }
agent_t *create_security_agent(void) { return agent_create("SecurityAgent", security_handler, NULL, 64); }
agent_t *create_monitor_agent(void) { return agent_create("Monitor", monitor_handler, NULL, 256); }
