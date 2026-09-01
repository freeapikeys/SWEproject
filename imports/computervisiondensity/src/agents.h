#ifndef AGENTS_H
#define AGENTS_H

#include "msgbus.h"

typedef struct {
    int gate_id;
    int occupied;
    char flight[16];
} gate_t;

typedef struct {
    gate_t *gates;
    int gate_count;
    pthread_mutex_t lock;
} gate_state_t;

void coordinator_handler(agent_t *self, const message_t *msg);
void gate_handler(agent_t *self, const message_t *msg);
void baggage_handler(agent_t *self, const message_t *msg);
void security_handler(agent_t *self, const message_t *msg);
void monitor_handler(agent_t *self, const message_t *msg);

agent_t *create_coordinator(gate_state_t *gate_state);
agent_t *create_gate_agent(gate_state_t *gate_state);
agent_t *create_baggage_agent(void);
agent_t *create_security_agent(void);
agent_t *create_monitor_agent(void);

#endif
