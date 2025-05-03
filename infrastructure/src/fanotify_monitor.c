#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <unistd.h>
#include <string.h>

#include "infrastructure/fanotify_monitor.h"
#include "infrastructure/context.h"
#include "domain/config.h"
#include "domain/logger.h"
#include "infrastructure/fanotify_event_batch.h"

void start_file_creations_monitoring(void) {
    GlobalContext* global_context = get_context();

    ssize_t bytes_read;
    char fanotify_events_buffer[FANOTIFY_BUFFER_SIZE];
    struct pollfd fds;

    global_context->mount_fd = open("/", O_DIRECTORY | O_RDONLY);
    if (global_context->mount_fd == -1) {
        log_message(LOG_LEVEL_ERROR, "[File Creation Monitoring] Opening / failed: %s\n", strerror(errno));
        pthread_exit(NULL);
    }


    global_context->fanotify_creation_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_REPORT_DFID_NAME, 0);
    if (global_context->fanotify_creation_fd == -1) {
        log_message(LOG_LEVEL_ERROR, "[File Creation Monitoring] fanotify_init failed: %s\n", strerror(errno));
        pthread_exit(NULL);
    }


    if (fanotify_mark(global_context->fanotify_creation_fd,
        FAN_MARK_ADD | FAN_MARK_FILESYSTEM,
        FAN_CLOSE_WRITE,
        AT_FDCWD,
        "/") == -1) {
        log_message(LOG_LEVEL_ERROR, "[File Creation Monitoring] fanotify_mark failed %s\n", strerror(errno));
        pthread_exit(NULL);
        }

    fds.fd = global_context->fanotify_creation_fd;
    fds.events = POLLIN;
    while (global_context->keep_running) {
        int poll_ret = poll(&fds, 1, 500);
        if (poll_ret == 0) {
            continue;
        }

        bytes_read = read(global_context->fanotify_creation_fd, fanotify_events_buffer, FANOTIFY_BUFFER_SIZE);
        if (bytes_read == -1) {
            log_message(LOG_LEVEL_ERROR, "[File Creation Monitoring] Reading from fanotify_creation_fd failed\n");
            pthread_exit(NULL);
        }

        FanotifyEventBatch* fanotify_event_batch = malloc(sizeof(FanotifyEventBatch));
        fanotify_event_batch->global_context = global_context;
        memcpy(fanotify_event_batch->fanotify_events_buffer, fanotify_events_buffer, bytes_read);
        fanotify_event_batch->fanotify_events_buffer_len = bytes_read;

        g_async_queue_push(global_context->fanotify_event_batch_queue, fanotify_event_batch);
    }
}

void start_file_executions_monitoring(void) {
    GlobalContext* global_context = get_context();

    char fanotify_events_buffer[FANOTIFY_BUFFER_SIZE];
    ssize_t bytes_read;
    struct pollfd fds;

    global_context->fanotify_execution_fd = fanotify_init(FAN_CLOEXEC | FAN_CLASS_PRE_CONTENT, 0);
    if (global_context->fanotify_execution_fd == -1) {
        log_message(LOG_LEVEL_ERROR, "[File Executions Monitoring] fanotify_init failed %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fanotify_mark(global_context->fanotify_execution_fd,
    FAN_MARK_ADD | FAN_MARK_FILESYSTEM,
    FAN_OPEN_EXEC_PERM,
    AT_FDCWD,
    "/") == -1) {
        log_message(LOG_LEVEL_ERROR, "[File Executions Monitoring] fanotify_mark failed %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fds.fd = global_context->fanotify_execution_fd;
    fds.events = POLLIN;

    while (global_context->keep_running) {
        int poll_ret = poll(&fds, 1, 500);
        if (poll_ret == 0) {
            continue;
        }

        bytes_read = read(global_context->fanotify_execution_fd, fanotify_events_buffer, FANOTIFY_BUFFER_SIZE);
        if (bytes_read == -1) {
            log_message(LOG_LEVEL_ERROR, "[File Executions Monitoring] Reading from fanotify_creation_fd failed\n");
            exit(EXIT_FAILURE);
        }

        FanotifyEventBatch* fanotify_event_batch = malloc(sizeof(FanotifyEventBatch));
        fanotify_event_batch->global_context = global_context;
        memcpy(fanotify_event_batch->fanotify_events_buffer, fanotify_events_buffer, bytes_read);
        fanotify_event_batch->fanotify_events_buffer_len = bytes_read;

        g_async_queue_push(global_context->fanotify_event_batch_queue, fanotify_event_batch);
    }
}