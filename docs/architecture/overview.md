# Architecture Overview

## System Purpose

rpiRadio is a headless internet radio player for Raspberry Pi. It plays audio streams from an M3U playlist using mpv, accepts input from a physical IR/RF remote control via evdev, and publishes its state over MQTT for integration with home automation systems (e.g., Home Assistant).

## High-Level Architecture

The system is a single binary (`rpiradio`) that operates in two modes:

```
┌─────────────────────────────────────────────┐
│  rpiradio binary                            │
│                                             │
│  ┌──────────────┐    ┌───────────────────┐  │
│  │  CLI mode    │    │   Daemon mode     │  │
│  │  (one-shot)  │───▶│   (long-running)  │  │
│  └──────────────┘    └───────────────────┘  │
│        via Unix domain socket IPC           │
└─────────────────────────────────────────────┘
```

- **Daemon mode** (`rpiradio daemon`): Long-running process with an epoll event loop. Owns all subsystems.
- **CLI mode** (`rpiradio <command>`): One-shot process that sends a JSON request to the daemon via IPC and prints the response.

## Component Map

### Daemon Components (all wired in `daemon.cpp`)

| Component | Class | File | Role |
|---|---|---|---|
| Audio playback | `MpvController` | `src/mpv_controller.h/cpp` | Forks an mpv child process, communicates via mpv's JSON IPC protocol over a Unix socket. Manages play/stop/pause/volume and receives metadata + pause property changes. |
| Station management | `StationManager` | `src/station_manager.h/cpp` | Parses M3U playlists (supports `#EXTINF` station names). Tracks current station index, provides next/prev/select navigation. |
| MQTT integration | `MqttPublisher` | `src/mqtt_publisher.h/cpp` | Publishes JSON state to MQTT topics using libmosquitto. Topics: `{prefix}/state`, `{prefix}/station`, `{prefix}/metadata`, `{prefix}/volume`. QoS 1, retained. |
| Physical input | `InputHandler` | `src/input_handler.h/cpp` | Opens an evdev device (e.g., IR receiver), reads key-down events. Also provides `scan_key()` for interactive key binding setup. Grabs the device exclusively when in daemon mode. |
| Key binding | `KeybindManager` | `src/keybind_manager.h/cpp` | Maps evdev key names (e.g., `KEY_PLAY`) to action strings (e.g., `play_pause`). Bindings stored in config and persisted on change. |
| IPC server | `IpcServer` | `src/ipc_server.h/cpp` | Listens on a Unix domain socket. Accepts one connection at a time, reads one JSON line, dispatches to handler, writes one JSON line response, closes. Non-blocking listen fd for epoll integration. |

### CLI Components

| Component | Class | File | Role |
|---|---|---|---|
| IPC client | `IpcClient` | `src/ipc_client.h/cpp` | Connects to daemon socket, sends JSON request, reads JSON response. 5-second receive timeout. |
| Command dispatch | `cli_dispatch()` | `src/cli.h/cpp` | Parses CLI subcommands, builds JSON requests, calls IPC client, formats output. |

### Shared Components

| Component | File | Role |
|---|---|---|
| Configuration | `src/config.h/cpp` | Loads/saves JSON config. Path: `$XDG_CONFIG_HOME/rpiradio/config.json`. Uses nlohmann/json. |
| Logging | `src/log.h/cpp` | 5-level logging to stderr. See [logging standard](../standards/logging.md). |

## Daemon Event Loop

The daemon uses Linux `epoll` to multiplex four file descriptors:

```
epoll_wait()
  ├── signalfd      → SIGTERM/SIGINT: clean shutdown
  │                   SIGHUP: reload config + stations + bindings
  ├── IPC listen fd → accept connection, read JSON command, dispatch, respond
  ├── evdev fd      → read key event, lookup binding, execute action
  └── mpv socket fd → read mpv events (metadata changes, pause state, end-of-file)
```

All I/O is non-blocking. The daemon runs single-threaded.

## IPC Protocol

Communication between CLI and daemon uses newline-delimited JSON over a Unix domain socket.

**Request format:**
```json
{"command": "<name>", "args": {"key": "value"}}
```

**Response format:**
```json
{"status": "ok", "data": ...}
{"status": "error", "message": "description"}
```

**Available commands:** `play`, `stop`, `next`, `prev`, `volume`, `list`, `status`, `bind_list`, `bind_set`, `bind_remove`, `reload`.

Each CLI invocation opens a new connection, sends one request, reads one response, and disconnects.

## MQTT Topics

All topics are prefixed with the configured `topic_prefix` (default: `rpiradio`).

| Topic | Payload | Published when |
|---|---|---|
| `{prefix}/state` | Full JSON state object | Station change, play/stop/pause, volume change |
| `{prefix}/station` | `{"index": N, "name": "...", "url": "..."}` | Station change |
| `{prefix}/metadata` | Stream title string (e.g., artist — song) | mpv reports new `icy-title` or `title` |
| `{prefix}/volume` | Integer as string | Volume change |

All messages are published with QoS 1 and the retain flag set.

## External Dependencies

| Dependency | How used | Failure behavior |
|---|---|---|
| **mpv** | Forked as child process, controlled via JSON IPC socket | Fatal: daemon exits if mpv fails to start |
| **libmosquitto** | MQTT client library, linked at build time | Graceful: daemon continues without MQTT if connection fails |
| **libevdev** | Used for `libevdev_event_code_get_name()` to resolve keycode names | Graceful: daemon continues without input if device not configured/available |
| **nlohmann/json** | Header-only JSON library, used throughout | Build-time dependency |

## Actions

The following action strings are used by the keybind system and execute_action():

| Action | Behavior |
|---|---|
| `play_pause` | If not playing: play current station. If playing: toggle pause. |
| `stop` | Stop playback |
| `next` | Advance to next station and play |
| `prev` | Go to previous station and play |
| `volume_up` | Increase volume by 5 |
| `volume_down` | Decrease volume by 5 |
