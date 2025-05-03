#pragma once

#include "domain/mappers.h"

#include "glib.h"

typedef struct {
    GAsyncQueue* connections;
} DatabaseContext;

DatabaseContext* database_context_create(void);
void database_context_destroy(DatabaseContext* database_context);

GList* run_query(DatabaseContext* database_context, const char *query, GList *params, RowMapperFunction row_mapper);
void run_non_query(DatabaseContext* database_context, const char *query, GList *params);