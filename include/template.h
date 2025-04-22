#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <string.h>    
#include <stdlib.h>    
#include <stddef.h>    
#include <stdio.h> 
#include <stdbool.h>

typedef struct {
    char** keys;
    char** values;
    int count;
    int capacity;
} template_data_t;

char* replace_placeholders(char* result, const char** keys, const char** values, int num_pairs);
char* process_if_else(char* result, const char** keys, const char** values, int num_pairs);
char* process_loops(char* result, const char* loop_key, const char** loop_values, int loop_count);
char* process_template(const char* template, const char** keys, const char** values, int num_pairs, const char* loop_key, const char** loop_values, int loop_count);
char* process_template_auto(const char* template, const char** prog_keys, const char** prog_values, int prog_pairs);
char* get_item_part(const char* item, char delimiter, int part_index);


template_data_t* init_template_data(void);
void add_template_var(template_data_t* data, const char* key, const char* value);
template_data_t* parse_template_variables(const char* template_str);
void free_template_data(template_data_t* data);

#endif
