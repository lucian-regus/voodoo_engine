#include "infrastructure/context.h"
#include "infrastructure/helpers.h"
#include "domain/config.h"
#include "infrastructure/quarantine.h"

#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>

#include "domain/logger.h"


void register_quarantined_file(char* file_identity, char* original_folder, char* original_filename, char* new_filename, char* detection_plugin_name, int detected_malware_id) {
    GlobalContext* global_context = get_context();

    char detected_malware_id_str[12];
    snprintf(detected_malware_id_str, sizeof(detected_malware_id_str), "%d", detected_malware_id);

    GList *params = NULL;
    params = g_list_append(params, g_strdup(file_identity));
    params = g_list_append(params, g_strdup(original_folder));
    params = g_list_append(params, g_strdup(original_filename));
    params = g_list_append(params, g_strdup(new_filename));
    params = g_list_append(params, g_strdup(detection_plugin_name));
    params = g_list_append(params, g_strdup(detected_malware_id_str));

    run_non_query(global_context->database_context, "INSERT INTO malware_detection_log(file_identity, original_path, old_name, new_name, detected_by, detected_by_id) VALUES($1, $2, $3, $4, $5, $6)", params);
}

void move_file_to_quarantine_folder(const char* full_path, char* original_folder, char* original_filename, char* new_filename) {
    char* uuid = generate_uuid();
    if (uuid == NULL) {
        log_message(LOG_LEVEL_ERROR,"Failed to generate uuid\n");
        exit(1);
    }

    split_path(full_path, original_folder, PATH_MAX, original_filename, NAME_MAX);

    snprintf(new_filename, NAME_MAX, "%s", uuid);

    char quarantine_path[PATH_MAX];
    snprintf(quarantine_path, PATH_MAX, "%s%s", QUARANTINE_DIR, new_filename);

    if (rename(full_path, quarantine_path) != 0) {
        log_message(LOG_LEVEL_ERROR,"Failed to move file -> %s\n", original_filename);
    }

    free(uuid);
}

void quarantine_file(char* file_identity, const char* full_path, char* detection_plugin_name, int detected_malware_id) {
    char original_folder[PATH_MAX];
    char original_filename[NAME_MAX];
    char new_filename[NAME_MAX];

    move_file_to_quarantine_folder(full_path, original_folder, original_filename, new_filename);

    register_quarantined_file(file_identity, original_folder, original_filename, new_filename, detection_plugin_name, detected_malware_id);
}