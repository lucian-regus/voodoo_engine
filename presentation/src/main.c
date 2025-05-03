#include "infrastructure/context.h"
#include "presentation/signal_handler.h"
#include "infrastructure/file_system_monitor.h"
#include "infrastructure/fanotify_event_consumer.h"
#include "infrastructure/file_system_event_consumer.h"
#include "infrastructure/plugin_manager.h"


#include <unistd.h>
#include <stdint.h>
#include <poll.h>

static void initialize() {
    setup_signal_handlers();
    load_plugins();
    start_file_system_monitoring();
}

static void wait_for_shutdown(GlobalContext *context) {
    struct pollfd pfd = {
        .fd = context->shutdown_event_fd,
        .events = POLLIN
    };

    poll(&pfd, 1, -1);

    uint64_t buf;
    read(context->shutdown_event_fd, &buf, sizeof(buf));
}

static void cleanup(GlobalContext *context) {
    stop_fanotify_monitoring();

    g_async_queue_push(context->fanotify_event_batch_queue, FANOTIFY_EVENT_BATCH_TERMINATED);
    stop_fanotify_event_consumer();

    g_async_queue_push(context->file_system_event_batch_queue, FILE_SYSTEM_EVENT_TERMINATE_SIGNAL);
    stop_file_system_event_consumer();

    unload_plugins();
    destroy_context();
}

int main(){
    GlobalContext *context = get_context();

    initialize();
    wait_for_shutdown(context);
    cleanup(context);
    return 0;
}