# Makefile
CC = gcc
# Compiler flags: all warnings + debugger meta-data
CFLAGS = -Wall -g -m64 -Wall -D_REENTRANT
INCLUDES=-Iinclude -I/opt/DIS/include -I/opt/DIS/src/include -I/opt/DIS/include/dis 

#OS Specific?
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
# External libraries: 
LIBS = -Wl,-L/opt/DIS/$(LIB_DIRECTORY),-rpath,/opt/DIS/$(LIB_DIRECTORY),-lsisci -lSDL2 -lavcodec -lavformat -lavutil -lpthread 

# Pre-defined macros for conditional compilation
DEFS = -DDEBUG_FLAG -DEXPERIMENTAL=0

# The final executable program file, i.e. name of our program
BIN = server
BIN2 = client

# Object files from which $BIN depends
SERVER_OBJS = process_video.o videoplayer.o
CLIENT_OBJS = videoplayer.o

$(BIN): $(SERVER_OBJS) $(BIN).c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) $(SERVER_OBJS) $(BIN).c $(LIBS) -o $(BIN)

$(BIN2): $(CLIENT_OBJS) $(BIN2).c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) $(CLIENT_OBJS) $(BIN2).c $(LIBS) -o $(BIN2)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $(DEFS) $(INCLUDES) $< -o $@

clean:
	rm -f *~ *.o $(BIN)

depend:
	makedepend -Y -- $(CFLAGS) $(DEFS) -- *.c