#pragma once

#include "infrastructure/context.h"
#include "domain/config.h"

typedef struct {
    GlobalContext* global_context;
    char fanotify_events_buffer[FANOTIFY_BUFFER_SIZE];
    size_t fanotify_events_buffer_len;
} FanotifyEventBatch;