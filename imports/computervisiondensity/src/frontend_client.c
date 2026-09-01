#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <arpa/inet.h>

#define SERVER_PORT 5555
#define SERVER_ADDR "127.0.0.1"

static int sockfd = -1;
static pthread_t recv_thread;

#define LOG_LINES 100
#define LOG_WIDTH 256

typedef struct {
    char lines[LOG_LINES][LOG_WIDTH];
    int head;
    int count;
    pthread_mutex_t lock;
} log_buffer_t;

static log_buffer_t lb;

static void lb_put(const char *s) {
    pthread_mutex_lock(&lb.lock);
    int idx = (lb.head + lb.count) % LOG_LINES;
    if (lb.count == LOG_LINES) { lb.head = (lb.head + 1) % LOG_LINES; idx = (lb.head + lb.count -1) % LOG_LINES; }
    strncpy(lb.lines[idx], s, LOG_WIDTH-1);
    lb.lines[idx][LOG_WIDTH-1] = '\0';
    if (lb.count < LOG_LINES) lb.count++;
    pthread_mutex_unlock(&lb.lock);
}

static void *recv_loop(void *arg) {
    char buf[512]; ssize_t n;
    while ((n = recv(sockfd, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        lb_put(buf);
    }
    return NULL;
}

static int connect_server(void) {
    struct sockaddr_in serv;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;
    serv.sin_family = AF_INET; serv.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_ADDR, &serv.sin_addr);
    if (connect(sockfd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
        close(sockfd); sockfd = -1; return -1;
    }
    pthread_create(&recv_thread, NULL, recv_loop, NULL);
    return 0;
}

static void draw_node(int y, int x, int h, int w, const char *title) {
    mvaddch(y, x, '+'); mvaddch(y+h, x, '+'); mvaddch(y, x+w, '+'); mvaddch(y+h, x+w, '+');
    for (int i=1;i<w;i++) { mvaddch(y, x+i, '-'); mvaddch(y+h, x+i, '-'); }
    for (int i=1;i<h;i++) { mvaddch(y+i, x, '|'); mvaddch(y+i, x+w, '|'); }
    mvprintw(y + h/2, x + 2, "%s", title);
}

int main(void) {
    lb.head = 0; lb.count = 0; pthread_mutex_init(&lb.lock, NULL);

    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); nodelay(stdscr, TRUE);

    if (connect_server() < 0) {
        lb_put("[client] Could not connect to backend (is backend_server running?)");
    } else {
        lb_put("[client] Connected to backend");
    }

    int running = 1; int ch;
    while (running) {
        clear(); int rows, cols; getmaxyx(stdscr, rows, cols);
        mvprintw(1, (cols/2)-12, "Frontend Client - Biometric UI");
        draw_node(3, 2, 3, 18, "Frontend UI");
        draw_node(3, cols/2 - 10, 3, 20, "Backend Service");
        draw_node(10, cols/2 - 12, 3, 24, "Logs");
        mvprintw(rows-4, 2, "[E] Enroll   [V] Verify   [L] Clear logs   [Q] Quit");

        int log_y = 11; int max_display = rows - log_y - 6; if (max_display > 8) max_display = 8;
        pthread_mutex_lock(&lb.lock);
        int start = lb.head;
        for (int i=0;i< (lb.count < max_display ? lb.count : max_display); ++i) {
            int idx = (start + lb.count - ( (lb.count < max_display)? lb.count : max_display) + i) % LOG_LINES;
            mvprintw(log_y + i, cols/2 - 10, "%-50s", lb.lines[idx]);
        }
        pthread_mutex_unlock(&lb.lock);

        refresh();
        ch = getch();
        if (ch == 'q' || ch == 'Q') { running = 0; break; }
        else if (ch == 'e' || ch == 'E') {
            echo(); nodelay(stdscr, FALSE); char pid[64]; mvprintw(rows-2, 2, "Enroll Passenger ID: "); getnstr(pid, sizeof(pid)-1);
            nodelay(stdscr, TRUE); noecho(); if (sockfd >= 0) { char cmd[256]; snprintf(cmd, sizeof(cmd), "ENROLL %s\n", pid); send(sockfd, cmd, strlen(cmd), 0); }
            else lb_put("[client] Not connected");
        } else if (ch == 'v' || ch == 'V') {
            echo(); nodelay(stdscr, FALSE); char pid[64]; mvprintw(rows-2, 2, "Verify Passenger ID: "); getnstr(pid, sizeof(pid)-1);
            nodelay(stdscr, TRUE); noecho(); if (sockfd >= 0) { char cmd[256]; snprintf(cmd, sizeof(cmd), "VERIFY %s\n", pid); send(sockfd, cmd, strlen(cmd), 0); }
            else lb_put("[client] Not connected");
        } else if (ch == 'l' || ch == 'L') {
            pthread_mutex_lock(&lb.lock); lb.head = 0; lb.count = 0; pthread_mutex_unlock(&lb.lock);
        }

        usleep(100000);
    }

    if (sockfd >= 0) close(sockfd);
    endwin(); return 0;
}
