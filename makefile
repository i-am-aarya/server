CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2 -D_GNU_SOURCE
LDFLAGS = -pthread

SRCS = main.c server.c queue.c thread_pool.c http.c io.c utils.c
OBJS = $(SRCS:.c=.o)
TARGET = server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) *.o

test_queue: tests/test_queue.c queue.c utils.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	./$@

bench_queue: tests/bench_queue.c queue.c utils.c
	$(CC) $(CFLAGS) -O3 -o $@ $^ $(LDFLAGS)
	./$@
