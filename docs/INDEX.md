# rpiRadio — Project Index

## Overview

rpiRadio is a C++17 internet radio player for Raspberry Pi. It runs as a systemd daemon that plays M3U-based station lists through mpv, accepts physical remote control input via evdev, and publishes state over MQTT. A CLI binary (same executable) communicates with the daemon over a Unix domain socket.

## Quick Start

```bash
# Build
make

# Run daemon (foreground)
./build/rpiradio daemon

# CLI commands (daemon must be running)
./build/rpiradio play 1          # Play station #1
./build/rpiradio stop
./build/rpiradio next / prev
./build/rpiradio volume 80       # Set volume
./build/rpiradio volume up/down  # Adjust ±5
./build/rpiradio list            # List stations
./build/rpiradio status          # Current state as JSON
./build/rpiradio reload          # Reload config + stations
./build/rpiradio devices         # Select input device (interactive menu)
./build/rpiradio bind list       # Show key bindings
./build/rpiradio bind scan <action>      # Scan a key press and bind it
./build/rpiradio bind set <key> <action> # Set binding directly
./build/rpiradio bind remove <key>       # Remove binding
```

## Dependencies

| Dependency | Purpose | Package |
|---|---|---|
| mpv | Audio playback engine (forked as child process) | `apt install mpv` |
| libmosquitto | MQTT client library | `apt install libmosquitto-dev` |
| libevdev | Linux input device handling | `apt install libevdev-dev` |
| nlohmann/json | JSON parsing (header-only) | `apt install nlohmann-json3-dev` |
| g++ (C++17) | Compiler | `apt install g++` |
| pkg-config | Build-time flag resolution | `apt install pkg-config` |

## Source Files

| File | Responsibility |
|---|---|
| `src/main.cpp` | Entry point — dispatches to `daemon_run()` or `cli_dispatch()`. Only the daemon path loads config; CLI commands use the well-known socket path directly. |
| `src/daemon.h/cpp` | Daemon mode: epoll event loop, wires all components together, handles IPC commands and signal handling |
| `src/cli.h/cpp` | CLI mode: parses subcommands, sends JSON requests to daemon via IPC. Does not load config — uses the default IPC socket path. |
| `src/config.h/cpp` | JSON config load from `/etc/rpiradio/config.json`. Used only by the daemon. |
| `src/mpv_controller.h/cpp` | Forks mpv child process, communicates via mpv's JSON IPC socket |
| `src/station_manager.h/cpp` | Loads M3U playlists, tracks current station, provides next/prev/select |
| `src/ipc_server.h/cpp` | Unix domain socket server — accepts one-shot JSON request/response connections |
| `src/ipc_client.h/cpp` | Unix domain socket client — sends a JSON request and reads one response |
| `src/mqtt_publisher.h/cpp` | Publishes state, station, metadata, and volume to MQTT topics |
| `src/input_handler.h/cpp` | Reads evdev key events, device discovery by name, key scanning for binding setup |
| `src/keybind_manager.h/cpp` | Maps evdev key names to action strings, persisted via config |
| `src/log.h/cpp` | Logging module: 5 levels, timestamp + file:line format, stderr output |

## Configuration

Config file location: `/etc/rpiradio/config.json`. Installed by `make install` from `config/default_config.json`.

The daemon reads this file at startup and on reload (SIGHUP or `rpiradio reload`). If the file is missing or unparseable, compiled-in defaults are used. The daemon never writes to the config file — it is admin-managed.

CLI commands (`rpiradio play`, `rpiradio status`, etc.) do **not** read the config file. They communicate with the daemon over the well-known IPC socket path (`/tmp/rpiradio.sock`).

| Key | Type | Default | Purpose |
|---|---|---|---|
| `m3u_path` | string | `/etc/rpiradio/stations.m3u` | Path to M3U station playlist |
| `mqtt_host` | string | `localhost` | MQTT broker hostname |
| `mqtt_port` | int | `1883` | MQTT broker port |
| `topic_prefix` | string | `rpiradio` | MQTT topic prefix (e.g., `rpiradio/state`) |
| `evdev_name` | string | `""` | Name of evdev input device (e.g., `gpio_ir_recv`). Resolved to `/dev/input/eventN` at startup by scanning devices. Use `rpiradio devices` to list available names. |
| `bindings` | object | *(see default_config.json)* | Key name → action string map |
| `log_level` | string | `INFO` | Log level: TRACE, DEBUG, INFO, WARN, ERROR |
| `mpv_extra_args` | array | `[]` | Additional arguments passed to mpv |
| `ipc_socket_path` | string | `/tmp/rpiradio.sock` | Unix socket for daemon ↔ CLI IPC |
| `mpv_socket_path` | string | `/tmp/rpiradio-mpv.sock` | Unix socket for daemon ↔ mpv IPC |

## Architecture

- [Overview](architecture/overview.md) — System architecture, component interactions, data flow

## Standards

- [Logging](standards/logging.md) — Log levels, format, toggle mechanism, macros

## Decisions

*None yet — ADRs will be created as architectural decisions are made.*

## Caveats

*None yet — caveats will be documented as they are discovered.*

## Other Files

| Path | Purpose |
|---|---|
| `Makefile` | Build system — `make` to build, `make clean` to remove artifacts |
| `config/default_config.json` | Reference default configuration, installed to `/etc/rpiradio/config.json` |
| `systemd/rpiradio.service` | systemd unit file — runs as user `rpiradio`, groups `input` + `audio` |
