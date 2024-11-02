#include "template.h"

// Function to replace placeholders in the template
char* replace_placeholders(char* result, const char** keys, const char** values, int num_pairs) {
    for (int i = 0; i < num_pairs; i++) {
        char placeholder[64];
        snprintf(placeholder, sizeof(placeholder), "{{%s}}", keys[i]);

        char* position;
        while ((position = strstr(result, placeholder)) != NULL) {
            size_t len_before = position - result;
            size_t len_key = strlen(placeholder);
            size_t len_value = strlen(values[i]);

            // Allocate new result memory
            char* new_result = malloc(len_before + len_value + strlen(position + len_key) + 1);
            if (!new_result) {
                perror("Memory allocation failed");
                return NULL;
            }

            // Copy parts into the new result
            strncpy(new_result, result, len_before);
            strcpy(new_result + len_before, values[i]);
            strcpy(new_result + len_before + len_value, position + len_key);

            free(result); // Free old result
            result = new_result; // Point to new result
        }
    }
    return result;
}

// Function to handle if-else conditions in the template
char* process_if_else(char* result, const char** keys, const char** values, int num_pairs) {
    for (int i = 0; i < num_pairs; i++) {
        char condition[64];
        snprintf(condition, sizeof(condition), "{%% if %s %%}", keys[i]);

        char* if_position;
        while ((if_position = strstr(result, condition)) != NULL) {
            char* else_position = strstr(if_position, "{% else %}");
            char* endif_position = strstr(if_position, "{% endif %}");
            if (else_position && endif_position) {
                char* block_start = if_position + strlen(condition);
                char* block_end = else_position;

                char* content_if = strndup(block_start, block_end - block_start);
                char* content_else = strndup(else_position + strlen("{% else %}"), endif_position - (else_position + strlen("{% else %}")));

                if (!content_if || !content_else) {
                    perror("Memory allocation failed for if-else content");
                    free(content_if);
                    free(content_else);
                    free(result);
                    return NULL;
                }

                char* replacement_content = strlen(values[i]) > 0 ? content_if : content_else;

                size_t len_before = if_position - result;
                size_t len_after = strlen(endif_position + strlen("{% endif %}"));
                char* new_result = malloc(len_before + strlen(replacement_content) + len_after + 1);
                if (!new_result) {
                    perror("Memory allocation failed for new result");
                    free(content_if);
                    free(content_else);
                    free(result);
                    return NULL;
                }

                // Copy the new result
                strncpy(new_result, result, len_before);
                strcpy(new_result + len_before, replacement_content);
                strcpy(new_result + len_before + strlen(replacement_content), endif_position + strlen("{% endif %}"));

                free(result); // Free old result
                result = new_result; // Point to new result

                free(content_if); // Free content if
                free(content_else); // Free content else
            }
            else {
                break; // If the end of the block isn't found
            }
        }
    }
    return result;
}

// Function to process loops in the template
char* process_loops(char* result, const char* loop_key, const char** loop_values, int loop_count) {
    char loop_start[64];
    snprintf(loop_start, sizeof(loop_start), "{%% for %s in items %%}", loop_key);
    char* loop_position;

    while ((loop_position = strstr(result, loop_start)) != NULL) {
        char* end_loop_position = strstr(loop_position, "{% endfor %}");
        if (!end_loop_position) break;

        size_t len_before = loop_position - result;
        size_t len_after = strlen(end_loop_position + strlen("{% endfor %}"));
        size_t loop_content_length = end_loop_position - (loop_position + strlen(loop_start));

        // Prepare to create the new result string with loop content
        char* loop_content = strndup(loop_position + strlen(loop_start), loop_content_length);
        if (!loop_content) {
            perror("Memory allocation failed for loop content");
            free(result);
            return NULL;
        }

        // Allocate memory for the final result
        char* new_result = malloc(len_before + (loop_content_length + strlen(loop_content)) * loop_count + len_after + 1);
        if (!new_result) {
            perror("Memory allocation failed for new result");
            free(loop_content);
            free(result);
            return NULL;
        }

        // Copy the content before the loop
        strncpy(new_result, result, len_before);

        // Process loop values
        for (int i = 0; i < loop_count; i++) {
            // Replace the placeholder with the current loop value
            char item_placeholder[64];
            snprintf(item_placeholder, sizeof(item_placeholder), "{{%s}}", loop_key);
            char* loop_result = replace_placeholders(loop_content, (const char**)&loop_key, (const char**)&loop_values[i], 1);
            if (loop_result) {
                strcpy(new_result + len_before, loop_result);
                len_before += strlen(loop_result);
                free(loop_result);
            }
        }

        // Copy the content after the loop
        strcpy(new_result + len_before, end_loop_position + strlen("{% endfor %}"));

        free(loop_content); // Free loop content
        free(result); // Free old result
        result = new_result; // Point to new result
    }
    return result;
}

// Main template processing function
char* process_template(const char* template, const char** keys, const char** values, int num_pairs, const char* loop_key, const char** loop_values, int loop_count) {
    char* result = strdup(template);  // Make a copy of the template
    if (!result) {
        perror("Failed to allocate memory for template processing");
        return NULL;
    }

    // Replace placeholders
    result = replace_placeholders(result, keys, values, num_pairs);
    // Handle if-else conditions
    result = process_if_else(result, keys, values, num_pairs);
    // Process loops
    result = process_loops(result, loop_key, loop_values, loop_count);

    return result;
}
