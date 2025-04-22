# Blink: Lightweight Web Server with Templating

This project is a simple and fast web server written in C, using socket programming concepts. It listens on a specified port, accepts client connections, and serves dynamic HTML content based on incoming requests using a templating system that supports dynamic values, if-else conditions, and loops. The code is organized into multiple files with CMake as the build system.

## Features

- Fast and lightweight TCP socket server implementation
- HTML templating system with dynamic values, conditions, and loops
- **Hot Reloading**: Automatic browser refresh when HTML files are modified
- WebSocket implementation for real-time client-server communication
- Command-line options for configuring server behavior

## Project Structure

```
project/
├── CMakeLists.txt             # Build configuration
├── include/
│   ├── server.h               # Main server header file
│   ├── html_serve.h           # Header for serve_html function
│   ├── request_handler.h      # Header for handle_client function
│   ├── template.h             # Header for template processing functions
│   ├── socket_utils.h         # Header for socket utility functions
│   ├── file_watcher.h         # Header for file watching functionality
│   └── websocket.h            # Header for WebSocket functionality
├── src/
│   ├── server.c               # Main server program
│   ├── html_serve.c           # serve_html function
│   ├── request_handler.c      # handle_client function
│   ├── template.c             # Template processing functions
│   ├── socket_utils.c         # Utility functions for socket operations
│   ├── file_watcher.c         # File watcher implementation
│   └── websocket.c            # WebSocket implementation
├── docs/
│   └── Template.md            # Detailed templating system documentation
├── template-examples/
│   ├── 01-variables.html      # Basic variable replacement examples
│   ├── 02-conditionals.html   # Conditional logic examples
│   ├── 03-loops.html          # Loop examples
│   └── 04-conditional-loops.html # Combined condition and loop examples
└── README.md                  # Project documentation
```

### Files 
- **`server.c`**: Contains the main server loop, which listens for and accepts client connections.
- **`html_serve.c`**: Handles HTML file reading and serves HTML content to clients. Contains the `serve_html` function.
- **`request_handler.c`**: Manages client requests by processing incoming HTTP requests and serving appropriate responses. Contains the `handle_client` function.
- **`template.c`**: Implements template processing functions to support dynamic values, if-else conditions, and loops in HTML content.
- **`socket_utils.c`**: Includes utility functions for socket initialization and client data handling. Contains the `initialize_server` and `read_client_data` functions.
- **`file_watcher.c`**: Implements the file watcher functionality for hot reloading.
- **`websocket.c`**: Implements WebSocket protocol for real-time client-server communication.

## Prerequisites

- **CMake** (version 3.10 or higher)
- **GCC** or another compatible C compiler
- **OpenSSL** development libraries
- **Linux** or **WSL** (Windows Subsystem for Linux) recommended for running Blink

## Setup Instructions

### 1. Install Dependencies

```bash
# On Debian/Ubuntu
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

### 2. Clone the Repository

```bash
git clone <repository_url>
cd blink
```

### 3. Build the Project

1. Create a `build` directory and navigate into it:
   ```bash
   mkdir build && cd build
   ```
   
2. Generate the makefiles with CMake:
   ```bash
   cmake ..
   ```

3. Compile the project using `make`:
   ```bash
   make
   ```

This will create an executable file named `blink` inside a `bin` directory under the `build` folder.

### 4. Run the Server

After building, you can start Blink as follows:

```bash
./bin/blink
```

The server will listen on port `8080` (default setting) and process incoming client requests.

### Command-Line Options

Blink supports several command-line options to customize its behavior:

```
Options:
  -p, --port PORT      Specify port number (default: 8080)
  -s, --serve FILE     Specify a custom HTML file to serve
  -n, --no-templates   Disable template processing
  -h, --help           Display help message
```

Examples:

```bash
# Run on a different port
./bin/blink --port 8081

# Serve a specific HTML file
./bin/blink --serve my-custom-page.html

# Disable template processing
./bin/blink --no-templates

# Run with multiple options
./bin/blink --port 9000 --no-templates
```

## Hot Reload Feature

Blink includes a hot reload feature that automatically refreshes connected browsers when HTML files are modified:

1. The server watches the `html` directory for any changes to HTML files
2. When a file is modified, the server notifies all connected clients via WebSockets
3. The client-side JavaScript (injected into each HTML page) receives the notification and reloads the page

This allows for a smoother development experience as you can see your changes immediately without manually refreshing the browser.

## Template Features

Blink supports a templating system that allows for dynamic content in HTML files. For detailed documentation on the templating system, refer to the [Template Documentation](./docs/Template.md).

The template-examples directory contains working examples of each template feature:

- **01-variables.html**: Shows basic variable replacement
- **02-conditionals.html**: Demonstrates if-else conditional blocks
- **03-loops.html**: Examples of using loops to iterate over items
- **04-conditional-loops.html**: Advanced examples combining conditions and loops

### 1. Dynamic Values

You can replace placeholders in the template with dynamic values. Placeholders are defined as `{{key}}`, where `key` corresponds to the variable you want to insert.

### 2. If-Else Conditions

Blink can handle if-else statements within templates. Use the following syntax:

```
{% if condition %}
    Content to show if condition is true
{% else %}
    Content to show if condition is false
{% endif %}
```

### 3. For Loops

You can create loops to iterate over a list of items in your templates using the following syntax:

```
{% for item in items %}
    Content to repeat for each item
{% endfor %}
```

## Example Usage

### HTML Template

Create an HTML file (e.g., `index.html`) with the following content to test the features:

```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Blink Template Example</title>
</head>
<body>
    <h1>Welcome to Blink Server</h1>
    
    <p>{{ greeting }}</p>

    {% if is_logged_in %}
        <p>Hello, {{ username }}!</p>
    {% else %}
        <p>Please log in to access more features.</p>
    {% endif %}

    <h2>Your Items:</h2>
    <ul>
        {% for item in items %}
            <li>{{ item }}</li>
        {% endfor %}
    </ul>
</body>
</html>
```

### Sending a Request

To send a request to Blink, you can use a web browser or a tool like `curl`. Make sure to specify the correct HTML file to serve based on your request.

## Troubleshooting

If you encounter errors:
1. Ensure that no other process is using port `8080`.
2. Verify that CMake, GCC, and OpenSSL are correctly installed.
3. Use the command `netstat -tuln | grep 8080` to check if the port is occupied.

## Acknowledgments

- [CMake](https://cmake.org/) - Cross-platform build system
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) - A great resource for learning socket programming
- [RFC 6455](https://tools.ietf.org/html/rfc6455) - The WebSocket Protocol

---

Feel free to reach out for any questions or improvements!