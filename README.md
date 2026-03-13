# Trashverse

A multiplayer **client-server space cleanup game** written in **C**.

In Trashverse, players control trash ships in a 2D universe filled with planets and drifting space trash. Trash and ships are affected by gravity and inertia, planets are stationary, and one planet acts as the current recycling point. Players collect roaming trash, avoid losing cargo on non-recycling planets, and try to keep the universe from collapsing under excessive trash.

## Project Overview

This project implements a simplified distributed game world. The system is split into two main applications:

- **universe-server** — simulates the universe, applies physics, manages ships/trash/planets, and publishes the current state.
- **trash-ship-client** — connects to the server, receives universe snapshots, displays the game, and sends ship movement commands.

The server and client communicate using:

- **ZeroMQ** for networking
- **Protocol Buffers (protobuf-c)** for message serialization
- **SDL2** for rendering and keyboard input
- **libconfig** for configuration files

## Main Features

- 2D universe with configurable dimensions
- Randomly generated planets and trash
- Gravitational motion with inertia and friction
- Edge wrap-around / teleportation at universe boundaries
- Multiplayer ship support
- Trash collection by ship proximity
- Cargo deposit on the recycling planet
- Cargo spill when colliding with a non-recycling planet
- Server-published universe snapshots for clients
- Configuration-based setup for ports, universe size, trash limits, and ship capacity

## Architecture

The repository is organized into:

```text
trashverse/
├── trash-ship-client/
│   ├── new-universe-client.c
│   ├── client.cfg
│   └── makefile
├── universe-server/
│   ├── universe-server.c
│   ├── physics-rules.c
│   ├── physics-rules.h
│   ├── server.cfg
│   └── makefile
├── comm.c / comm.h
├── display.c / display.h
├── universe-data.c / universe-data.h
├── space_trash.proto
└── direction.h
```

### Shared modules

- **universe-data**: core game data structures and universe state management
- **physics-rules**: gravitational physics, velocity update, and position update
- **comm**: ZeroMQ channels and protobuf-based communication helpers
- **display**: SDL-based visualization of planets, ships, and trash
- **space_trash.proto**: protocol definition for client messages and server snapshots

## Game Rules Implemented

The project follows the following core mechanics:

- planets are fixed in place
- trash and ships are affected by gravity
- velocity is damped by friction over time
- objects wrap around when crossing universe boundaries
- ships collect trash by contact
- ships can deposit cargo on the recycling planet
- colliding with a non-recycling planet spills cargo back into the universe
- the universe has a maximum trash limit

## Technologies Used

- **C11**
- **SDL2** and **SDL2_gfx**
- **ZeroMQ**
- **protobuf-c**
- **libconfig**
- **Makefiles**

## Build

### Requirements

Make sure the following dependencies are installed:

- `gcc`
- `make`
- `SDL2`
- `SDL2_gfx`
- `ZeroMQ`
- `protobuf-c`
- `protoc-c`
- `libconfig`

### Compile the server

```bash
cd universe-server
make
```

### Compile the client

```bash
cd trash-ship-client
make
```

Both makefiles generate protobuf C bindings from `space_trash.proto` before building.

## Run

### Start the server

```bash
cd universe-server
./universe-server
```

### Start a client

```bash
cd trash-ship-client
./new-universe-client
```

You can launch multiple clients to connect multiple ships to the same server.

## Configuration

The project uses configuration files instead of hardcoded values.

### `universe-server/server.cfg`
Controls:

- request/reply port
- publish/subscribe port
- universe width and height
- number of planets
- maximum trash
- initial trash
- ship cargo capacity

Example:

```cfg
reqrep_port = 5555;
pub_port = 5556;
width = 1000;
height = 800;
num_planets = 3;
max_trash = 500;
initial_trash = 10;
ship_capacity = 5;
```

### `trash-ship-client/client.cfg`
Controls:

- server address
- request/reply port
- publish/subscribe port
- client display width and height

Example:

```cfg
server_address = "127.0.0.1";
reqrep_port = 5555;
pub_port = 5556;
width = 1000;
height = 800;
```

## Communication Model

The project uses two communication patterns:

- **REQ/REP** for control messages such as connect, move, and quit
- **PUB/SUB** for continuous server state updates sent to all clients

Client messages include:

- `CONNECT`
- `MOVE`
- `QUIT`

The protocol also defines serialized state snapshots for:

- planets
- trash
- ships
- overall universe statistics

## Controls

Use the **arrow keys** to move the ship.

- `↑` move up
- `↓` move down
- `←` move left
- `→` move right

## What I Worked On

This project gave me hands-on experience with:

- low-level programming in C
- modular software design
- client-server communication
- serialization with Protocol Buffers
- graphical visualization with SDL2
- simulation of physics-based movement
- managing shared state in a multiplayer system

## Notes

This repository may still be extended with improvements such as:

- clearer in-game HUD / score display
- better error handling and logging
- more gameplay mechanics
- personalized themes

