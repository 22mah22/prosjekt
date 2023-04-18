CC = gcc
CFLAGS=-g -m64 -Wall
PTHREAD_CFLAGS=-D_REENTRANT 
INCLUDES=-Iinclude -I/opt/DIS/include -I/opt/DIS/src/include -I/opt/DIS/include/dis


SERVER_SRCS=server.c
CLIENT_SRCS=client.c
SERVER_OBJS=$(SERVER_SRCS:%.c=%.o)
CLIENT_OBJS=$(CLIENT_SRCS:%.c=%.o)

#Library stuff?
BITS=$(shell sh -c 'getconf LONG_BIT || echo NA')

ifeq ($(BITS),NA)
LIB_DIRECTORY=lib
endif

ifeq ($(BITS),32)
LIB_DIRECTORY=lib
endif

ifeq ($(BITS),64)
LIB_DIRECTORY=lib64
endif

LD=$(CC)
LFLAGS=-Wl,-L/opt/DIS/$(LIB_DIRECTORY),-rpath,/opt/DIS/$(LIB_DIRECTORY),-lsisci

#End of library stuff

all: server client

#.PHONY: all clean executables 

server: $(SERVER_OBJS)
	$(CC) $(INCLUDES) $(PTHREAD_CFLAGS) $^ -o $@ $(LFLAGS)

client: $(CLIENT_OBJS)
	$(CC) $(INCLUDES) $(PTHREAD_CFLAGS) $^ -o $@ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(PTHREAD_CFLAGS)  -c $< -o $@

%: %.o
	$(LD) $^ -o $@ $(LFLAGS)


clean:
	rm -f $(SERVER_OBJS) $(CLIENT_OBJS) server client