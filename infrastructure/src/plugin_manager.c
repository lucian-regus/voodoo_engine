#include "infrastructure/plugin_manager.h"
#include "infrastructure/context.h"
#include "domain/config.h"
#include "domain/logger.h"
#include "infrastructure/plugin_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>

static int load_plugin_from_file(const char* full_path, const char* requested_name) {
    GlobalContext* ctx = get_context();

    void* plugin_handle = dlopen(full_path, RTLD_NOW);
    if (!plugin_handle) {
        log_message(LOG_LEVEL_ERROR, "dlopen failed for: %s -> %s\n", full_path, dlerror());
        return 0;
    }

    Plugin* (*get_plugin)() = dlsym(plugin_handle, "get_plugin");
    if (!get_plugin) {
        log_message(LOG_LEVEL_ERROR, "Missing get_plugin symbol: %s\n", full_path);
        dlclose(plugin_handle);
        return 0;
    }

    Plugin* plugin = get_plugin();
    if (!plugin) {
        log_message(LOG_LEVEL_ERROR, "get_plugin returned NULL: %s\n", full_path);
        dlclose(plugin_handle);
        return 0;
    }

    plugin->handle = plugin_handle;

    if (requested_name && strcmp(plugin->name, requested_name) != 0) {
        dlclose(plugin_handle);
        return 0;
    }

    int init_result = plugin->init();
    if (init_result != 0) {
        log_message(LOG_LEVEL_ERROR, "Plugin '%s' failed to initialize (code: %d)\n", plugin->name, init_result);
        dlclose(plugin_handle);
        return 0;
    }

    log_message(LOG_LEVEL_INFO, "Loaded plugin: %s\n", plugin->name);
    ctx->loaded_plugins = g_list_append(ctx->loaded_plugins, plugin);
    return 1;
}

void load_plugins(void) {
    char full_path[PATH_MAX];
    DIR* dir = opendir(PLUGIN_DIR);

    if (!dir) {
        log_message(LOG_LEVEL_ERROR, "Plugin folder missing\n");
        exit(1);
    }

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        if (strstr(entry->d_name, ".so")) {
            snprintf(full_path, sizeof(full_path), "%s/%s", PLUGIN_DIR, entry->d_name);
            load_plugin_from_file(full_path, NULL);
        }
    }

    closedir(dir);
}

int load_plugin(const char* plugin_name) {
    GlobalContext* ctx = get_context();

    for (GList* node = ctx->loaded_plugins; node; node = node->next) {
        Plugin* plugin = (Plugin*)node->data;
        if (strcmp(plugin->name, plugin_name) == 0) {
            return 1;
        }
    }

    char full_path[PATH_MAX];
    DIR* dir = opendir(PLUGIN_DIR);
    if (!dir) {
        log_message(LOG_LEVEL_ERROR, "Plugin folder missing\n");
        exit(1);
    }

    struct dirent* entry;
    int loaded = 0;
    while ((entry = readdir(dir)) && !loaded) {
        if (strstr(entry->d_name, ".so")) {
            snprintf(full_path, sizeof(full_path), "%s/%s", PLUGIN_DIR, entry->d_name);
            loaded = load_plugin_from_file(full_path, plugin_name);
        }
    }

    closedir(dir);
    return loaded ? 0 : 2;
}

void unload_plugins(void) {
    GlobalContext* ctx = get_context();

    if (ctx->loaded_plugins) {
        for (GList* node = ctx->loaded_plugins; node != NULL; node = node->next) {
            Plugin* plugin = (Plugin*)node->data;

            if (plugin->cleanup) {
                plugin->cleanup();
                dlclose(plugin->handle);
            }
        }
    }

    log_message(LOG_LEVEL_INFO, "All plugins successfully unloaded.\n");
}

int unload_plugin(const char* plugin_name) {
    GlobalContext* ctx = get_context();

    if (ctx->loaded_plugins) {
        for (GList* node = ctx->loaded_plugins; node != NULL; node = node->next) {
            Plugin* plugin = (Plugin*)node->data;

            if (strcmp(plugin->name, plugin_name) == 0) {
                plugin->cleanup();
                dlclose(plugin->handle);

                ctx->loaded_plugins = g_list_delete_link(ctx->loaded_plugins, node);

                return 0;
            }
        }
    }

    return 1;
}