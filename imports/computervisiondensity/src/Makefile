CC=gcc
CFLAGS=-Wall -O2
LDFLAGS=-lraylib -lm -lpthread -ldl -lrt
TARGET=trolley_app
SRC=main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
