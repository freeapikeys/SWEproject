#define _GNU_SOURCE
#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "msgbus.h"

#define LOG_LINES 100
#define LOG_WIDTH 256

typedef struct {
    char lines[LOG_LINES][LOG_WIDTH];
    int head;
    int count;
    pthread_mutex_t lock;
} log_buffer_t;

/* MonitorUI handler: stores logs into shared buffer */
void monitor_ui_handler(agent_t *self, const message_t *msg) {
    log_buffer_t *lb = (log_buffer_t*)self->state;
    if (!lb) return;
    if (msg->type == EVT_LOG_MESSAGE || msg->type == EVT_BAGGAGE_ROUTED || msg->type == EVT_SECURITY_CHECKPOINT_UPDATED) {
        pthread_mutex_lock(&lb->lock);
        int idx = (lb->head + lb->count) % LOG_LINES;
        if (lb->count == LOG_LINES) {
            // overwrite oldest
            lb->head = (lb->head + 1) % LOG_LINES;
            idx = (lb->head + lb->count - 1) % LOG_LINES;
        } else {
            lb->count++;
        }
        strncpy(lb->lines[idx], msg->payload, LOG_WIDTH-1);
        lb->lines[idx][LOG_WIDTH-1] = '\0';
        pthread_mutex_unlock(&lb->lock);
    }
}

/* draw a box with title */
static void draw_node(int y, int x, int h, int w, const char *title) {
    mvaddch(y, x, '+');
    mvaddch(y+h, x, '+');
    mvaddch(y, x+w, '+');
    mvaddch(y+h, x+w, '+');
    for (int i=1;i<w;i++) { mvaddch(y, x+i, '-'); mvaddch(y+h, x+i, '-'); }
    for (int i=1;i<h;i++) { mvaddch(y+i, x, '|'); mvaddch(y+i, x+w, '|'); }
    mvprintw(y + h/2, x + 2, "%s", title);
}

static void draw_edge(int y1, int x1, int y2, int x2) {
    // simple straight-line connection
    int xm = (x1 + x2) / 2;
    for (int x = x1+6; x <= x2-1; ++x) mvaddch(y1, x, '-');
    mvaddch(y1, x2-1, '>');
}

int main(void) {
    msgbus_init();

    /* Setup log buffer and MonitorUI */
    log_buffer_t *lb = calloc(1, sizeof(log_buffer_t));
    pthread_mutex_init(&lb->lock, NULL);
    agent_t *monitor = agent_create("Monitor", monitor_ui_handler, lb, 256);

    /* Create biometric and door agents */
    extern agent_t *create_biometric_agent(void);
    extern agent_t *create_door_agent(void);
    agent_t *bio = create_biometric_agent();
    agent_t *door = create_door_agent();

    agent_start(monitor);
    agent_start(bio);
    agent_start(door);

    /* NCurses UI */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    int running = 1;
    int ch;

    while (running) {
        clear();
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        // Draw title
        mvprintw(1, (cols/2)-12, "Mauritius Airport - Biometric Graph");

        // Draw nodes
        draw_node(3, 2, 3, 18, "Coordinator");
        draw_node(3, cols/2 - 10, 3, 20, "BiometricAgent");
        draw_node(3, cols - 22, 3, 18, "DoorAgent");
        draw_node(10, cols/2 - 12, 3, 24, "Monitor (Logs)");

        // Edges
        draw_edge(5, 2, 5, cols/2 - 10);
        draw_edge(5, cols/2 - 10, 5, cols - 22);
        draw_edge(7, cols/2 - 10, 11, cols/2 - 10);

        // Buttons
        mvprintw(rows-4, 2, "[E] Enroll   [V] Verify   [L] Clear logs   [Q] Quit");

        // Display logs in Monitor box area
        int log_y = 11;
        int max_display = rows - log_y - 6;
        if (max_display > 8) max_display = 8;
        pthread_mutex_lock(&lb->lock);
        int start = lb->head;
        for (int i=0;i< (lb->count < max_display ? lb->count : max_display); ++i) {
            int idx = (start + lb->count - ( (lb->count < max_display)? lb->count : max_display) + i) % LOG_LINES;
            mvprintw(log_y + i, cols/2 - 10, "%-50s", lb->lines[idx]);
        }
        pthread_mutex_unlock(&lb->lock);

        refresh();

        ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = 0;
            break;
        } else if (ch == 'e' || ch == 'E') {
            // prompt for id
            echo();
            nodelay(stdscr, FALSE);
            char pid[64];
            mvprintw(rows-2, 2, "Enroll Passenger ID: ");
            getnstr(pid, sizeof(pid)-1);
            nodelay(stdscr, TRUE);
            noecho();
            // send enroll message
            message_t m = { .type = EVT_BIOMETRIC_ENROLL };
            snprintf(m.payload, sizeof(m.payload), "%s", pid);
            msgbus_send(bio, &m);
        } else if (ch == 'v' || ch == 'V') {
            echo();
            nodelay(stdscr, FALSE);
            char pid[64];
            mvprintw(rows-2, 2, "Verify Passenger ID: ");
            getnstr(pid, sizeof(pid)-1);
            nodelay(stdscr, TRUE);
            noecho();
            message_t m = { .type = EVT_BIOMETRIC_VERIFY };
            snprintf(m.payload, sizeof(m.payload), "%s", pid);
            msgbus_send(bio, &m);
        } else if (ch == 'l' || ch == 'L') {
            pthread_mutex_lock(&lb->lock);
            lb->head = 0; lb->count = 0;
            pthread_mutex_unlock(&lb->lock);
        }

        usleep(100000);
    }

    // Shutdown
    message_t sd = { .type = EVT_SHUTDOWN };
    msgbus_broadcast(&sd);
    msgbus_shutdown_all();

    endwin();

    // cleanup
    free(lb);

    return 0;
}
