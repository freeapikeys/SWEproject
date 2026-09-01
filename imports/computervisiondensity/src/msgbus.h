#ifndef MSGBUS_H
#define MSGBUS_H

#include <pthread.h>

typedef enum {
    EVT_NONE = 0,
    EVT_FLIGHT_ARRIVAL,
    EVT_GATE_ASSIGN,
    EVT_GATE_RELEASE,
    EVT_BAGGAGE_DROPPED,
    EVT_BAGGAGE_ROUTED,
    EVT_SECURITY_CHECKPOINT_UPDATED,
    EVT_LOG_MESSAGE,
    EVT_BIOMETRIC_ENROLL,
    EVT_BIOMETRIC_VERIFY,
    EVT_SHUTDOWN
} evt_type_t;

typedef struct {
    evt_type_t type;
    char topic[64];
    char payload[256];
} message_t;

typedef struct agent agent_t;
typedef void (*agent_handler_fn)(agent_t *self, const message_t *msg);

struct agent {
    char name[64];
    agent_handler_fn handler;
    void *state;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    message_t *queue;
    int qsize;
    int qhead, qtail;
    int running;
    agent_t *next;
};

void msgbus_init(void);
agent_t *agent_create(const char *name, agent_handler_fn handler, void *state, int qsize);
void agent_start(agent_t *a);
void agent_stop(agent_t *a);
void msgbus_send(agent_t *dst, const message_t *msg);
agent_t *msgbus_find_by_name(const char *name);
void msgbus_broadcast(const message_t *msg);
void msgbus_shutdown_all(void);

#endif
