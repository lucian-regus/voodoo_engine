#pragma once

void start_file_system_monitoring(void);
void stop_fanotify_monitoring(void);
void stop_fanotify_event_consumer(void);
void stop_file_system_event_consumer(void);