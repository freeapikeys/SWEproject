#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 5555
#define BACKLOG 8
#define MAX_ENTRIES 1024

typedef struct { char id[128]; char tmpl[128]; } entry_t;
typedef struct { entry_t entries[MAX_ENTRIES]; int count; pthread_mutex_t lock; } store_t;
static store_t store;

static void store_init(store_t *s) { s->count = 0; pthread_mutex_init(&s->lock, NULL); }
static int store_find(store_t *s, const char *id) {
    for (int i = 0; i < s->count; ++i) if (strcmp(s->entries[i].id, id) == 0) return i;
    return -1;
}
static int handle_enroll(store_t *s, const char *id, char *resp, size_t rlen) {
    pthread_mutex_lock(&s->lock);
    if (store_find(s, id) >= 0) { snprintf(resp, rlen, "ENROLL_EXISTS %s\n", id); pthread_mutex_unlock(&s->lock); return 0; }
    if (s->count >= MAX_ENTRIES) { snprintf(resp, rlen, "ENROLL_FULL\n"); pthread_mutex_unlock(&s->lock); return 0; }
    entry_t *e = &s->entries[s->count++];
    strncpy(e->id, id, sizeof(e->id)-1);
    snprintf(e->tmpl, sizeof(e->tmpl), "tmpl_%08x", (unsigned)strlen(id) ^ (unsigned)id[0]);
    snprintf(resp, rlen, "ENROLL_OK %s\n", id);
    pthread_mutex_unlock(&s->lock);
    return 1;
}
static int handle_verify(store_t *s, const char *id, char *resp, size_t rlen) {
    pthread_mutex_lock(&s->lock);
    int idx = store_find(s, id);
    if (idx < 0) { snprintf(resp, rlen, "VERIFY_FAIL %s\n", id); pthread_mutex_unlock(&s->lock); return 0; }
    snprintf(resp, rlen, "VERIFY_OK %s\n", id);
    pthread_mutex_unlock(&s->lock);
    return 1;
}
static void *client_thread(void *arg) {
    int cfd = *(int*)arg; free(arg);
    char buf[512]; ssize_t n; char resp[256];
    while ((n = recv(cfd, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = '\0';
        char cmd[64], arg1[128];
        if (sscanf(buf, "%63s %127s", cmd, arg1) >= 1) {
            if (strcasecmp(cmd, "ENROLL") == 0) { handle_enroll(&store, arg1, resp, sizeof(resp)); send(cfd, resp, strlen(resp), 0); }
            else if (strcasecmp(cmd, "VERIFY") == 0) { handle_verify(&store, arg1, resp, sizeof(resp)); send(cfd, resp, strlen(resp), 0); }
            else if (strcasecmp(cmd, "PING") == 0) send(cfd, "PONG\n", 5, 0);
            else if (strcasecmp(cmd, "SHUTDOWN") == 0) { send(cfd, "SHUTTING_DOWN\n", 14, 0); break; }
            else send(cfd, "UNKNOWN_COMMAND\n", 16, 0);
        }
    }
    close(cfd); return NULL;
}
int main(void) {
    store_init(&store);
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); return 1; }
    int opt = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(PORT);
    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(sfd, BACKLOG) < 0) { perror("listen"); return 1; }
    printf("Backend server listening on port %d\n", PORT);
    while (1) {
        struct sockaddr_in caddr; socklen_t clen = sizeof(caddr);
        int *cfdp = malloc(sizeof(int)); *cfdp = accept(sfd, (struct sockaddr*)&caddr, &clen);
        if (*cfdp < 0) { free(cfdp); perror("accept"); continue; }
        pthread_t t; pthread_create(&t, NULL, client_thread, cfdp); pthread_detach(t);
    }
    close(sfd); return 0;
}
