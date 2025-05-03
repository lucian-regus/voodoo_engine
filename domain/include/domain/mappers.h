#pragma once

#include "glib.h"
#include <libpq-fe.h>

typedef GList* (*RowMapperFunction)(const PGresult*);

GList* id_row_mapper(const PGresult* result);
GList* exists_row_mapper(const PGresult* result);