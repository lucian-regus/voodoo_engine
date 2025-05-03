#include "presentation/signal_handler.h"
#include "infrastructure/context.h"
#include "domain/logger.h"
#include "infrastructure/file_system_monitor.h"

#include <signal.h>
#include <unistd.h>
#include <stdint.h>

static void signal_handler(const int signum) {
    log_message(LOG_LEVEL_INFO, "Signal received: %d\n", signum);

    uint64_t val = 1;
    write(get_context()->shutdown_event_fd, &val, sizeof(val));

    get_context()->keep_running = 0;
}

void setup_signal_handlers(void) {
    signal(SIGTERM, signal_handler); // kill <pid>
    signal(SIGINT, signal_handler); // CTR-C
}