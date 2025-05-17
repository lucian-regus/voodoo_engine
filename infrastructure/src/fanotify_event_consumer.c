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

void build_events(FanotifyEventBatch* fanotify_event_batch) {
    struct fanotify_event_metadata *metadata;
    char procfd_path[PATH_MAX];
    char file_path[PATH_MAX];
    struct fanotify_response response = {0};

    for (metadata = (struct fanotify_event_metadata *)fanotify_event_batch->fanotify_events_buffer;
         FAN_EVENT_OK(metadata, fanotify_event_batch->fanotify_events_buffer_len);
         metadata = FAN_EVENT_NEXT(metadata, fanotify_event_batch->fanotify_events_buffer_len)) {

        int event_fd = -1;
        const char *file_name = NULL;
        size_t path_len = 0;

        if (metadata->mask & FAN_CLOSE_WRITE) {
            struct fanotify_event_info_fid *fid = (struct fanotify_event_info_fid *)(metadata + 1);
            struct file_handle *file_handle = (struct file_handle *)fid->handle;

            file_name = file_handle->f_handle + file_handle->handle_bytes;

            event_fd = open_by_handle_at(fanotify_event_batch->global_context->mount_fd, file_handle, O_RDONLY);
            if (event_fd == -1) {
                if (errno == ESTALE) {
                    log_message(LOG_LEVEL_WARN, "File handle is no longer valid. File has been deleted: %s\n", file_name);
                    continue;
                }
                perror("open_by_handle_at");
                exit(EXIT_FAILURE);
            }

            snprintf(procfd_path, sizeof(procfd_path), "/proc/self/fd/%d", event_fd);
            path_len = readlink(procfd_path, file_path, sizeof(file_path) - 1);
            if (path_len == -1) {
                perror("readlink");
                close(event_fd);
                exit(EXIT_FAILURE);
            }

            file_path[path_len] = '\0';

            if (strncmp(file_path, "/root", 5) == 0 ||
                strncmp(file_path, "/proc", 5) == 0 ||
                strncmp(file_path, "/sys", 4) == 0 ||
                strncmp(file_path, "/dev", 4) == 0 ||
                strncmp(file_path, "/var/log/journal", 16) == 0
                ) {
                close(event_fd);
                continue;
            }

            strncat(file_path, "/", sizeof(file_path) - strlen(file_path) - 1);
            strncat(file_path, file_name, sizeof(file_path) - strlen(file_path) - 1);

            FileSystemEvent* event = malloc(sizeof(FileSystemEvent));
            if (!event) {
                perror("malloc");
                close(event_fd);
                exit(EXIT_FAILURE);
            }
            event->global_context = fanotify_event_batch->global_context;
            event->event_type = metadata->mask;
            strncpy(event->path, file_path, sizeof(event->path) - 1);
            event->path[sizeof(event->path) - 1] = '\0';

            g_async_queue_push(fanotify_event_batch->global_context->file_system_event_batch_queue, event);

            close(event_fd);
        }

        if (metadata->mask & FAN_OPEN_EXEC_PERM && metadata->fd >= 0) {
            snprintf(procfd_path, sizeof(procfd_path), "/proc/self/fd/%d", metadata->fd);
            path_len = readlink(procfd_path, file_path, sizeof(file_path) - 1);
            if (path_len == -1) {
                perror("readlink");
                close(metadata->fd);
                exit(EXIT_FAILURE);
            }

            file_path[path_len] = '\0';

            if (strncmp(file_path, "/root", 5) == 0 ||
                strncmp(file_path, "/proc", 5) == 0 ||
                strncmp(file_path, "/sys", 4) == 0 ||
                strncmp(file_path, "/dev", 4) == 0 ||
                strncmp(file_path, "/var/log/journal", 16) == 0) {

                response.fd = metadata->fd;
                response.response = FAN_ALLOW;
                write(fanotify_event_batch->global_context->fanotify_execution_fd, &response, sizeof(response));
                close(metadata->fd);

                continue;
            }

            FileSystemEvent* event = malloc(sizeof(FileSystemEvent));
            if (!event) {
                perror("malloc");
                close(metadata->fd);
                exit(EXIT_FAILURE);
            }
            event->global_context = fanotify_event_batch->global_context;
            event->event_type = metadata->mask;
            strncpy(event->path, file_path, sizeof(event->path) - 1);
            event->path[sizeof(event->path) - 1] = '\0';
            event->metadata_fd = metadata->fd;

            g_async_queue_push(fanotify_event_batch->global_context->file_system_event_batch_queue, event);
        }
    }

    free(fanotify_event_batch);
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