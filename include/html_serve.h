#ifndef HTML_SERVE_H
#define HTML_SERVE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* serve_html(const char* filename);
char* inject_hot_reload_js(char* html_content);

#endif
