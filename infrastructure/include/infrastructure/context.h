#pragma once

#include "domain/database.h"

typedef struct {
    int shutdown_event_fd;

    DatabaseContext* database_context;
    volatile sig_atomic_t keep_running;

    GAsyncQueue* fanotify_event_batch_queue;
    GAsyncQueue* file_system_event_batch_queue;

    int fanotify_creation_fd, fanotify_execution_fd;
    int mount_fd;

    GList* loaded_plugins;
    GList* loaded_plugins_handles;
} GlobalContext;

GlobalContext* get_context(void);
void destroy_context(void);