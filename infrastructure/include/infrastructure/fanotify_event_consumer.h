#pragma once

#include "infrastructure/fanotify_event_batch.h"

#define FANOTIFY_EVENT_BATCH_TERMINATED ((FanotifyEventBatch*)-1)

void start_raw_event_consumer(void);