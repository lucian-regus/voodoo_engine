#include "infrastructure/file_system_monitor.h"
#include "infrastructure/fanotify_monitor.h"
#include "infrastructure/fanotify_event_consumer.h"
#include "infrastructure/file_system_event_consumer.h"

#include <pthread.h>

static pthread_t file_creations_thread, file_executions_thread, fanotify_event_consumer, file_system_event_consumer;

void start_file_system_monitoring(void) {
    pthread_create(&fanotify_event_consumer, NULL, (void*)start_raw_event_consumer, NULL);
    pthread_create(&file_system_event_consumer, NULL, (void*)start_file_system_event_consumer, NULL);

    pthread_create(&file_creations_thread, NULL, (void*)start_file_creations_monitoring, NULL);
    pthread_create(&file_executions_thread, NULL, (void*)start_file_executions_monitoring, NULL);
}

void stop_fanotify_monitoring(void) {
    pthread_join(file_creations_thread, NULL);
    pthread_join(file_executions_thread, NULL);
}

void stop_fanotify_event_consumer(void) {
    pthread_join(fanotify_event_consumer, NULL);
}

void stop_file_system_event_consumer(void) {
    pthread_join(file_system_event_consumer, NULL);
}