#include "infrastructure/context.h"
#include "domain/logger.h"
#include <sys/eventfd.h>

static GlobalContext* global_context = NULL;
static pthread_mutex_t context_lock = PTHREAD_MUTEX_INITIALIZER;

GlobalContext* get_context(void) {
    pthread_mutex_lock(&context_lock);

    if (global_context == NULL) {
        global_context = malloc(sizeof(GlobalContext));
        if (global_context == NULL) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for the Global Context.\n");
            exit(1);
        }

        init_logger();

        global_context->shutdown_event_fd = eventfd(0, 0);
        global_context->database_context = database_context_create();
        global_context->keep_running = 1;

        global_context->fanotify_event_batch_queue = g_async_queue_new();
        global_context->file_system_event_batch_queue = g_async_queue_new();

        global_context->loaded_plugins = NULL;
        global_context->loaded_plugins_handles = NULL;
    }

    pthread_mutex_unlock(&context_lock);
    return global_context;
}

void destroy_context(void) {
    pthread_mutex_lock(&context_lock);

    if (global_context != NULL) {
        database_context_destroy(global_context->database_context);

        g_async_queue_unref(global_context->fanotify_event_batch_queue);
        g_async_queue_unref(global_context->file_system_event_batch_queue);

        g_list_free(global_context->loaded_plugins);
        g_list_free(global_context->loaded_plugins_handles);

        free(global_context);
        global_context = NULL;
        close_logger();
    }

    pthread_mutex_unlock(&context_lock);
}