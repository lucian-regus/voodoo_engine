#pragma once

typedef struct {
    char* name;
    int (*init)(void);
    int (*evaluate_file)(char* filepath);
    void (*cleanup)(void);
    void* handle;
} Plugin;

Plugin* get_plugin(void);