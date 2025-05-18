#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fanotify.h>
#include <unistd.h>
#include <string.h>
#include "glib.h"

#include "infrastructure/file_system_event.h"
#include "infrastructure/fanotify_event_consumer.h"

#include "domain/logger.h"
#include "infrastructure/context.h"
#include "infrastructure/fanotify_event_batch.h"

static int should_ignore_path(const char* path) {
    return strncmp(path, "/root", 5) == 0 ||
           strncmp(path, "/proc", 5) == 0 ||
           strncmp(path, "/sys", 4) == 0 ||
           strncmp(path, "/dev", 4) == 0 ||
           strncmp(path, "/var/lib/postgresql", 19) == 0 ||
           strncmp(path, "/var/log/journal", 16) == 0;
}

static ssize_t get_file_path(int fd, char* buffer, size_t bufsize) {
    char procfd_path[64];
    snprintf(procfd_path, sizeof(procfd_path), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(procfd_path, buffer, bufsize - 1);
    if (len != -1) buffer[len] = '\0';

    return len;
}

static void send_fanotify_response(int fanotify_fd, int metadata_fd, uint32_t response_flag) {
    struct fanotify_response response = {
        .fd = metadata_fd,
        .response = response_flag
    };
    write(fanotify_fd, &response, sizeof(response));
    close(metadata_fd);
}

static FileSystemEvent* create_filesystem_event(GlobalContext* ctx, uint64_t event_type, const char* path, int fd) {
    FileSystemEvent* event = malloc(sizeof(FileSystemEvent));
    if (!event) {
        perror("malloc");
        if (fd >= 0) close(fd);
        exit(EXIT_FAILURE);
    }
    event->global_context = ctx;
    event->event_type = event_type;
    strncpy(event->path, path, sizeof(event->path) - 1);
    event->path[sizeof(event->path) - 1] = '\0';
    event->metadata_fd = fd;

    return event;
}

void build_events(FanotifyEventBatch* batch) {
    struct fanotify_event_metadata* metadata;
    GlobalContext* global_context = batch->global_context;

    for (metadata = (struct fanotify_event_metadata*)batch->fanotify_events_buffer;
         FAN_EVENT_OK(metadata, batch->fanotify_events_buffer_len);
         metadata = FAN_EVENT_NEXT(metadata, batch->fanotify_events_buffer_len)) {

        if (metadata->mask & FAN_CLOSE_WRITE) {
            struct fanotify_event_info_fid* fid = (struct fanotify_event_info_fid*)(metadata + 1);
            struct file_handle* handle = (struct file_handle*)fid->handle;
            const char* file_name = handle->f_handle + handle->handle_bytes;

            int event_fd = open_by_handle_at(global_context->mount_fd, handle, O_RDONLY);
            if (event_fd == -1) {
                if (errno == ESTALE) {
                    log_message(LOG_LEVEL_WARN, "File deleted before handle could open: %s\n", file_name);
                    continue;
                }
                perror("open_by_handle_at");
                continue;
            }

            char path[PATH_MAX];
            if (get_file_path(event_fd, path, sizeof(path)) == -1) {
                perror("readlink");
                close(event_fd);
                exit(EXIT_FAILURE);
            }

            if (should_ignore_path(path)) {
                close(event_fd);
                continue;
            }

            if (strlen(path) + strlen(file_name) + 2 < sizeof(path)) {
                strncat(path, "/", sizeof(path) - strlen(path) - 1);
                strncat(path, file_name, sizeof(path) - strlen(path) - 1);
            }

            FileSystemEvent* event = create_filesystem_event(global_context, metadata->mask, path, -1);
            g_async_queue_push(global_context->file_system_event_batch_queue, event);
            close(event_fd);
        }

        if (metadata->mask & FAN_OPEN_EXEC_PERM && metadata->fd >= 0) {
            char path[PATH_MAX];
            if (get_file_path(metadata->fd, path, sizeof(path)) == -1) {
                perror("readlink");
                send_fanotify_response(global_context->fanotify_execution_fd, metadata->fd, FAN_ALLOW);
                continue;
            }

            if (should_ignore_path(path)) {
                send_fanotify_response(global_context->fanotify_execution_fd, metadata->fd, FAN_ALLOW);
                continue;
            }

            FileSystemEvent* event = create_filesystem_event(global_context, metadata->mask, path, metadata->fd);
            g_async_queue_push(global_context->file_system_event_batch_queue, event);
        }
    }

    free(batch);
}

void start_raw_event_consumer(void) {
    GThreadPool* thread_pool = g_thread_pool_new((void*)build_events, NULL, 5, TRUE, NULL);
    GlobalContext* global_context = get_context();

    while (1) {
        FanotifyEventBatch* event = g_async_queue_pop(global_context->fanotify_event_batch_queue);
        if ((gpointer)event == FANOTIFY_EVENT_BATCH_TERMINATED) {
            break;
        }

        g_thread_pool_push(thread_pool, event, NULL);
    }

    g_thread_pool_free(thread_pool, FALSE, TRUE);
}