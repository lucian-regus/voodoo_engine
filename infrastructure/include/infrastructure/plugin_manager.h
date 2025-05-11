#pragma once

void load_plugins(void);
void unload_plugins(void);
int load_plugin(const char* plugin_name);
int unload_plugin(const char* plugin_name);