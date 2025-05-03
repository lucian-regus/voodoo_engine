#include "domain/database.h"
#include "domain/config.h"
#include "domain/logger.h"

#include "stdio.h"
#include <libpq-fe.h>

void exit_on_error(DatabaseContext* database_context, PGconn* connection, const char* context) {
    const char* safe_context = context ? context : "Unknown error";
    const char* pg_error = connection ? PQerrorMessage(connection) : "No connection context";

    log_message(LOG_LEVEL_ERROR, "%s:  %s", safe_context, pg_error);

    g_async_queue_push(database_context->connections, connection);

    database_context_destroy(database_context);

    exit(EXIT_FAILURE);
}

DatabaseContext* database_context_create(void) {
    DatabaseContext* database_context = malloc(sizeof(DatabaseContext));
    if (!database_context) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for DatabaseContext");
        exit(EXIT_FAILURE);
    }

    database_context->connections = NULL;

    database_context->connections = g_async_queue_new();

    const char *user = getenv("DATABASE_USER");
    const char *password = getenv("DATABASE_PASSWORD");

    char connection_info[512];
    snprintf(connection_info, sizeof(connection_info),"host=localhost port=6432 dbname=voodoo_av user=%s password=%s", user, password);

    for (int i = 0 ; i < MAX_DATABASE_CONNECTIONS ; i++) {
        PGconn *conn = PQconnectdb(connection_info);

        if (PQstatus(conn) != CONNECTION_OK) {
            exit_on_error(database_context, conn, "database_context_create");
        }

        g_async_queue_push(database_context->connections, conn);
    }

    return database_context;
}

void database_context_destroy(DatabaseContext* database_context) {
    PGconn* conn;
    while ((conn = g_async_queue_try_pop(database_context->connections)) != NULL) {
        PQfinish(conn);
    }

    g_async_queue_unref(database_context->connections);

    free(database_context);
    database_context = NULL;
}

static const char** prepare_params(GList* params, int* param_count) {
    *param_count = g_list_length(params);
    if (*param_count == 0) {
        return NULL;
    }

    const char** param_values = g_new0(const char*, *param_count);
    int i = 0;
    for (GList* iter = params; iter != NULL; iter = iter->next, i++) {
        param_values[i] = (const char*)iter->data;
    }

    return param_values;
}

static PGresult* execute_query(PGconn* connection, const char* query, int param_count, const char** param_values) {
    return PQexecParams(connection,
                        query,
                        param_count,
                        NULL,
                        param_values,
                        NULL,
                        NULL,
                        0);
}

GList* run_query(DatabaseContext* database_context, const char *query, GList *params, RowMapperFunction row_mapper) {
    PGconn *connection = g_async_queue_pop(database_context->connections);

    int param_count = 0;
    const char** param_values = prepare_params(params, &param_count);

    PGresult* response = execute_query(connection, query, param_count, param_values);

    if (param_values) {
        g_free(param_values);
        g_list_free_full(params, g_free);
    }

    if (PQresultStatus(response) != PGRES_TUPLES_OK) {
        PQclear(response);
        exit_on_error(database_context, connection, "run_query");
    }

    GList* result = row_mapper(response);

    PQclear(response);
    g_async_queue_push(database_context->connections, connection);

    return result;
}

void run_non_query(DatabaseContext* database_context, const char *query, GList *params) {
    PGconn *connection = g_async_queue_pop(database_context->connections);

    int param_count = g_list_length(params);
    const char** param_values = prepare_params(params, &param_count);

    PGresult* response = execute_query(connection, query, param_count, param_values);

    if (param_values) {
        g_free(param_values);
        g_list_free_full(params, g_free);
    }

    if (PQresultStatus(response) != PGRES_COMMAND_OK) {
        PQclear(response);
        exit_on_error(database_context, connection, "run_non_query");
    }

    PQclear(response);
    g_async_queue_push(database_context->connections, connection);

    log_message(LOG_LEVEL_DEBUG, "Insert complete.");
}