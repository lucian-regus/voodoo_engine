#include "domain/logger.h"
#include "domain/config.h"

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;

void init_logger() {
    pthread_mutex_lock(&log_mutex);

    log_file = fopen(LOG_FILE, "a");
    if (!log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", LOG_FILE);
    }

    pthread_mutex_unlock(&log_mutex);
}

void close_logger() {
    pthread_mutex_lock(&log_mutex);

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_destroy(&log_mutex);
}

static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARN: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        default: return "UNKNOWN";
    }
}

void log_message(LogLevel level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    if (!log_file) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *tm_info = localtime(&tv.tv_sec);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s,%03ld [%s] ", timestamp, tv.tv_usec / 1000, level_to_string(level));

    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    fprintf(log_file, "%s%s", prefix, message);
    fprintf(stderr, "%s%s", prefix, message);

    fflush(log_file);
    fflush(stderr);

    pthread_mutex_unlock(&log_mutex);
}
