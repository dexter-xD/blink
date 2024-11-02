#ifndef TEMPLATE_H
#define TEMPLATE_H

#include <string.h>    
#include <stdlib.h>    
#include <stddef.h>    
#include <stdio.h> 

char* replace_placeholders(char* result, const char** keys, const char** values, int num_pairs);
char* process_if_else(char* result, const char** keys, const char** values, int num_pairs);
char* process_loops(char* result, const char* loop_key, const char** loop_values, int loop_count);
char* process_template(const char* template, const char** keys, const char** values, int num_pairs, const char* loop_key, const char** loop_values, int loop_count);

#endif
