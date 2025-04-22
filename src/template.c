#include "template.h"

char* replace_placeholders(char* result, const char** keys, const char** values, int num_pairs) {
    if (!result) return NULL;

    for (int i = 0; i < num_pairs; i++) {
        char placeholder[64];
        snprintf(placeholder, sizeof(placeholder), "{{%s}}", keys[i]);

        char* position;
        while ((position = strstr(result, placeholder)) != NULL) {
            size_t len_before = position - result;
            size_t len_key = strlen(placeholder);
            size_t len_value = strlen(values[i]);
            char* new_result = malloc(len_before + len_value + strlen(position + len_key) + 1);
            if (!new_result) {
                perror("Memory allocation failed");
                return result;
            }
            strncpy(new_result, result, len_before);
            strcpy(new_result + len_before, values[i]);
            strcpy(new_result + len_before + len_value, position + len_key);

            free(result);
            result = new_result;
        }
    }
    return result;
}

char* process_if_else(char* result, const char** keys, const char** values, int num_pairs) {
    if (!result) return NULL;

    for (int i = 0; i < num_pairs; i++) {
        if (!keys[i] || !values[i]) {
            fprintf(stderr, "Warning: Null key or value in process_if_else at index %d\n", i);
            continue;
        }
        
        char condition[64];
        if (strlen(keys[i]) > 50) {
            fprintf(stderr, "Warning: Key too long for condition: %s\n", keys[i]);
            continue;
        }
        snprintf(condition, sizeof(condition), "{%% if %s %%}", keys[i]);

        char* if_position;
        while ((if_position = strstr(result, condition)) != NULL) {
            char* endif_position = strstr(if_position, "{% endif %}");
            if (!endif_position) {
                fprintf(stderr, "Error: Missing endif for condition: %s\n", condition);
                if_position += strlen(condition);
                continue;
            }
            
            char* else_position = strstr(if_position, "{% else %}");
            
            if (else_position && else_position > endif_position) {
                else_position = NULL;
            }
            
            char* temp_if = if_position + strlen(condition);
            char* nested_if = strstr(temp_if, "{% if");
            while (nested_if && nested_if < endif_position) {
                char* next_endif = strstr(endif_position + 9, "{% endif %}");
                if (!next_endif) break;
                endif_position = next_endif;
                temp_if = nested_if + 5;
                nested_if = strstr(temp_if, "{% if");
            }
            
            char* content_if = NULL;
            char* content_else = NULL;
            
            if (else_position && else_position < endif_position) {
                char* block_start = if_position + strlen(condition);
                char* block_end = else_position;

                size_t if_content_len = block_end - block_start;
                if (if_content_len > 0 && if_content_len < 1000000) {
                    content_if = strndup(block_start, if_content_len);
                } else {
                    fprintf(stderr, "Error: Invalid if content length: %zu\n", if_content_len);
                    content_if = strdup("");
                }
                
                size_t else_content_len = endif_position - (else_position + strlen("{% else %}"));
                if (else_content_len > 0 && else_content_len < 1000000) {
                    content_else = strndup(else_position + strlen("{% else %}"), else_content_len);
                } else {
                    fprintf(stderr, "Error: Invalid else content length: %zu\n", else_content_len);
                    content_else = strdup("");
                }
            } else {
                size_t content_len = endif_position - (if_position + strlen(condition));
                if (content_len > 0 && content_len < 1000000) {
                    content_if = strndup(if_position + strlen(condition), content_len);
                } else {
                    fprintf(stderr, "Error: Invalid if content length: %zu\n", content_len);
                    content_if = strdup("");
                }
                content_else = strdup("");
            }

                if (!content_if || !content_else) {
                    perror("Memory allocation failed for if-else content");
                    free(content_if);
                    free(content_else);
                if_position += strlen(condition);
                continue;
            }

            bool condition_true = false;
            const char* value = values[i];
            
            if (value && *value) {
                if (strcmp(value, "1") == 0 || 
                    strcasecmp(value, "true") == 0 || 
                    strcasecmp(value, "yes") == 0 || 
                    strcasecmp(value, "y") == 0 ||
                    strcasecmp(value, "on") == 0) {
                    condition_true = true;
                } 
                else if (strcmp(value, "0") == 0 || 
                         strcasecmp(value, "false") == 0 || 
                         strcasecmp(value, "no") == 0 || 
                         strcasecmp(value, "n") == 0 ||
                         strcasecmp(value, "off") == 0) {
                    condition_true = false;
                }
                else {
                    condition_true = true;
                }
            }

            char* replacement_content = condition_true ? content_if : content_else;

                size_t len_before = if_position - result;
            size_t len_replacement = strlen(replacement_content);
                size_t len_after = strlen(endif_position + strlen("{% endif %}"));
            
            if (len_before > strlen(result) || len_after > strlen(result) || 
                len_before + len_replacement + len_after > 1000000) {
                fprintf(stderr, "Error: Invalid buffer lengths in if-else processing\n");
                free(content_if);
                free(content_else);
                if_position += strlen(condition);
                continue;
            }
            
            char* new_result = malloc(len_before + len_replacement + len_after + 1);
                if (!new_result) {
                    perror("Memory allocation failed for new result");
                    free(content_if);
                    free(content_else);
                    return result;
                }
            
                strncpy(new_result, result, len_before);
            new_result[len_before] = '\0';
            strcat(new_result, replacement_content);
            strcat(new_result, endif_position + strlen("{% endif %}"));
            
                free(result); 
                result = new_result; 
                free(content_if); 
                free(content_else); 
        }
    }
    return result;
}

char* get_item_part(const char* item, char delimiter, int part_index) {
    if (!item) return NULL;
    
    const char* start = item;
    int current_part = 0;
    
    while (*start && current_part < part_index) {
        if (*start == delimiter) {
            current_part++;
        }
        start++;
    }
    
    if (current_part != part_index) {
        return strdup(item);
    }
    
    const char* end = start;
    while (*end && *end != delimiter) {
        end++;
    }
    
    size_t part_len = end - start;
    char* part = malloc(part_len + 1);
    if (!part) {
        perror("Memory allocation failed for item part");
        return NULL;
    }
    
    strncpy(part, start, part_len);
    part[part_len] = '\0';
    
    return part;
}

char* process_loops(char* result, const char* loop_key, const char** loop_values, int loop_count) {
    if (!result || !loop_key || !loop_values || loop_count <= 0) {
        return result;
    }
    
    char loop_start[128];
    snprintf(loop_start, sizeof(loop_start), "{%% for %s in items %%}", loop_key);
    
    char cond_loop_start_prefix[64];
    snprintf(cond_loop_start_prefix, sizeof(cond_loop_start_prefix), "{%% for %s in items if ", loop_key);
    
    char* loop_position;

    const char* pos = result;
    while ((pos = strstr(pos, cond_loop_start_prefix)) != NULL) {
        
        const char* cond_end = strstr(pos, " %}");
        if (!cond_end) {
            pos += strlen(cond_loop_start_prefix);
            continue;
        }
        
        size_t cond_len = cond_end - (pos + strlen(cond_loop_start_prefix));
        char* condition = strndup(pos + strlen(cond_loop_start_prefix), cond_len);
        if (!condition) {
            pos = cond_end + 3;
            continue;
        }
        
        char specific_cond_pattern[256];
        snprintf(specific_cond_pattern, sizeof(specific_cond_pattern), 
                "{%% for %s in items if %s %%}", loop_key, condition);
        
        char* cond_loop_position = strstr(result, specific_cond_pattern);
        if (!cond_loop_position) {
            free(condition);
            pos = cond_end + 3;
            continue;
        }
        
        char* end_loop_position = strstr(cond_loop_position, "{% endfor %}");
        if (!end_loop_position) {
            free(condition);
            pos = cond_end + 3;
            continue;
        }

        char* temp_for = cond_loop_position + strlen(specific_cond_pattern);
        char* nested_for = strstr(temp_for, "{% for");
        while (nested_for && nested_for < end_loop_position) {
            char* next_endfor = strstr(end_loop_position + 12, "{% endfor %}");
            if (!next_endfor) break;
            end_loop_position = next_endfor;
            temp_for = nested_for + 7;
            nested_for = strstr(temp_for, "{% for");
        }

        size_t len_before = cond_loop_position - result;
        size_t len_after = strlen(end_loop_position + strlen("{% endfor %}"));
        size_t loop_content_length = end_loop_position - (cond_loop_position + strlen(specific_cond_pattern));
        
        char* loop_content = strndup(cond_loop_position + strlen(specific_cond_pattern), loop_content_length);
        if (!loop_content) {
            free(condition);
            pos = cond_end + 3;
            continue;
        }
        
        bool* include_item = malloc(loop_count * sizeof(bool));
        if (!include_item) {
            free(condition);
            free(loop_content);
            pos = cond_end + 3;
            continue;
        }
        
        for (int i = 0; i < loop_count; i++) {
            include_item[i] = false;
        }
        
        int filtered_count = 0;
        
        for (int i = 0; i < loop_count; i++) {
            const char* item_value = loop_values[i];
            bool condition_met = false;
            
            if (strstr(condition, ".") != NULL) {
                char field_part[64] = {0};
                char* dot_pos = strstr(condition, ".");
                if (dot_pos) {
                    const char* field_start = dot_pos + 1;
                    const char* op_pos = strstr(field_start, "==");
                    if (!op_pos) op_pos = strstr(field_start, "!=");
                    
                    if (op_pos) {
                        size_t field_len = op_pos - field_start;
                        strncpy(field_part, field_start, field_len < 63 ? field_len : 63);
                        field_part[field_len < 63 ? field_len : 63] = '\0';
                        
                        char* p = field_part;
                        while (*p == ' ' || *p == '\t') p++;
                        memmove(field_part, p, strlen(p) + 1);
                        
                        p = field_part + strlen(field_part) - 1;
                        while (p >= field_part && (*p == ' ' || *p == '\t')) {
                            *p-- = '\0';
                        }
                        
                        int field_idx = atoi(field_part);
                        char* field_value = get_item_part(item_value, '|', field_idx);
                        
                        if (field_value) {
                            const char* val_start = NULL;
                            bool is_equality = true;
                            
                            if (strstr(condition, "==") != NULL) {
                                val_start = strstr(condition, "==") + 2;
                                is_equality = true;
                            } else if (strstr(condition, "!=") != NULL) {
                                val_start = strstr(condition, "!=") + 2;
                                is_equality = false;
                            }
                            
                            if (val_start) {
                                while (*val_start == ' ' || *val_start == '\t') val_start++;
                                
                                char compare_value[128] = {0};
                                if (*val_start == '"' || *val_start == '\'') {
                                    char quote = *val_start;
                                    val_start++;
                                    const char* quote_end = strchr(val_start, quote);
                                    if (quote_end) {
                                        size_t val_len = quote_end - val_start;
                                        strncpy(compare_value, val_start, val_len < 127 ? val_len : 127);
                                    }
                                } else {
                                    const char* space = strpbrk(val_start, " \t\r\n");
                                    size_t val_len = space ? space - val_start : strlen(val_start);
                                    strncpy(compare_value, val_start, val_len < 127 ? val_len : 127);
                                }
                                
                                int comparison_result = strcmp(field_value, compare_value);
                                condition_met = is_equality ? (comparison_result == 0) : (comparison_result != 0);
                            }
                            
                            free(field_value);
                        }
                    }
                }
            } 
            else if (strstr(condition, "==") != NULL || strstr(condition, "!=") != NULL) {
                bool is_equality = strstr(condition, "==") != NULL;
                const char* op_str = is_equality ? "==" : "!=";
                const char* val_start = strstr(condition, op_str) + strlen(op_str);
                
                while (*val_start == ' ' || *val_start == '\t') val_start++;
                
                char compare_value[128] = {0};
                if (*val_start == '"' || *val_start == '\'') {
                    char quote = *val_start;
                    val_start++;
                    const char* quote_end = strchr(val_start, quote);
                    if (quote_end) {
                        size_t val_len = quote_end - val_start;
                        strncpy(compare_value, val_start, val_len < 127 ? val_len : 127);
                    }
                } else {
                    const char* space = strpbrk(val_start, " \t\r\n");
                    size_t val_len = space ? space - val_start : strlen(val_start);
                    strncpy(compare_value, val_start, val_len < 127 ? val_len : 127);
                }
                
                int comparison_result = strcmp(item_value, compare_value);
                condition_met = is_equality ? (comparison_result == 0) : (comparison_result != 0);
            }
            
            include_item[i] = condition_met;
            if (condition_met) {
                filtered_count++;
            }
        }
        
        size_t loop_expanded_size = filtered_count * (loop_content_length + 100);
        char* new_result = malloc(len_before + loop_expanded_size + len_after + 1);
        if (!new_result) {
            free(condition);
            free(loop_content);
            free(include_item);
            pos = cond_end + 3;
            continue;
        }
        
        strncpy(new_result, result, len_before);
        new_result[len_before] = '\0';
        size_t current_position = len_before;
        
        for (int i = 0; i < loop_count; i++) {
            if (!include_item[i]) {
                continue;
            }
            
            bool has_parts = false;
            const char* item_value = loop_values[i];
            
            if (item_value && strchr(item_value, '|') != NULL) {
                has_parts = true;
            }
            
            char* loop_item_content = strdup(loop_content);
            if (!loop_item_content) {
                free(condition);
                free(loop_content);
                free(include_item);
                free(new_result);
                pos = cond_end + 3;
                return result;
            }
            
            const char* keys[] = {loop_key};
            const char* values[] = {loop_values[i]};
            loop_item_content = replace_placeholders(loop_item_content, keys, values, 1);
            
            if (has_parts) {
                char part_key[64];
                char* temp_content = loop_item_content;
                
                for (int part_idx = 0; part_idx < 10; part_idx++) {
                    snprintf(part_key, sizeof(part_key), "{{%s.%d}}", loop_key, part_idx);
                    
                    char* part_pos;
                    while ((part_pos = strstr(temp_content, part_key)) != NULL) {
                        char* part_value = get_item_part(item_value, '|', part_idx);
                        if (!part_value) continue;
                        
                        size_t len_before_part = part_pos - temp_content;
                        size_t len_key = strlen(part_key);
                        size_t len_value = strlen(part_value);
                        
                        char* new_content = malloc(len_before_part + len_value + 
                                                 strlen(part_pos + len_key) + 1);
                        if (!new_content) {
                            free(part_value);
                            continue;
                        }
                        
                        strncpy(new_content, temp_content, len_before_part);
                        new_content[len_before_part] = '\0';
                        strcat(new_content, part_value);
                        strcat(new_content, part_pos + len_key);
                        
                        free(part_value);
                        free(temp_content);
                        temp_content = new_content;
                    }
                }
                
                loop_item_content = temp_content;
            }
            
            strcat(new_result + current_position, loop_item_content);
            current_position += strlen(loop_item_content);
            free(loop_item_content);
        }
        
        strcat(new_result + current_position, end_loop_position + strlen("{% endfor %}"));

        free(loop_content);
        free(condition);
        free(include_item);
        free(result); 
        result = new_result;
        
        pos = cond_end + 3;
    }

    while ((loop_position = strstr(result, loop_start)) != NULL) {
        char* end_loop_position = strstr(loop_position, "{% endfor %}");
        if (!end_loop_position) {
            break;
        }

        char* temp_for = loop_position + strlen(loop_start);
        char* nested_for = strstr(temp_for, "{% for");
        while (nested_for && nested_for < end_loop_position) {
            char* next_endfor = strstr(end_loop_position + 12, "{% endfor %}");
            if (!next_endfor) break;
            end_loop_position = next_endfor;
            temp_for = nested_for + 7;
            nested_for = strstr(temp_for, "{% for");
        }

        size_t len_before = loop_position - result;
        size_t len_after = strlen(end_loop_position + strlen("{% endfor %}"));
        size_t loop_content_length = end_loop_position - (loop_position + strlen(loop_start));
        
        char* loop_content = strndup(loop_position + strlen(loop_start), loop_content_length);
        if (!loop_content) {
            perror("Memory allocation failed for loop content");
            return result;
        }
        
        size_t loop_expanded_size = loop_count * (loop_content_length + 100);
        char* new_result = malloc(len_before + loop_expanded_size + len_after + 1);
        if (!new_result) {
            perror("Memory allocation failed for new result");
            free(loop_content);
            return result; 
        }
        
        strncpy(new_result, result, len_before);
        new_result[len_before] = '\0';
        size_t current_position = len_before;
        
        for (int i = 0; i < loop_count; i++) {
            bool has_parts = false;
            const char* item_value = loop_values[i];
            
            if (item_value && strchr(item_value, '|') != NULL) {
                has_parts = true;
            }
            
            char* loop_item_content = strdup(loop_content);
            if (!loop_item_content) {
                perror("Memory allocation failed for loop item content");
                free(loop_content);
                free(new_result);
                return result;
            }
            
            const char* keys[] = {loop_key};
            const char* values[] = {loop_values[i]};
            loop_item_content = replace_placeholders(loop_item_content, keys, values, 1);
            
            if (has_parts) {
                char part_key[64];
                char* temp_content = loop_item_content;
                
                for (int part_idx = 0; part_idx < 10; part_idx++) {
                    snprintf(part_key, sizeof(part_key), "{{%s.%d}}", loop_key, part_idx);
                    
                    char* part_pos;
                    while ((part_pos = strstr(temp_content, part_key)) != NULL) {
                        char* part_value = get_item_part(item_value, '|', part_idx);
                        if (!part_value) continue;
                        
                        size_t len_before_part = part_pos - temp_content;
                        size_t len_key = strlen(part_key);
                        size_t len_value = strlen(part_value);
                        
                        char* new_content = malloc(len_before_part + len_value + 
                                                 strlen(part_pos + len_key) + 1);
                        if (!new_content) {
                            perror("Memory allocation failed for part replacement");
                            free(part_value);
                            continue;
                        }
                        
                        strncpy(new_content, temp_content, len_before_part);
                        new_content[len_before_part] = '\0';
                        strcat(new_content, part_value);
                        strcat(new_content, part_pos + len_key);
                        
                        free(part_value);
                        free(temp_content);
                        temp_content = new_content;
                    }
                }
                
                loop_item_content = temp_content;
            }
            
            strcat(new_result + current_position, loop_item_content);
            current_position += strlen(loop_item_content);
            free(loop_item_content);
        }
        
        strcat(new_result + current_position, end_loop_position + strlen("{% endfor %}"));

        free(loop_content); 
        free(result); 
        result = new_result;
    }
    return result;
}

char* process_template(const char* template, const char** keys, const char** values, int num_pairs, const char* loop_key, const char** loop_values, int loop_count) {
    if (!template) return NULL;
    
    printf("Processing template with %d key-value pairs\n", num_pairs);
    
    if (keys == NULL || values == NULL) {
        printf("Warning: NULL keys or values array passed to process_template\n");
        return strdup(template);
    }
    
    for (int i = 0; i < num_pairs; i++) {
        if (keys[i] == NULL) {
            printf("Warning: NULL key at index %d\n", i);
            continue;
        }
    }
    
    char* result = strdup(template);
    if (!result) {
        perror("Failed to allocate memory for template processing");
        return NULL;
    }
    
    printf("Applying placeholder replacements...\n");
    char* temp = replace_placeholders(result, keys, values, num_pairs);
    if (temp) {
        result = temp;
    } else {
        printf("Warning: Placeholder replacement failed, continuing with original content\n");
    }
    
    printf("Applying conditional logic...\n");
    temp = process_if_else(result, keys, values, num_pairs);
    if (temp) {
        result = temp;
    } else {
        printf("Warning: If-else processing failed, continuing with current content\n");
    }
    
    if (loop_key && loop_values && loop_count > 0) {
        printf("Applying loops for key '%s' with %d items...\n", loop_key, loop_count);
    temp = process_loops(result, loop_key, loop_values, loop_count);
        if (temp) {
            result = temp;
        } else {
            printf("Warning: Loop processing failed, continuing with current content\n");
        }
    }
    
    printf("Template processing complete\n");
    return result;
}

template_data_t* init_template_data() {
    template_data_t* data = malloc(sizeof(template_data_t));
    if (!data) {
        perror("Failed to allocate memory for template data");
        return NULL;
    }
    
    data->capacity = 10;
    data->count = 0;
    data->keys = malloc(data->capacity * sizeof(char*));
    data->values = malloc(data->capacity * sizeof(char*));
    
    if (!data->keys || !data->values) {
        if (data->keys) free(data->keys);
        if (data->values) free(data->values);
        free(data);
        perror("Failed to allocate memory for template data arrays");
        return NULL;
    }
    
    return data;
}

void add_template_var(template_data_t* data, const char* key, const char* value) {
    if (!data || !key || !value) return;
    
    for (int i = 0; i < data->count; i++) {
        if (strcmp(data->keys[i], key) == 0) {
            free(data->values[i]);
            data->values[i] = strdup(value);
            return;
        }
    }
    
    if (data->count >= data->capacity) {
        int new_capacity = data->capacity * 2;
        char** new_keys = realloc(data->keys, new_capacity * sizeof(char*));
        char** new_values = realloc(data->values, new_capacity * sizeof(char*));
        
        if (!new_keys || !new_values) {
            if (new_keys) data->keys = new_keys;
            if (new_values) data->values = new_values;
            fprintf(stderr, "Warning: Memory allocation failed, not adding new template variable\n");
            return;
        }
        
        data->keys = new_keys;
        data->values = new_values;
        data->capacity = new_capacity;
    }
    
    data->keys[data->count] = strdup(key);
    data->values[data->count] = strdup(value);
    data->count++;
}

void free_template_data(template_data_t* data) {
    if (!data) return;
    
    for (int i = 0; i < data->count; i++) {
        free(data->keys[i]);
        free(data->values[i]);
    }
    
    free(data->keys);
    free(data->values);
    free(data);
}

template_data_t* parse_template_variables(const char* template_str) {
    if (!template_str) return NULL;
    
    template_data_t* data = init_template_data();
    if (!data) return NULL;
    
    const char* comment_start = "<!-- template:var";
    const char* comment_end = "-->";
    const char* pos = template_str;
    
    while ((pos = strstr(pos, comment_start)) != NULL) {
        pos += strlen(comment_start);
        
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) {
            pos++;
        }
        
        const char* end_pos = strstr(pos, comment_end);
        if (!end_pos) break;
        
        size_t var_def_len = end_pos - pos;
        char* var_def = strndup(pos, var_def_len);
        if (!var_def) {
            fprintf(stderr, "Warning: Memory allocation failed for variable definition\n");
            pos = end_pos + strlen(comment_end);
            continue;
        }
        
        char* end = var_def + strlen(var_def) - 1;
        while (end > var_def && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
            *end-- = '\0';
        }
        
        char* current_pos = var_def;
        while (*current_pos) {
            while (*current_pos && (*current_pos == ' ' || *current_pos == '\t' || *current_pos == '\n' || *current_pos == '\r')) {
                current_pos++;
            }
            
            if (!*current_pos) break;
            
            char* key_start = current_pos;
            while (*current_pos && *current_pos != '=') {
                current_pos++;
            }
            
            if (!*current_pos) break;
            
            *current_pos = '\0';
            char* key = key_start;
            current_pos++;
            
            char* key_end = key + strlen(key) - 1;
            while (key_end > key && (*key_end == ' ' || *key_end == '\t' || *key_end == '\n' || *key_end == '\r')) {
                *key_end-- = '\0';
            }
            
            char* value_start = current_pos;
            bool in_quotes = false;
            char quote_char = '\0';
            
            if (*value_start == '"' || *value_start == '\'') {
                in_quotes = true;
                quote_char = *value_start;
                value_start++;
                current_pos++;
            }
            
            char* value_end = value_start;
            
            if (in_quotes) {
                while (*current_pos && *current_pos != quote_char) {
                    value_end = current_pos;
                    current_pos++;
                }
                
                if (*current_pos == quote_char) {
                    current_pos++;
                }
            } else {
                while (*current_pos && *current_pos != ' ' && *current_pos != '\t' && *current_pos != '\n' && *current_pos != '\r') {
                    value_end = current_pos;
                    current_pos++;
                }
            }
            
            char saved_char = *(value_end + 1);
            *(value_end + 1) = '\0';
            
            add_template_var(data, key, value_start);
            
            *(value_end + 1) = saved_char;
        }
        
        free(var_def);
        pos = end_pos + strlen(comment_end);
    }
    
    pos = template_str;
    const char* items_start = "<!-- template:items";
    while ((pos = strstr(pos, items_start)) != NULL) {
        pos += strlen(items_start);
        
        while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) {
            pos++;
        }
        
        const char* end_pos = strstr(pos, comment_end);
        if (!end_pos) break;
        
        size_t items_def_len = end_pos - pos;
        char* items_def = strndup(pos, items_def_len);
        if (!items_def) {
            fprintf(stderr, "Warning: Memory allocation failed for items definition\n");
            pos = end_pos + strlen(comment_end);
            continue;
        }
        
        int item_count = 0;
        char* items[100];
        
        char* current_pos = items_def;
        while (*current_pos && item_count < 100) {
            while (*current_pos && (*current_pos == ' ' || *current_pos == '\t' || *current_pos == '\n' || *current_pos == '\r')) {
                current_pos++;
            }
            
            if (!*current_pos) break;
            
            if (*current_pos == '"' || *current_pos == '\'') {
                char quote_char = *current_pos;
                current_pos++;
                
                char* item_start = current_pos;
                char* item_end = NULL;
                
                while (*current_pos && *current_pos != quote_char) {
                    current_pos++;
                }
                
                if (*current_pos == quote_char) {
                    item_end = current_pos;
                    *item_end = '\0';
                    current_pos++;
                    
                    items[item_count++] = strdup(item_start);
                }
            } else {
                char* item_start = current_pos;
                
                while (*current_pos && *current_pos != ' ' && *current_pos != '\t' && *current_pos != '\n' && *current_pos != '\r') {
                    current_pos++;
                }
                
                char saved_char = *current_pos;
                *current_pos = '\0';
                
                items[item_count++] = strdup(item_start);
                
                *current_pos = saved_char;
                if (saved_char) current_pos++;
            }
        }
        
        char count_str[16];
        snprintf(count_str, sizeof(count_str), "%d", item_count);
        add_template_var(data, "_items_count", count_str);
        
        for (int i = 0; i < item_count; i++) {
            char key[32];
            snprintf(key, sizeof(key), "_item_%d", i);
            if (items[i]) {
                add_template_var(data, key, items[i]);
                free(items[i]);
            }
        }
        
        free(items_def);
        pos = end_pos + strlen(comment_end);
    }
    
    return data;
}

char* process_template_auto(const char* template, const char** prog_keys, const char** prog_values, int prog_pairs) {
    if (!template) return NULL;
    
    template_data_t* inline_data = parse_template_variables(template);
    if (!inline_data) {
        return process_template(template, prog_keys, prog_values, prog_pairs, 
                               prog_pairs > 0 ? "item" : NULL, NULL, 0);
    }
    
    int total_pairs = inline_data->count + prog_pairs;
    char** combined_keys = malloc(total_pairs * sizeof(char*));
    char** combined_values = malloc(total_pairs * sizeof(char*));
    
    if (!combined_keys || !combined_values) {
        if (combined_keys) free(combined_keys);
        if (combined_values) free(combined_values);
        free_template_data(inline_data);
        perror("Failed to allocate memory for combined template variables");
        return process_template(template, prog_keys, prog_values, prog_pairs, 
                               prog_pairs > 0 ? "item" : NULL, NULL, 0);
    }
    
    for (int i = 0; i < inline_data->count; i++) {
        combined_keys[i] = inline_data->keys[i];
        combined_values[i] = inline_data->values[i];
    }
    
    int adjusted_total_pairs = inline_data->count;
    for (int i = 0; i < prog_pairs; i++) {
        bool key_exists = false;
        for (int j = 0; j < inline_data->count; j++) {
            if (strcmp(inline_data->keys[j], prog_keys[i]) == 0) {
                combined_values[j] = (char*)prog_values[i];
                key_exists = true;
                break;
            }
        }
        
        if (!key_exists) {
            combined_keys[adjusted_total_pairs] = (char*)prog_keys[i];
            combined_values[adjusted_total_pairs] = (char*)prog_values[i];
            adjusted_total_pairs++;
        }
    }
    
    total_pairs = adjusted_total_pairs;
    
    const char* loop_key = "item";
    const char** loop_values = NULL;
    int loop_count = 0;
    
    for (int i = 0; i < inline_data->count; i++) {
        if (strcmp(inline_data->keys[i], "_items_count") == 0) {
            loop_count = atoi(inline_data->values[i]);
            
            if (loop_count > 0) {
                loop_values = malloc(loop_count * sizeof(char*));
                if (loop_values) {
                    for (int j = 0; j < loop_count; j++) {
                        char item_key[32];
                        snprintf(item_key, sizeof(item_key), "_item_%d", j);
                        
                        for (int k = 0; k < inline_data->count; k++) {
                            if (strcmp(inline_data->keys[k], item_key) == 0) {
                                loop_values[j] = inline_data->values[k];
                                break;
                            }
                        }
                    }
                }
            }
            break;
        }
    }
    
    char* result = process_template(template, (const char**)combined_keys, 
                                  (const char**)combined_values, total_pairs,
                                  loop_key, loop_values, loop_count);
    
    free(combined_keys);
    free(combined_values);
    if (loop_values) free(loop_values);
    
    free(inline_data->keys);
    free(inline_data->values);
    free(inline_data);

    return result;
}