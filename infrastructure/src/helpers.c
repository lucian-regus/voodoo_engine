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
        if (errno == ENOENT) {
            //log_message(LOG_LEVEL_WARN, "File %s was deleted or rotated.\n", full_path);
        } else {
            log_message(LOG_LEVEL_ERROR, "stat() failed for %s: %s\n", full_path, strerror(errno));
        }

        return NULL;
    }

    char* file_identity = malloc(50);
    if (file_identity == NULL) {
        log_message(LOG_LEVEL_ERROR, " generate_file_identity malloc failed\n");
        return NULL;
    }

    snprintf(file_identity, 100, "%lu-%lu-%ld", (unsigned long)statbuf.st_dev, (unsigned long)statbuf.st_ino, (long)statbuf.st_ctime);

    return file_identity;
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