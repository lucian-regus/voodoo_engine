#include "infrastructure/helpers.h"
#include "domain/database.h"
#include "domain/mappers.h"
#include "domain/logger.h"

#include <libgen.h>
#include <stdio.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "infrastructure/context.h"

char* generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);

    char* uuid_str = malloc(37);
    if (uuid_str == NULL) {
        return NULL;
    }

    uuid_unparse(uuid, uuid_str);

    return uuid_str;
}

void split_path(const char* full_path, char* folder_out, size_t folder_size, char* file_out, size_t file_size) {
    char path_copy1[PATH_MAX];
    char path_copy2[PATH_MAX];

    strncpy(path_copy1, full_path, PATH_MAX - 1);
    path_copy1[PATH_MAX - 1] = '\0';

    strncpy(path_copy2, full_path, PATH_MAX - 1);
    path_copy2[PATH_MAX - 1] = '\0';

    snprintf(folder_out, folder_size, "%s", dirname(path_copy1));

    snprintf(file_out, file_size, "%s", basename(path_copy2));
}

char* generate_file_identity(const char* full_path) {
    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0) {
        log_message(LOG_LEVEL_ERROR, "stat() failed %s\n", full_path);
        return NULL;
    }

    char* output_hash = malloc(50);
    if (output_hash == NULL) {
        log_message(LOG_LEVEL_ERROR, " generate_file_identity malloc failed\n");
        return NULL;
    }

    snprintf(output_hash, 50, "%lu-%lu", (unsigned long)statbuf.st_dev, (unsigned long)statbuf.st_ino);

    return output_hash;
}

int is_malware_allowed(GlobalContext* global_context, char* file_identity) {
    GList *params = NULL;
    params = g_list_append(params, g_strdup(file_identity));
    GList* result = run_query(global_context->database_context, "SELECT 1 FROM malware_detection_log WHERE file_identity = $1 AND allowed_at IS NOT NULL LIMIT 1", params, exists_row_mapper);

    if (result == NULL) {
        return 0;
    }

    g_list_free(result);
    return 1;
}