CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

TARGET = appendlog
OBJS = appendlog.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

appendlog.o: appendlog.c
	$(CC) $(CFLAGS) -c appendlog.c

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean