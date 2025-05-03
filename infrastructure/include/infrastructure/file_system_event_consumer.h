#pragma once

#include "file_system_event.h"

#define FILE_SYSTEM_EVENT_TERMINATE_SIGNAL ((FileSystemEvent*)-1)

void start_file_system_event_consumer(void);