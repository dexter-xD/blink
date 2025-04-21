#include "html_serve.h"

char* serve_html(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file");
        fclose(file);                         
        return NULL;
    }

    long length = ftell(file);
    if (length == -1) {
        perror("Error getting file size"); 
        fclose(file);                      
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        perror("Error seeking to beginning of file"); 
        fclose(file);                                
        return NULL;
    }

    char* buffer = malloc(length + 1);
    if (!buffer) {
        perror("Error allocating memory"); 
        fclose(file);                    
        return NULL;
    }

    if (fread(buffer, 1, length, file) != length) {
        perror("Error reading file");                  
        free(buffer);                                 
        fclose(file);                                 
        return NULL;
    }

    buffer[length] = '\0'; 
    fclose(file);         
    return buffer;         
}

char* inject_hot_reload_js(char* html_content) {
    if (!html_content) {
        fprintf(stderr, "Error: Null HTML content passed to inject_hot_reload_js\n");
        return NULL;
    }
    
    const char* hot_reload_js = 
        "<script>\n"
        "// Hot reload WebSocket connection\n"
        "(function() {\n"
        "    let socket;\n"
        "    let reconnectInterval;\n"
        "    let reconnectAttempts = 0;\n"
        "    let isReloading = false;\n"
        "    const maxReconnectAttempts = 10;\n"
        "    \n"
        "    function connect() {\n"
        "        if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) {\n"
        "            console.log('Hot reload: WebSocket already connected or connecting');\n"
        "            return;\n"
        "        }\n"
        "        \n"
        "        clearInterval(reconnectInterval);\n"
        "        \n"
        "        try {\n"
        "            const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';\n"
        "            const host = window.location.host || (window.location.hostname + ':8080');\n"
        "            const wsUrl = protocol + host + '/ws';\n"
        "            \n"
        "            console.log('Hot reload: Connecting to WebSocket at', wsUrl);\n"
        "            socket = new WebSocket(wsUrl);\n"
        "            \n"
        "            socket.onopen = function() {\n"
        "                console.log('Hot reload: WebSocket connected');\n"
        "                reconnectAttempts = 0;\n"
        "                clearInterval(reconnectInterval);\n"
        "            };\n"
        "            \n"
        "            socket.onclose = function(event) {\n"
        "                console.log('Hot reload: WebSocket disconnected', event.code, event.reason);\n"
        "                \n"
        "                if (isReloading) return;\n"
        "                \n"
        "                if (reconnectAttempts < maxReconnectAttempts) {\n"
        "                    const delay = Math.min(1000 * Math.pow(1.5, reconnectAttempts), 10000);\n"
        "                    console.log('Hot reload: Reconnecting in', delay, 'ms');\n"
        "                    \n"
        "                    clearInterval(reconnectInterval);\n"
        "                    reconnectInterval = setInterval(function() {\n"
        "                        reconnectAttempts++;\n"
        "                        connect();\n"
        "                    }, delay);\n"
        "                } else {\n"
        "                    console.log('Hot reload: Maximum reconnect attempts reached');\n"
        "                }\n"
        "            };\n"
        "            \n"
        "            socket.onmessage = function(event) {\n"
        "                console.log('Hot reload: Received message', event.data);\n"
        "                if (event.data === 'reload' && !isReloading) {\n"
        "                    isReloading = true;\n"
        "                    console.log('Hot reload: Reloading page');\n"
        "                    \n"
        "                    // Add a slight delay to ensure server has processed file changes\n"
        "                    setTimeout(function() {\n"
        "                        // Close socket before reload\n"
        "                        if (socket && socket.readyState === WebSocket.OPEN) {\n"
        "                            socket.close();\n"
        "                        }\n"
        "                        \n"
        "                        // Force a cache-busting reload\n"
        "                        window.location.reload(true);\n"
        "                    }, 100);\n"
        "                }\n"
        "            };\n"
        "            \n"
        "            socket.onerror = function(error) {\n"
        "                console.error('Hot reload: WebSocket error', error);\n"
        "            };\n"
        "        } catch (err) {\n"
        "            console.error('Hot reload: Error creating WebSocket', err);\n"
        "        }\n"
        "    }\n"
        "    \n"
        "    // On page load, try to connect\n"
        "    if (document.readyState === 'complete' || document.readyState === 'interactive') {\n"
        "        connect();\n"
        "    } else {\n"
        "        window.addEventListener('load', connect);\n"
        "    }\n"
        "    \n"
        "    // Also reconnect when page becomes visible again\n"
        "    document.addEventListener('visibilitychange', function() {\n"
        "        if (!document.hidden && (!socket || socket.readyState !== WebSocket.OPEN)) {\n"
        "            connect();\n"
        "        }\n"
        "    });\n"
        "})();\n"
        "</script>\n";
    
    char* body_close_tag = strstr(html_content, "</body>");
    if (!body_close_tag) {
        printf("No </body> tag found in HTML content, not injecting hot reload script\n");
        return html_content;
    }
    size_t original_len = strlen(html_content);
    size_t script_len = strlen(hot_reload_js);
    size_t offset = body_close_tag - html_content;
    char* new_html = malloc(original_len + script_len + 1);
    if (!new_html) {
        perror("Memory allocation failed for hot reload script injection");
        return html_content;
    }
    
    strncpy(new_html, html_content, offset);
    new_html[offset] = '\0';
    strcat(new_html, hot_reload_js);
    strcat(new_html, body_close_tag);
    free(html_content);
    return new_html;
}
