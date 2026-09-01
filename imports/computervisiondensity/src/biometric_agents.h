#ifndef BIOMETRIC_AGENTS_H
#define BIOMETRIC_AGENTS_H

#include "msgbus.h"

/* create biometric agent that handles enroll/verify */
agent_t *create_biometric_agent(void);
/* create door agent to act on verify results */
agent_t *create_door_agent(void);

#endif
