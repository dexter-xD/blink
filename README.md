# Blink: Lightweight Web Server with Advanced Templating

Blink is a lightweight, powerful web server written in C that features a comprehensive templating system with support for dynamic content, conditional logic, loops, and SQLite database integration. It's designed to be fast, easy to use, and perfect for both development and small-scale deployments.

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support-yellow.svg)](https://buymeacoffee.com/trish07)

## Features

- **Lightweight HTTP Server**: Fast and efficient C-based HTTP server with minimal dependencies
- **Hot Reloading**: Automatic browser refresh when HTML files are modified
- **WebSocket Support**: Real-time bidirectional communication
- **Comprehensive Templating System**:
  - Variable replacement
  - Conditional logic (if/else blocks)
  - Loops with item iteration
  - Conditional loops with filtering
  - Nested template structures
- **SQLite Integration**:
  - Execute SQL queries directly in templates
  - Display query results as formatted HTML tables
  - Form-based database operations (Create, Read, Update, Delete)
  - Placeholder substitution for safe user input handling
- **Flexible Configuration**:
  - Customizable port settings
  - Custom HTML file serving
  - Database path configuration
  - Template processing toggles

## Project Structure

```
blink/
├── CMakeLists.txt             # Build configuration
├── LICENSE                    # Project license
├── README.md                  # Project documentation
├── .gitignore                 # Git ignore file
│
├── include/                   # Header files
│   ├── blink_orm.h            # ORM functionality for SQLite
│   ├── debug.h                # Debugging utilities
│   ├── file_watcher.h         # File watching for hot reload
│   ├── html_serve.h           # HTML serving functionality
│   ├── request_handler.h      # HTTP request handler
│   ├── server.h               # Main server header
│   ├── socket_utils.h         # Socket utilities
│   ├── sqlite_handler.h       # SQLite database integration
│   ├── template.h             # Template processing
│   └── websocket.h            # WebSocket protocol support
│
├── src/                       # Source code files
│   ├── file_watcher.c         # Implementation of file watcher
│   ├── handle_client.c        # Client connection handler
│   ├── html_serve.c           # HTML content serving
│   ├── request_handler.c      # HTTP request processing
│   ├── server.c               # Main server implementation
│   ├── socket_utils.c         # Socket utility functions
│   ├── sqlite_handler.c       # SQLite database functions
│   ├── template.c             # Template engine implementation
│   └── websocket.c            # WebSocket implementation
│
└── build/                     # Build directory (generated)
    └── bin/                   # Compiled binaries
        └── blink              # Main executable
```

## Quick Start

### Prerequisites

- **CMake** (version 3.10 or higher)
- **GCC** or another compatible C compiler
- **OpenSSL** development libraries
- **SQLite3** development libraries
- **Linux** or **WSL** (Windows Subsystem for Linux) recommended

### Installation

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt update
sudo apt install build-essential cmake libssl-dev libsqlite3-dev

# Clone the repository
git clone https://github.com/dexter-xD/blink.git
cd blink

# Build the project
mkdir build && cd build
cmake ..
make

# Run the server
./bin/blink
```

### Command-Line Options

```
Options:
  -p, --port PORT      Specify port number (default: 8080)
  -s, --serve FILE     Specify a custom HTML file to serve
  -db, --database FILE Specify SQLite database path
  -n, --no-templates   Disable template processing
  -h, --help           Display help message
```

Example usage:

```bash
# Run with a custom HTML file and SQLite database
./bin/blink --serve myapp.html --database mydata.db --port 9000
```

## Template Engine Guide

The Blink template engine allows dynamic HTML generation with various powerful features. Here's an overview of the main capabilities:

### 1. Variable Replacement

Define variables using HTML comments and reference them with double curly braces:

```html
<!-- template:var username="John" company="Acme Corp" -->

<h1>Welcome, {{username}}!</h1>
<p>You are logged into {{company}} systems.</p>
```

### 2. Conditional Logic

Use if-else blocks to display content conditionally:

```html
<!-- template:var is_admin="1" -->

{% if is_admin %}
  <div class="admin-panel">
    <h2>Admin Controls</h2>
    <!-- Admin content here -->
  </div>
{% else %}
  <p>You don't have admin privileges.</p>
{% endif %}
```

### 3. Loops

Iterate over items using for loops:

```html
<!-- template:items "Apple" "Banana" "Cherry" "Date" -->

<ul>
  {% for item in items %}
    <li>{{item}}</li>
  {% endfor %}
</ul>
```

### 4. Multi-part Items

Use pipe-delimited values for structured data:

```html
<!-- template:items "Apple|Fruit|0.99" "Carrot|Vegetable|0.50" "Bread|Bakery|2.49" -->

<table>
  <tr><th>Product</th><th>Category</th><th>Price</th></tr>
  {% for item in items %}
    <tr>
      <td>{{item.0}}</td>
      <td>{{item.1}}</td>
      <td>${{item.2}}</td>
    </tr>
  {% endfor %}
</table>
```

### 5. Conditional Loops

Filter items in loops using conditions:

```html
<!-- template:items "Apple|Fruit|0.99" "Carrot|Vegetable|0.50" "Banana|Fruit|0.75" -->

<h2>Fruits Only:</h2>
<ul>
  {% for item in items if item.1 == "Fruit" %}
    <li>{{item.0}} - ${{item.2}}</li>
  {% endfor %}
</ul>
```

### 6. SQLite Integration

Execute SQL queries directly in your templates:

```html
<h2>User List</h2>
{% query "SELECT id, name, email FROM users ORDER BY name LIMIT 10" %}

<h2>Item Statistics</h2>
{% query "SELECT category, COUNT(*) as count, AVG(price) as avg_price FROM products GROUP BY category" %}
```

### 7. Form-Based Database Operations

Create forms that perform database operations:

```html
<form action="/sql" method="POST">
  <input type="hidden" name="sql_action" value="insert">
  
  <label for="name">Name:</label>
  <input type="text" id="name" name="name" required>
  
  <label for="email">Email:</label>
  <input type="email" id="email" name="email">
  
  <input type="hidden" name="sql_query" value="INSERT INTO users (name, email) VALUES ('[name]', '[email]')">
  
  <button type="submit">Add User</button>
</form>
```

For detailed documentation on all template features, see the [Template Documentation](./docs/Template.md).

## Example Applications

### 1. Simple Dynamic Page

```html
<!-- template:var page_title="Welcome Page" username="Guest" -->

<!DOCTYPE html>
<html>
<head>
  <title>{{page_title}}</title>
</head>
<body>
  <h1>Hello, {{username}}!</h1>
  <p>Welcome to our website.</p>
</body>
</html>
```

### 2. Data Dashboard with SQLite

```html
<!DOCTYPE html>
<html>
<head>
  <title>Sales Dashboard</title>
</head>
<body>
  <h1>Sales Dashboard</h1>
  
  <h2>Recent Orders</h2>
  {% query "SELECT id, customer_name, amount, date FROM orders ORDER BY date DESC LIMIT 5" %}
  
  <h2>Sales by Category</h2>
  {% query "SELECT category, SUM(amount) as total FROM orders GROUP BY category ORDER BY total DESC" %}
</body>
</html>
```

## Hot Reload Feature

Blink's hot reload feature automatically refreshes connected browsers when HTML files are modified:

1. Start the server with default options
2. Edit any HTML file in your project directory
3. The browser will automatically refresh to show your changes

This feature works by:
- Watching file system events in the HTML directory
- Using WebSockets to notify connected clients
- Injecting a small JavaScript snippet into served HTML pages

## WebSocket Support

Blink includes WebSocket support for real-time bidirectional communication:

1. Access WebSocket functionality at `/ws` endpoint
2. Establish a WebSocket connection from your client-side JavaScript
3. Exchange messages between client and server in real time

## SQLite Database Integration

To use SQLite features:

1. Start Blink with a database: `./bin/blink --database mydata.db`
2. Use `{% query "SQL_STATEMENT" %}` tags in your HTML templates
3. Create forms with action="/sql" to perform database operations

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Support Development

If you find Blink useful, consider supporting its development:

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support-yellow.svg)](https://www.buymeacoffee.com/yourusername)

## License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## Template Engine Documentation

For full documentation on the template engine, see [docs/Template.md](./docs/Template.md).