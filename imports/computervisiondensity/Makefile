CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS = -lncurses

# Targets
all: airport_sim biometric_app backend_server frontend_client

airport_sim: src/main.c src/msgbus.c src/agents.c
	$(CC) $(CFLAGS) -o $@ src/main.c src/msgbus.c src/agents.c

biometric_app: src/biometric_app.c src/biometric_agents.c src/msgbus.c
	$(CC) $(CFLAGS) -o $@ src/biometric_app.c src/biometric_agents.c src/msgbus.c $(LDFLAGS)

backend_server: src/backend_server.c
	$(CC) $(CFLAGS) -o $@ src/backend_server.c -pthread

frontend_client: src/frontend_client.c
	$(CC) $(CFLAGS) -o $@ src/frontend_client.c -lncurses -pthread

clean:
	rm -f airport_sim biometric_app backend_server frontend_client
