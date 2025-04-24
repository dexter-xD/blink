#ifndef BLINK_ORM_H
#define BLINK_ORM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sqlite_handler.h"

// Mega awesome ORM macros with funny syntax
#define YO_TABLE(name) typedef struct name##_model name##_model; \
                      struct name##_model { \
                          int id; \
                          char* _table_name;

#define WASSUP(type, name) type name;

#define PEACE } 

#define FIRE_IN_THE_HOLE(name) void name##_init(name##_model* model) { \
                              model->_table_name = #name; \
                              model->id = 0; \
                           }

#define YEET(name) name##_model* name##_create() { \
                  name##_model* model = malloc(sizeof(name##_model)); \
                  if (!model) return NULL; \
                  name##_init(model); \
                  return model; \
               }

#define GET_LITTY(model, query) do { \
                            char query_str[1024]; \
                            snprintf(query_str, sizeof(query_str), "SELECT * FROM %s %s LIMIT 1", \
                                    model->_table_name, query); \
                            printf("🔥🔥🔥 LITTY QUERY: %s 🔥🔥🔥\n", query_str); \
                            sqlite_result_t* result = execute_query(query_str); \
                            if (result && result->row_count > 0) { \
                                _populate_model_from_result(model, result); \
                            } \
                            if (result) free_query_results(result); \
                        } while(0)

#define SEND_IT(model) do { \
                      if (model->id == 0) { \
                          _insert_model(model); \
                      } else { \
                          _update_model(model); \
                      } \
                  } while(0)

#define YOINK(model) do { \
                    if (model->id > 0) { \
                        char query_str[256]; \
                        snprintf(query_str, sizeof(query_str), "DELETE FROM %s WHERE id = %d", \
                                model->_table_name, model->id); \
                        printf("👋 YOINKING: %s\n", query_str); \
                        execute_query(query_str); \
                        model->id = 0; \
                    } \
                } while(0)

#define SQUAD_UP(name, query) name##_model** name##_find_all(const char* query, int* count) { \
                            char query_str[1024]; \
                            snprintf(query_str, sizeof(query_str), "SELECT * FROM %s %s", #name, query); \
                            printf("🔎 SQUAD UP QUERY: %s\n", query_str); \
                            sqlite_result_t* result = execute_query(query_str); \
                            name##_model** models = NULL; \
                            *count = 0; \
                            if (result) { \
                                *count = result->row_count; \
                                if (*count > 0) { \
                                    models = malloc(*count * sizeof(name##_model*)); \
                                    for (int i = 0; i < *count; i++) { \
                                        models[i] = name##_create(); \
                                        _populate_model_from_result_row(models[i], result, i); \
                                    } \
                                } \
                                free_query_results(result); \
                            } \
                            return models; \
                        }

// Helper function declarations
void _populate_model_from_result(void* model, sqlite_result_t* result);
void _populate_model_from_result_row(void* model, sqlite_result_t* result, int row);
void _insert_model(void* model);
void _update_model(void* model);

#endif /* BLINK_ORM_H */ 