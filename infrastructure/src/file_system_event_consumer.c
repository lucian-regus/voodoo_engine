#include "infrastructure/file_system_event_consumer.h"
#include "infrastructure/context.h"
#include "infrastructure/plugin_interface.h"
#include "infrastructure/helpers.h"
#include "domain/logger.h"
#include "infrastructure/quarantine.h"

#include <unistd.h>
#include <linux/fanotify.h>

void process_event(FileSystemEvent* event) {
    struct fanotify_response response = {0};

    int detected_malware_id = 0;
    Plugin* detecting_plugin = NULL;

    char* file_identity = generate_file_identity(event->path);
    if (file_identity == NULL) {
        log_message(LOG_LEVEL_ERROR,"Failed to generate file identity. -> %s\n", event->path);
        free(event);

        return;
    }

    if (is_malware_allowed(event->global_context, file_identity) == 1) {
        if (event->event_type == FAN_OPEN_EXEC_PERM) {
            response.fd = event->metadata_fd;
            response.response = FAN_ALLOW;
            write(event->global_context->fanotify_execution_fd, &response, sizeof(response));
            close(event->metadata_fd);
        }

        free(file_identity);
        free(event);

        return;
    }

    for (GList* l = event->global_context->loaded_plugins; l != NULL; l = l->next) {
        Plugin* plugin = (Plugin*)l->data;

        int scan_result_id = plugin->evaluate_file(event->path);
        if (scan_result_id) {
            detected_malware_id = scan_result_id;
            detecting_plugin = plugin;
            break;
        }
    }

    if (event->event_type == FAN_OPEN_EXEC_PERM) {
        response.fd = event->metadata_fd;
        response.response = detected_malware_id ? FAN_DENY : FAN_ALLOW;
        write(event->global_context->fanotify_execution_fd, &response, sizeof(response));
        close(event->metadata_fd);
    }

    if (detected_malware_id) {
        quarantine_file(file_identity, event->path, detecting_plugin->name, detected_malware_id);
    }

    free(event);
    free(file_identity);
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
    close(global_context->fanotify_execution_fd);

    g_thread_pool_free(thread_pool, FALSE, TRUE);
}