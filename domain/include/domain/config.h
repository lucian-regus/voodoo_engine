#pragma once

#define MAX_DATABASE_CONNECTIONS 50
#define MAX_LOADED_PLUGINS 10
#define FANOTIFY_BUFFER_SIZE 1024
#define FANOTIFY_THREAD_POOL_SIZE 10

#define PLUGIN_DIR "/usr/lib/voodoo/plugins"
#define QUARANTINE_DIR "/var/lib/voodoo/quarantine/"
#define LOG_FILE "/var/log/voodoo/voodoo_engine.log"

#define SOCKET_PATH "/tmp/voodoo.sock"