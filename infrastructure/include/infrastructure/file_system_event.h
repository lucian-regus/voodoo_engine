#pragma once

#include <limits.h>

#include "context.h"

typedef struct {
    GlobalContext* global_context;

    char path[PATH_MAX];
    int event_type;
    int metadata_fd;
} FileSystemEvent;
