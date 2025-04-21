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
        "// Hot Reload Script\n"
        "(function() {\n"
        "    function connectWebSocket() {\n"
        "        console.log('Hot reload: Connecting WebSocket...');\n"
        "        \n"
        "        // Create WebSocket connection\n"
        "        const protocol = window.location.protocol === 'https:' ? 'wss://' : 'ws://';\n"
        "        const host = window.location.hostname + ':' + (window.location.port || '8080');\n"
        "        const ws = new WebSocket(protocol + host + '/ws');\n"
        "        \n"
        "        // Connection opened\n"
        "        ws.addEventListener('open', (event) => {\n"
        "            console.log('Hot reload: WebSocket connected');\n"
        "            ws.send('ping');\n"
        "        });\n"
        "        \n"
        "        // Listen for messages\n"
        "        ws.addEventListener('message', (event) => {\n"
        "            console.log('Hot reload: Message from server:', event.data);\n"
        "            if (event.data === 'reload') {\n"
        "                console.log('Hot reload: Reloading page...');\n"
        "                window.location.reload();\n"
        "            }\n"
        "        });\n"
        "        \n"
        "        // Connection closed or error\n"
        "        ws.addEventListener('close', (event) => {\n"
        "            console.log('Hot reload: Connection closed. Reconnecting in 2 seconds...');\n"
        "            setTimeout(connectWebSocket, 2000);\n"
        "        });\n"
        "        \n"
        "        ws.addEventListener('error', (error) => {\n"
        "            console.error('Hot reload: WebSocket error', error);\n"
        "        });\n"
        "        \n"
        "        // Keep connection alive\n"
        "        setInterval(() => {\n"
        "            if (ws.readyState === WebSocket.OPEN) {\n"
        "                ws.send('ping');\n"
        "            }\n"
        "        }, 30000);\n"
        "        \n"
        "        return ws;\n"
        "    }\n"
        "    \n"
        "    // Start connection when page is loaded\n"
        "    if (document.readyState === 'complete') {\n"
        "        connectWebSocket();\n"
        "    } else {\n"
        "        window.addEventListener('load', connectWebSocket);\n"
        "    }\n"
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
