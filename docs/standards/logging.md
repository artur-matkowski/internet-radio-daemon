# Logging Standard

## Implementation

Logging is implemented in `src/log.h` and `src/log.cpp`. All logging goes to **stderr**.

## Log Levels

| Level | Enum value | Macro | Use for |
|---|---|---|---|
| TRACE | `LogLevel::TRACE` | `LOG_TRACE(...)` | Fine-grained diagnostics: function entry/exit, variable values |
| DEBUG | `LogLevel::DEBUG` | `LOG_DEBUG(...)` | Diagnostic info: IPC request payloads, key events, MQTT publish details |
| INFO | `LogLevel::INFO` | `LOG_INFO(...)` | Operational milestones: startup, config loaded, station changes, connections |
| WARN | `LogLevel::WARN` | `LOG_WARN(...)` | Recoverable issues: MQTT connection failed, evdev unavailable, unknown action |
| ERROR | `LogLevel::ERROR` | `LOG_ERROR(...)` | Failures: mpv start failed, config parse error, socket bind failure |

Messages at or above the configured level are printed. Messages below are suppressed.

## Log Format

```
YYYY-MM-DD HH:MM:SS.mmm [LEVEL] filename.cpp:line: message
```

Example:
```
2026-02-18 14:30:05.123 [INFO ] daemon.cpp:203: rpiRadio daemon starting
2026-02-18 14:30:05.456 [INFO ] mpv_controller.cpp:59: started mpv pid=1234 socket=/tmp/rpiradio-mpv.sock
2026-02-18 14:30:06.789 [DEBUG] ipc_server.cpp:82: IPC request: {"command":"play","args":{"station":1}}
```

- Timestamp uses local time with millisecond precision (`clock_gettime(CLOCK_REALTIME)`)
- Level is left-padded to 5 characters
- Filename is the basename only (no directory path)
- Uses printf-style format strings (`%s`, `%d`, etc.)

## Toggle Mechanism

Log level is set at startup via two sources, in priority order:

1. **Environment variable** `LOG_LEVEL` (highest priority)
2. **Config field** `log_level` in `$XDG_CONFIG_HOME/rpiradio/config.json`

The `LOG_LEVEL` env var always overrides the config file value if set and non-empty.

Accepted values (case-insensitive): `TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`.

If an unrecognized value is provided, it defaults to `INFO`.

### Setting the log level

```bash
# Via environment variable (one-time)
LOG_LEVEL=DEBUG ./build/rpiradio daemon

# Via systemd override
sudo systemctl edit rpiradio
# Add: Environment=LOG_LEVEL=DEBUG

# Via config file (persistent)
# Edit ~/.config/rpiradio/config.json, set "log_level": "DEBUG"

# Runtime reload (changes config log_level, but not env var)
./build/rpiradio reload
```

Note: The `reload` command re-reads the config and calls `log_init()` again, so config-based level changes take effect at runtime. However, if `LOG_LEVEL` env var is set, it will still take precedence after reload.

## Usage in Code

```cpp
#include "log.h"

void some_function(const std::string& arg) {
    LOG_TRACE("some_function called with arg=%s", arg.c_str());

    if (something_unexpected) {
        LOG_WARN("unexpected condition: %s", detail.c_str());
    }

    LOG_INFO("operation completed successfully");
}
```

The macros automatically inject `__FILE__` and `__LINE__`, so callers only provide the format string and arguments.

## Conventions

- Use `LOG_INFO` for events meaningful to operators (startup, shutdown, station changes, connections)
- Use `LOG_DEBUG` for events useful during development (IPC payloads, key presses, MQTT publishes)
- Use `LOG_TRACE` sparingly for deep debugging
- Use `LOG_WARN` when something failed but the system can continue
- Use `LOG_ERROR` when something failed and it affects functionality
- Never log sensitive data (credentials, tokens)
- Pass C strings to format specifiers with `.c_str()`
