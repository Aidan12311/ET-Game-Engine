# ET Game Engine

> **Status: tentative overview.** This repository is a trimmed-down showcase of
> the game engine from a larger team project. Details below describe the current
> state of the code and may change.

A small server-side engine for a game inspired by *E.T. the Extra-Terrestrial*
on the Atari 2600. The player guides E.T. through a forest and a series of pits
to collect the three pieces of a phone (antenna, dial and speaker) before E.T.
runs out of energy. Candy (Reese's Pieces) can be picked up and eaten to restore
energy.

The engine is written in modern C++20. It owns all game rules and state, runs a
fixed-timestep simulation, and talks to a client over a WebSocket using a small
JSON protocol.

## Background

This started as a team microservice project:

| Service        | Stack                       | Role                                                        |
|----------------|-----------------------------|-------------------------------------------------------------|
| `game_service` | C++20                       | Authoritative game engine (**the part kept here**)          |
| `auth_service` | Python / FastAPI / MongoDB  | Users and save storage, consuming save events from RabbitMQ |
| `gui_service`  | C++ / Vulkan                | 3D client that rendered the world and sent player input     |

Traefik acted as the API gateway, and RabbitMQ carried save events from the
engine to the auth service.

This repository keeps only the engine. The other services, the gateway and the
message broker have been removed. The engine's links to them were replaced with
local equivalents, so it builds and runs **with no other service present**:

| Originally                                      | Now                                    |
|-------------------------------------------------|----------------------------------------|
| Load save over HTTP from `auth_service`         | Read `saves/<userId>.json` from disk   |
| Publish save events to RabbitMQ                 | Write `saves/<userId>.json` atomically |
| Client = Vulkan GUI behind Traefik              | Any WebSocket client on port `8001`    |

## Repository layout

```
.
├── docker-compose.yml          # Runs the engine container on its own
└── services/
    ├── game_service/           # The engine
    │   ├── CMakeLists.txt
    │   ├── Dockerfile
    │   ├── main.cpp            # Wiring: load world, start threads, run loop
    │   ├── models/             # Plain data types: World, Room, Tileset, Vec2, ...
    │   ├── engine/
    │   │   ├── world/          # Load, parse and validate world + tileset JSON
    │   │   ├── state/          # GameState (player, energy, inventory, status)
    │   │   ├── simulation/     # Movement, collision, room exits, pickups, win/lose
    │   │   ├── session/        # Fixed-timestep game loop and connect/disconnect
    │   │   ├── events/         # Input event types and dispatch to handlers
    │   │   ├── protocol/       # Outgoing JSON message builders
    │   │   ├── network/        # WebSocket server and sender thread
    │   │   ├── save/           # Save events, local SaveStore, save thread
    │   │   └── util/           # ConcurrentQueue, std::visit helper
    │   └── test_suite.py       # End-to-end WebSocket tests
    └── shared/
        ├── static/world.json       # Rooms, exits, pickups, game config
        └── tileset_metadata/*.json # Per-tileset walkability
```

## Architecture

The engine runs four threads that communicate only through thread-safe queues:

```
            ┌───────────────────┐   InputQueue    ┌──────────────────┐
 client ───▶│ WebSocket server  │ ───────────────▶│                  │
            └───────────────────┘                 │    Game loop     │
            ┌───────────────────┐   OutputQueue   │    (30 Hz)       │
 client ◀───│   Sender thread   │ ◀───────────────│                  │
            └───────────────────┘                 └────────┬─────────┘
                                                           │ SaveEventQueue
                                                  ┌────────▼─────────┐
                                                  │   Save thread    │──▶ saves/<userId>.json
                                                  └──────────────────┘
```

- **Game loop** (`engine/session/game_loop.cpp`): a fixed 30 Hz timestep with an
  accumulator, which clamps long frames to avoid a "spiral of death". Each tick
  it drains pending input, then applies movement and collision, handles room
  exits and pickups, drains energy, checks for a win or loss, and emits a state
  update.
- **World data** is loaded once at startup and checked thoroughly (exit
  symmetry, spawn walkability, reachability and so on) before the server
  accepts connections.
- **Persistence never blocks the simulation.** Notable moments (room change,
  pickup, candy eaten, win, loss) push a save event onto a queue, and the save
  thread writes it to disk.
- **Single player:** the server accepts one active session at a time and
  rejects additional connections with close code `1008`.

## Building

Requirements: CMake ≥ 3.20, a C++20 compiler (tested with GCC 13), git, and
network access on the first configure. All dependencies are fetched by CMake
`FetchContent`, so no system libraries are needed:

- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3
- [spdlog](https://github.com/gabime/spdlog) v1.13.0
- [IXWebSocket](https://github.com/machinezone/IXWebSocket) v11.4.5 (TLS and zlib disabled)

```bash
cd services/game_service
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Running

Run from `services/game_service` so the default paths resolve:

```bash
cd services/game_service
./build/et_game_service
```

Or with Docker, from the repository root:

```bash
docker compose up --build
```

### Configuration (environment variables)

| Variable           | Default                       | Purpose                       |
|--------------------|-------------------------------|-------------------------------|
| `WORLD_JSON_PATH`  | `../shared/static/world.json` | World definition              |
| `TILESET_DIR_PATH` | `../shared/tileset_metadata`  | Tileset metadata directory    |
| `SAVE_DIR_PATH`    | `./saves`                     | Where save files are written  |
| `GAME_PORT`        | `8001`                        | WebSocket listen port         |

## Protocol (summary)

Connect to `ws://<host>:8001/game?userId=<id>&mode=<new|continue>`. The path
is ignored, and only the query string matters.

- `userId` is required and limited to letters, digits, `_` and `-`.
- `mode=continue` resumes from the user's save if one exists. Otherwise a new
  game starts.

**Client → server**

```json
{ "type": "input_move", "direction": "N|S|E|W", "pressed": true }
{ "type": "input_consume_candy" }
```

**Server → client**

| `type`         | When                         | Key fields                                                      |
|----------------|------------------------------|-----------------------------------------------------------------|
| `room_loaded`  | On connect and on room change | `room` (id, tileset, size, tile rows, active pickups), `player`, `arrivedFrom` |
| `state_update` | Every tick while playing      | `player`, `energy`, `phonePieceCount`, `candyCount`, `activePickups` |
| `game_over`    | On win or loss                | `outcome`: `"won"` or `"lost"`                                  |

## Testing

`services/game_service/test_suite.py` drives a running engine over WebSocket.
It covers spawning, room transitions, pickups, eating candy, rejection of a
second client, and collision.

```bash
pip install websockets
cd services/game_service && ./build/et_game_service &   # in one terminal
python3 services/game_service/test_suite.py              # in another
```

The *lose by energy depletion* test only passes if `energyDrainPerSecond` in
`world.json` is raised to about 50 or more. With the default value (0.25) it
times out.

## Possible next steps

- Unit tests for the simulation and world validation (no network needed)
- Multiple concurrent sessions
- A lightweight reference client to replace the removed Vulkan GUI
