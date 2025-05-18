#include "domain/config.h"
#include "infrastructure/plugin_interface.h"
#include "../include/infrastructure/unix_control_server.h"
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>

#include "domain/logger.h"
#include "infrastructure/context.h"
#include "infrastructure/plugin_manager.h"

static pthread_t control_thread;

static void handle_list(int client_fd) {
    GlobalContext *ctx = get_context();
    if (!ctx->loaded_plugins) {
        dprintf(client_fd, "No plugins loaded.\n");
    }
    else {
        for (GList *p = ctx->loaded_plugins; p; p = p->next) {
            Plugin *plg = (Plugin *)p->data;
            dprintf(client_fd, "- %s\n", plg->name);
        }
    }
}

static void handle_load(int client_fd, char* arg) {
    if (arg) {
        int result = load_plugin(arg);

        if (result == 0) {
            dprintf(client_fd, "Successfully loaded plugin: %s\n", arg);
        }
        else if (result == 1) {
            dprintf(client_fd, "Plugin '%s' already loaded\n", arg);
        }
        else if (result == 2) {
            dprintf(client_fd, "No plugin named '%s' was found\n", arg);
        }
    } else {
        dprintf(client_fd, "Missing plugin name.\n");
    }
}

static void handle_unload(int client_fd, char* arg) {
    if (arg) {
        if (unload_plugin(arg) == 0) {
            dprintf(client_fd, "Successfully unloaded plugin: %s\n", arg);
        }
        else {
            dprintf(client_fd, "No plugin named '%s' was found\n", arg);
        }
    }
    else {
        dprintf(client_fd, "Missing plugin name.\n");
    }
}

static void handle_client(int client_fd) {
    char buffer[1024] = {0};
    ssize_t len = read(client_fd, buffer, sizeof(buffer) - 1);

    buffer[len] = '\0';

    char *command = strtok(buffer, " \n");
    char *arg = strtok(NULL, "\n");

    if (strcmp(command, "list") == 0) {
        handle_list(client_fd);
    }
    else if (strcmp(command, "load") == 0) {
        handle_load(client_fd, arg);
    }
    else if (strcmp(command, "unload") == 0) {
        handle_unload(client_fd, arg);
    }
    else {
        dprintf(client_fd, "Unknown command: %s\n", command);
    }

    close(client_fd);
}

void control_server_thread() {
    GlobalContext* global_context = get_context();

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    int server_fd;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        log_message(LOG_LEVEL_ERROR, "socket() failed: %s.\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        log_message(LOG_LEVEL_ERROR, "bind() failed: %s.\n", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        log_message(LOG_LEVEL_ERROR, "listen() failed: %s.\n", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    log_message(LOG_LEVEL_INFO, "UNIX socket server started at %s\n", SOCKET_PATH);

    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;

    while (global_context->keep_running) {
        int ret = poll(&pfd, 1, 500);

        if (ret == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd == -1) {
                log_message(LOG_LEVEL_ERROR, "accept() failed: %s.\n", strerror(errno));
                continue;
            }

            handle_client(client_fd);
        }
    }

    close(server_fd);
    unlink(SOCKET_PATH);
}

void start_unix_control_server() {
    pthread_create(&control_thread, NULL, (void*)control_server_thread, NULL);
}

void stop_unix_control_server() {
    log_message(LOG_LEVEL_INFO, "UNIX socket server shut down\n");
    pthread_join(control_thread, NULL);
}