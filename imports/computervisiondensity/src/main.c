#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "msgbus.h"
#include "agents.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s data/flights.csv\n", argv[0]);
        return 1;
    }

    const char *csv = argv[1];

    msgbus_init();

    /* Setup gate state (e.g., 6 gates) */
    gate_state_t *gs = calloc(1, sizeof(gate_state_t));
    gs->gate_count = 6;
    gs->gates = calloc(gs->gate_count, sizeof(gate_t));
    for (int i=0;i<gs->gate_count;i++) {
        gs->gates[i].gate_id = i;
        gs->gates[i].occupied = 0;
        gs->gates[i].flight[0] = '\0';
    }
    pthread_mutex_init(&gs->lock, NULL);

    /* Create agents */
    agent_t *monitor = create_monitor_agent();
    agent_t *gate = create_gate_agent(gs);
    agent_t *baggage = create_baggage_agent();
    agent_t *security = create_security_agent();
    agent_t *coord = create_coordinator(gs);

    /* Start agents */
    agent_start(monitor);
    agent_start(gate);
    agent_start(baggage);
    agent_start(security);
    agent_start(coord);

    /* Read CSV and post EVT_FLIGHT_ARRIVAL events */
    FILE *f = fopen(csv, "r");
    if (!f) {
        perror("open csv");
        return 1;
    }
    char line[256];
    int sim_time = 0;
    // events are scheduled by minute offset in CSV (small simulation)
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || strlen(line) < 3) continue;
        char flight[32], airline[32];
        int arrival = 0, pax = 0, priority = 0;
        // CSV: flight_id,airline,arrival_time,min,pax,priority
        // trust simple CSV
        sscanf(line, "%31[^,],%31[^,],%d,%d,%d", flight, airline, &arrival, &pax, &priority);
        // schedule: sleep until arrival (simulated quickly: 1 second per minute)
        int delay = arrival - sim_time;
        if (delay > 0) {
            for (int i=0;i<delay;i++) {
                sleep(1); // 1 sec == 1 minute in simulation
                sim_time++;
                // occasional heartbeat
                message_t hb = { .type = EVT_NONE };
                msgbus_broadcast(&hb);
            }
        }
        // post arrival event
        message_t evt = { .type = EVT_FLIGHT_ARRIVAL };
        snprintf(evt.payload, sizeof(evt.payload), "%s %d %d %d", flight, pax, arrival, priority);
        strncpy(evt.topic, flight, sizeof(evt.topic)-1);
        msgbus_send(coord, &evt);
    }
    fclose(f);

    // let the system process for a while
    sleep(5);

    // send shutdown broadcast
    message_t sd = { .type = EVT_SHUTDOWN };
    msgbus_broadcast(&sd);

    // graceful stop
    msgbus_shutdown_all();

    // cleanup
    free(gs->gates);
    free(gs);

    printf("Simulation finished.\n");
    return 0;
}
