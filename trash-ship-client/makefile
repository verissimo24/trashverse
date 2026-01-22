CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -O2 -g -pthread
SDL_C   := $(shell sdl2-config --cflags)
SDL_L   := $(shell sdl2-config --libs)
LDLIBS  := $(SDL_L) -lSDL2_gfx -lzmq -lprotobuf-c -lconfig -lm -pthread

COMMON      := ..
PROTO_NAME  := space_trash.proto
PROTO       := $(COMMON)/$(PROTO_NAME)
PB_C        := $(COMMON)/space_trash.pb-c.c
PB_H        := $(COMMON)/space_trash.pb-c.h


BIN     := new-universe-client

OBJS := new-universe-client.o universe-data.o display.o comm.o space_trash.pb-c.o

.PHONY: all clean proto

all: proto $(BIN)

proto: $(PB_C) $(PB_H)

$(PB_C) $(PB_H): $(PROTO)
	protoc-c --proto_path=$(COMMON) --c_out=$(COMMON) $(PROTO_NAME)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(SDL_C) -o $@ $^ $(LDLIBS)

new-universe-client.o: new-universe-client.c
	$(CC) $(CFLAGS) $(SDL_C) -I. -I$(COMMON) -c $< -o $@

universe-data.o: $(COMMON)/universe-data.c $(COMMON)/universe-data.h
	$(CC) $(CFLAGS) $(SDL_C) -I. -I$(COMMON) -c $< -o $@

display.o: $(COMMON)/display.c $(COMMON)/display.h
	$(CC) $(CFLAGS) $(SDL_C) -I. -I$(COMMON) -c $< -o $@

comm.o: $(COMMON)/comm.c $(COMMON)/comm.h
	$(CC) $(CFLAGS) $(SDL_C) -I. -I$(COMMON) -c $< -o $@

space_trash.pb-c.o: $(PB_C) $(PB_H)
	$(CC) $(CFLAGS) $(SDL_C) -I$(COMMON) -c $(PB_C) -o $@

clean:
	rm -f $(OBJS) $(BIN)
