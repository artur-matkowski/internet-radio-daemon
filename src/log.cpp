#include "log.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstring>

static LogLevel g_level = LogLevel::INFO;

static LogLevel parse_level(const std::string& s) {
    if (s == "TRACE" || s == "trace") return LogLevel::TRACE;
    if (s == "DEBUG" || s == "debug") return LogLevel::DEBUG;
    if (s == "INFO"  || s == "info")  return LogLevel::INFO;
    if (s == "WARN"  || s == "warn")  return LogLevel::WARN;
    if (s == "ERROR" || s == "error") return LogLevel::ERROR;
    return LogLevel::INFO;
}

static const char* level_str(LogLevel l) {
    switch (l) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?";
}

void log_init(const std::string& level) {
    const char* env = std::getenv("LOG_LEVEL");
    if (env && env[0]) {
        g_level = parse_level(env);
    } else {
        g_level = parse_level(level);
    }
}

void log_msg(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < g_level) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);

    const char* base = std::strrchr(file, '/');
    base = base ? base + 1 : file;

    fprintf(stderr, "%s.%03ld [%-5s] %s:%d: ",
            timebuf, ts.tv_nsec / 1000000, level_str(level), base, line);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}
