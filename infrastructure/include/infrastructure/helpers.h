#pragma once

#include <stddef.h>
#include "infrastructure/context.h"

char* generate_uuid();
void split_path(const char* full_path, char* folder_out, size_t folder_size, char* file_out, size_t file_size);
char* generate_file_identity(const char* full_path);
int is_malware_allowed(GlobalContext* global_context, char* file_identity);