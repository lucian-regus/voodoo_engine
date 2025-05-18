#include "infrastructure/file_system_event_consumer.h"
#include "infrastructure/context.h"
#include "infrastructure/plugin_interface.h"
#include "infrastructure/helpers.h"
#include "infrastructure/quarantine.h"

#include <stdint.h>
#include <unistd.h>
#include <linux/fanotify.h>

#include "domain/logger.h"

static void respond_to_fanotify(GlobalContext* ctx, int fd, uint32_t response_flag) {
    struct fanotify_response response = {
        .fd = fd,
        .response = response_flag
    };
    write(ctx->fanotify_execution_fd, &response, sizeof(response));
    close(fd);
}

void process_event(FileSystemEvent* event) {
    char* file_identity = generate_file_identity(event->path);
    if (!file_identity) {
        if (event->event_type == FAN_OPEN_EXEC_PERM) {
            respond_to_fanotify(event->global_context, event->metadata_fd, FAN_ALLOW);
        }

        free(event);
        return;
    }

    if (is_malware_allowed(event->global_context, file_identity)) {
        if (event->event_type == FAN_OPEN_EXEC_PERM) {
            respond_to_fanotify(event->global_context, event->metadata_fd, FAN_ALLOW);
        }

        free(file_identity);
        free(event);
        return;
    }

    int malware_id = 0;
    Plugin* detecting_plugin = NULL;

    for (GList* node = event->global_context->loaded_plugins; node != NULL; node = node->next) {
        Plugin* plugin = (Plugin*)node->data;
        int result = plugin->evaluate_file(event->path);
        if (result != 0) {
            malware_id = result;
            detecting_plugin = plugin;
            break;
        }
    }


    if (event->event_type == FAN_OPEN_EXEC_PERM) {
        respond_to_fanotify(event->global_context, event->metadata_fd,
                            malware_id ? FAN_DENY : FAN_ALLOW);
    }

    if (malware_id) {
        quarantine_file(file_identity, event->path, detecting_plugin->name, malware_id);
        log_message(LOG_LEVEL_INFO, "%s quarantined.\n", event->path);
    }

    free(file_identity);
    free(event);
}

void start_file_system_event_consumer(void) {
    GThreadPool* thread_pool = g_thread_pool_new((void*)process_event, NULL, 5, TRUE, NULL);
    GlobalContext* global_context = get_context();

    while (1) {
        FileSystemEvent* event = g_async_queue_pop(global_context->file_system_event_batch_queue);
        if (event == FILE_SYSTEM_EVENT_TERMINATE_SIGNAL)
            break;

        g_thread_pool_push(thread_pool, event, NULL);
    }
    close(global_context->fanotify_execution_fd);
    close(global_context->mount_fd);
    close(global_context->fanotify_creation_fd);

    g_thread_pool_free(thread_pool, FALSE, TRUE);
}