#pragma once

#include <string>

enum class LogLevel { TRACE, DEBUG, INFO, WARN, ERROR };

void log_init(const std::string& level_str);
void log_msg(LogLevel level, const char* file, int line, const char* fmt, ...);

#define LOG_TRACE(...) log_msg(LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) log_msg(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_msg(LogLevel::INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_msg(LogLevel::WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
