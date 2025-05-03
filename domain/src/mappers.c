#include "domain/mappers.h"

GList* id_row_mapper(const PGresult* result){
    GList* list = NULL;
    int nrows = PQntuples(result);

    for (int i = 0; i < nrows; i++) {
        char* id_value = PQgetvalue(result, i, 0);
        if (id_value) {
            list = g_list_append(list, g_strdup(id_value));
        }
    }

    return list;
}

GList* exists_row_mapper(const PGresult* result) {
    if (PQntuples(result) > 0) {
        return g_list_append(NULL, GINT_TO_POINTER(1));
    }

    return NULL;
}