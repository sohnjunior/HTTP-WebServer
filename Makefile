OBJS = srv.c ls.c linkedlist.c
CC = gcc
EXEC = semaphore_server_40309

all: $(OBJS)
		$(CC) -o $(EXEC) $^ -lpthread

clean:
		rm -rf $(EXEC)
