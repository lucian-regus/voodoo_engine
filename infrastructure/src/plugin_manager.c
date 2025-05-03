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

void load_plugins(void) {
    GlobalContext* global_context = get_context();

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

            void* plugin_handle = dlopen(full_path, RTLD_NOW);
            if (!plugin_handle) {
                log_message(LOG_LEVEL_ERROR, "dlopen failed for: %s -> %s\n", entry->d_name, dlerror());
                continue;
            }

            Plugin* (*get_plugin)() = dlsym(plugin_handle, "get_plugin");
            if (!get_plugin) {
                log_message(LOG_LEVEL_ERROR, "Missing get_plugin: %s\n", entry->d_name);
                dlclose(plugin_handle);
                continue;
            }

            Plugin* plugin = get_plugin();
            if (plugin == NULL) {
                log_message(LOG_LEVEL_ERROR, "Plugin is NULL\n");
                exit(1);
            }

            int init_result = plugin->init();
            if (init_result != 0) {
                log_message(LOG_LEVEL_ERROR, "Plugin '%s' failed to initialize (code: %d). Skipping...\n", plugin->name, init_result);
                dlclose(plugin_handle);
                continue;
            }

            log_message(LOG_LEVEL_INFO, "Loaded plugin: %s\n", plugin->name);

            global_context->loaded_plugins = g_list_append(global_context->loaded_plugins, plugin);
            global_context->loaded_plugins_handles = g_list_append(global_context->loaded_plugins_handles, plugin_handle);
        }
    }

    closedir(dir);
}

void unload_plugins(void) {
    GlobalContext* ctx = get_context();

    if (ctx->loaded_plugins) {
        for (GList* node = ctx->loaded_plugins; node != NULL; node = node->next) {
            Plugin* plugin = (Plugin*)node->data;
            if (plugin->cleanup) {
                plugin->cleanup();
            }
        }
    }

    if (ctx->loaded_plugins_handles) {
        for (GList* node = ctx->loaded_plugins_handles; node != NULL; node = node->next) {
            void* handle = node->data;
            if (handle) {
                dlclose(handle);
            }
        }
    }

    log_message(LOG_LEVEL_INFO, "All plugins successfully unloaded.\n");
}