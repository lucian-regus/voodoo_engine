#pragma once

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
} LogLevel;

void init_logger();
void log_message(LogLevel level, const char *format, ...);
void close_logger();